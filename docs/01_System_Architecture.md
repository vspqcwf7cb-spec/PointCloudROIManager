# 01 — 系统架构设计

- 版本：v1.0
- 模块：PointCloudROIManager

---

## 1. 模块目标

定义 PointCloudROIManager 在 VisionAlgorithmPlatform 中的系统架构、类关系、依赖方向与 Pipeline 交互方式，保证后续 03–08 模块设计有唯一的结构依据。

## 2. 工业应用场景

- 整车扫描后，按车门 / 天窗 / 电池包等位置切出胶道局部点云
- 焊缝检测时，按焊缝路径切出条状 ROI
- 零件尺寸检测时，按关键特征点切出测量区域

## 3. 架构位置

```
VisionAlgorithmPlatform
    └── PipelineEngine
            └── Operators
                    ├── PointCloudLoaderOperator
                    ├── PointCloudROIManagerOperator     ← 本模块对外暴露
                    ├── PointCloudPreprocessOperator
                    └── ...
```

内部：

```
PointCloudROIManagerOperator
        │ 持有
        ▼
PointCloudROIManager（核心调度）
        ├── ROIConfigManager
        ├── ROIParser            ──▶ nlohmann/json
        ├── ROIGeometry          ──▶ Eigen::AlignedBox3f
        ├── ROIExtractor          ──▶ PCL Octree
        └── ROIResultManager
```

## 4. 输入输出定义

| 方向 | 类型 | 说明 |
| --- | --- | --- |
| 输入 | `PointCloudData` | 整车扫描点云，含 coordinate / timestamp |
| 输入 | `std::string configPath` | ROI 配置文件路径 |
| 输出 | `ROIManagerResult` | 全部 ROI 结果 + 整批状态 |

## 5. 数据结构（概要）

详见 `02_Data_Structure_Design.md`。本架构仅引用名称：

- `PointXYZI`
- `PointCloudData`
- `ROIType`（enum class）
- `ROIRegion`
- `ROIConfig`
- `ROIResult`
- `ROIManagerResult`

## 6. 类关系

```
PointCloudROIManagerOperator   ──owns──▶ PointCloudROIManager
                                              │
                              ┌───────────────┼────────────────┐
                              ▼               ▼                ▼
                       ROIConfigManager  ROIParser        ROIExtractor
                              │               │                │
                              └──────┬────────┘                │
                                     ▼                         ▼
                                 ROIRegion              ROIResultManager
```

依赖方向规则（强制）：

- 上层 → 下层 单向依赖
- 禁止 ROIExtractor 反向调用 ROIParser
- 禁止 Manager 直接 `#include <pcl/octree/octree_search.h>`（由具体 Extractor 持有）
- 公共头仅暴露 `PointCloudData` / `ROIConfig` / `ROIManagerResult`，不暴露 PCL 类型

## 7. 接口设计（核心类骨架）

```cpp
// PointCloudROIManager.h
class PointCloudROIManager {
public:
    PointCloudROIManager();
    ~PointCloudROIManager();

    bool init(const std::string& configPath);
    bool execute(const PointCloudData& inputCloud);
    const ROIManagerResult& getResult() const;

private:
    std::unique_ptr<ROIConfigManager>  configManager_;
    std::unique_ptr<ROIParser>         parser_;
    std::unique_ptr<ROIExtractor>      extractor_;
    std::unique_ptr<ROIResultManager>  resultManager_;
};
```

## 8. 算法流程

```
init(configPath)
    → configManager_->load(configPath)
    → parser_->parse(configPath) → ROIConfig
    → configManager_->validate(ROIConfig)

execute(inputCloud)
    → extractor_->build(inputCloud)
    → for each ROIRegion:
          extractor_->extract(region) → ROIResult
    → resultManager_->collect(all ROIResult)
    → assemble ROIManagerResult

getResult()
    → return resultManager_->snapshot()
```

## 9. 异常处理

- 配置文件不存在 / JSON 语法错：`init` 返回 false，`ROIManagerResult.error` 写明文件名与行号
- 输入点云为空：`execute` 返回 false，不执行任何裁剪
- 单个 ROI 越界（命中 0 点）：不抛异常，对应 `ROIResult.success=false, message="no hit"`
- Octree 构建失败：整批失败，`ROIManagerResult.success=false`

## 10. 测试方案

- 类协作测试：使用 mock `ROIExtractor` 验证 Manager 调度顺序
- 依赖方向测试：头文件包含图检查（include-what-you-use），确认公共头不泄漏 PCL
- 性能测试：100 万点 / 10 ROI / < 100 ms

## 11. AI 实现约束

- 头文件中禁止出现 `pcl::` 命名空间
- 所有跨模块对象用 `std::shared_ptr<PointCloudData>` 传递，避免拷贝
- 类成员变量全部私有，禁止 public 成员变量
- `init` 必须幂等：重复调用不产生资源泄漏

## 12. 后续扩展

- 新增 ROI 类型：扩展 `ROIGeometry` 与 `ROIExtractor` 派生类，Manager 不改动
- 接入远程配置：`ROIConfigManager` 增加从 HTTP / 共享盘加载的实现
- 多线程裁剪：`ROIExtractor::extract` 支持 ROI 间并行（每个 ROI 独立查询 Octree，无写冲突）
