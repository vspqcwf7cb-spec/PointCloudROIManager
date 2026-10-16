# 07 — PointCloudROIManager 核心模块

- 版本：v1.0
- 对应实现：`include/PointCloudROIManager.h` + `src/PointCloudROIManager.cpp`

---

## 1. 模块目标

作为对外门面（Facade），把 ROIConfigManager / ROIParser / ROIExtractor / ROIResultManager 串成一次完整的"输入整车点云 → 输出全部 ROI 点云"调用。**Manager 不做具体算法**，只做调度、状态机与错误聚合。

## 2. 工业应用场景

- 产线每个扫描周期调用一次 `execute`，得到全部 ROI 结果
- 现场调试时 `init` 一次，多次 `execute`（同配置处理多帧点云）
- 故障时通过 `getResult().error` 直接定位到哪一步失败

## 3. 架构位置

```
PointCloudROIManagerOperator (08 模块)
            │ 持有
            ▼
       PointCloudROIManager            ← 本模块
            ├── owns ──▶ ROIConfigManager
            ├── owns ──▶ ROIExtractor（BoxROIExtractor）
            └── owns ──▶ ROIResultManager
```

## 4. 输入输出定义

| 方向 | 类型 | 说明 |
| --- | --- | --- |
| 输入 | `init(configPath)` | 配置路径 |
| 输入 | `execute(shared_ptr<const PointCloudData>)` | 整车点云 |
| 输出 | `bool` | init / execute 是否成功 |
| 输出 | `getResult()` | `const ROIManagerResult&` 整批结果 |

## 5. 数据结构

Manager 内部状态机：

```cpp
enum class ManagerState {
    Uninitialized,
    Ready,          // init 成功，等待 execute
    Executing,
    Done,
    Failed
};
```

## 6. 类设计

```cpp
class PointCloudROIManager {
public:
    PointCloudROIManager();
    ~PointCloudROIManager();

    // 生命周期
    bool init(const std::string& configPath);
    bool execute(std::shared_ptr<const PointCloudData> cloud);
    const ROIManagerResult& getResult() const;

    // 状态查询
    ManagerState getState() const;
    std::string getLastError() const;

private:
    std::unique_ptr<ROIConfigManager> configManager_;
    std::unique_ptr<ROIExtractor>     extractor_;
    std::unique_ptr<ROIResultManager> resultManager_;

    ManagerState state_ = ManagerState::Uninitialized;
    ROIManagerResult lastResult_;
};
```

## 7. 接口设计

| 接口 | 语义 |
| --- | --- |
| `init` | 加载配置、构建 Extractor，状态进入 Ready |
| `execute` | 构建 Octree + 全部 ROI 裁剪，状态进入 Done 或 Failed |
| `getResult` | 只读引用，禁止外部改写 |
| `getState` | 监控 / 日志用 |
| `getLastError` | 最近一次失败的人类可读原因 |

## 8. 算法流程

```
init(configPath)
  ├── configManager_->load(configPath)
  │     失败 → state=Failed, 写 lastError, return false
  ├── 从配置读 octreeResolutionMM（默认 5）
  ├── extractor_ = make_unique<BoxROIExtractor>(resolution)
  ├── state = Ready
  └── return true

execute(cloud)
  ├── 前置检查：state==Ready，cloud 非空，cloud->points 非空
  │     任一失败 → state=Failed, lastError, return false
  ├── 坐标系校验：
  │     ├── config.coordinate 非空 且 cloud.coordinate 非空 且 不一致 → state=Failed (2002)
  │     ├── 任一方为空 → 跳过校验，打 warn (RoiWarnCode::CoordinateSkipped)
  │     └── 一致 → 继续
  ├── state = Executing
  ├── extractor_->build(cloud)
  ├── results = extractor_->extractAll(config.regions)
  │     （每个 ROIResult.cloud 是 shared_ptr<const PointCloudData>，跨 DLL 共享）
  ├── resultManager_->collect(results)
  ├── lastResult_ = resultManager_->snapshot()
  ├── state = Done
  └── return true
```

## 9. 异常处理

- 任何子模块返回 false：Manager 立即 `state=Failed`，错误串向上聚合
- 子模块抛异常：catch 住，转成 `lastError = typeid(e).name() + e.what()`，不向上抛
- `execute` 被并发调用：内部用 `std::mutex` 串行化，防止 Octree 被多线程重 build
- 重复 `init`：先释放旧 Extractor 再重建，资源不泄漏

## 10. 测试方案

| 用例 | 期望 |
| --- | --- |
| init 合法配置 | state=Ready |
| init 非法路径 | state=Failed，lastError 含路径 |
| execute 空点云 | state=Failed，error 含 "empty" |
| 坐标系不一致 | state=Failed，error 含 coordinate |
| 正常 execute 10 ROI | state=Done，results.size()=10 |
| 单 ROI 0 命中 | results 中该 ROI success=true, pointCount=0，整批 success=true |
| 并发 execute 2 线程 | 串行执行，无崩溃，结果一致 |
| 重复 init | 不泄漏，第二次正常进入 Ready |

## 11. AI 实现要求

- 头文件中**禁止出现 PCL / JSON / 具体 Extractor 类型**
- 所有成员用 `std::unique_ptr` + 前置声明（`class ROIConfigManager;` 等），降低编译耦合
- `execute` 内不写 `std::cout`，日志由日志模块统一打
- 状态机迁移必须显式，不允许隐式跳转
- 函数行数控制在 60 行以内，超过就拆私有 helper

## 12. 后续扩展

- 增加 `initWithString(jsonText)`，避免每次都读盘
- 增加 ROI 子集执行：`execute(cloud, roiNameList)`，只跑指定 ROI
- 增加 ROI 缓存（同一帧点云重复 execute 不重 build）
- 接入后台线程池，支持异步 execute + future 返回
