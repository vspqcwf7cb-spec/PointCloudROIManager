# 08 — ROI Manager Operator 封装

- 版本：v1.0
- 对应实现：`include/PointCloudROIManagerOperator.h` + `src/PointCloudROIManagerOperator.cpp`

---

## 1. 模块目标

把 PointCloudROIManager 适配成 VisionAlgorithmPlatform 的标准 Operator 接口，使 Manager 可以被 PipelineEngine 动态调度、热加载、参数化配置。

## 2. 工业应用场景

- 产线平台通过统一接口 Init / Execute / Release 调度所有算法
- 不同工位只需换配置文件路径，Operator 二进制不变
- 平台做性能统计时，按 Operator 边界打点

## 3. 架构位置

```
PipelineEngine
    └── Loads Operator dynamic library (.dll/.so)
            │  calls via base interface
            ▼
        IOperator (platform base)
            ▲
            │ implements
        PointCloudROIManagerOperator    ← 本模块
            │ owns
            ▼
        PointCloudROIManager (07 模块)
```

## 4. 输入输出定义

Operator 不做类型转换，只转发：

| 平台接口 | 转发目标 |
| --- | --- |
| `init(opConfig)` | `PointCloudROIManager::init(configPath)` |
| `execute(input)` | 从 input bag 取 `PointCloudData`，调用 `execute` |
| `getOutput()` | 把 `ROIManagerResult` 写回 output bag |
| `release()` | 释放 Manager |

## 5. 数据结构

平台约定的 OpConfig（v1 字段）：

```cpp
struct OpConfig {
    std::string opName;          // "PointCloudROIManager"
    std::string configPath;       // roi_config.json 路径
    float octreeResolutionMM;    // 覆盖配置中的分辨率，可 0 表示用默认
};
```

## 6. 类设计

```cpp
class PointCloudROIManagerOperator : public IOperator {
public:
    PointCloudROIManagerOperator();
    ~PointCloudROIManagerOperator() override;

    bool init(const OpConfig& config) override;
    bool execute(const DataBag& input, DataBag& output) override;
    void release() override;

    std::string name() const override { return "PointCloudROIManager"; }
    ManagerState state() const override;

private:
    std::unique_ptr<PointCloudROIManager> manager_;
    OpConfig opConfig_;
};
```

`IOperator` / `DataBag` 是平台基类，由 VisionAlgorithmPlatform 提供，本模块只 include 平台头。

## 7. 接口设计

| 接口 | 平台契约 |
| --- | --- |
| `init` | 进程启动时调用一次，失败则平台不下发数据 |
| `execute` | 每帧调用，输入 `DataBag`，输出 `DataBag` |
| `release` | 进程退出 / 卸载时调用，必须幂等 |
| `name` | 平台日志 / 监控用 |

`DataBag` 约定键：

- 输入：`"input_cloud"` → `shared_ptr<const PointCloudData>`
- 输出：`"roi_result"` → `ROIManagerResult`
- 输出：`"roi_clouds"` → `vector<shared_ptr<PointCloudData>>`（按 ROI 顺序）

## 8. 算法流程

```
init(opConfig)
  ├── 校验 opConfig.configPath 非空
  ├── manager_ = make_unique<PointCloudROIManager>()
  ├── manager_->init(configPath)
  └── 失败 → release() + return false

execute(input, output)
  ├── input.get("input_cloud") → cloud
  ├── cloud 缺失 / 类型错 → output.setError("missing input_cloud"); return false
  ├── manager_->execute(cloud)
  ├── result = manager_->getResult()
  ├── output.set("roi_result", result)
  ├── output.set("roi_clouds", extractCloudPtrs(result))
  └── return result.success

release()
  ├── manager_.reset()
  └── 幂等，可重复调用
```

## 9. 异常处理

| 场景 | 行为 |
| --- | --- |
| 平台未传 input_cloud | 不抛，写 output error，返回 false |
| Manager 抛异常 | catch，output.setError(what())，return false |
| release 被多次调用 | `manager_.reset()` 天然幂等 |
| 动态库卸载时仍有 execute 在跑 | 平台层保证（本模块不实现同步原语） |

## 10. 测试方案

| 用例 | 期望 |
| --- | --- |
| init 缺 configPath | init 返回 false |
| execute 缺 input_cloud | execute 返回 false，output 含 error |
| 正常一帧 execute | output 含 roi_result 与 roi_clouds |
| release 后再 execute | 不崩溃，返回 false（manager 已空） |
| 动态库加载 / 卸载循环 100 次 | 无内存泄漏（VS Diagnostics / ASan） |

## 11. AI 实现要求

- 头文件 `#include "IOperator.h"` 只暴露平台类型，**不要 include 具体 Manager 头**（用前置声明）
- 导出宏 `SBT_OPS_API`（参考 `pointCloudCropProcess.h` 的 dllexport 范式）
- 禁止在 Operator 内做任何业务逻辑，所有判断委托 Manager
- 全局工厂函数必须导出：`extern "C" IOperator* CreateROIManagerOperator()`
- 配套导出 `extern "C" void DestroyOperator(IOperator*)`

## 12. 后续扩展

- 支持 OpConfig 热更新（平台通过 `reloadConfig` 下发新 JSON 路径）
- 支持指标上报：execute 平均耗时 / ROI 命中率，写入平台监控
- 增加异步执行模式（execute 立即返回 future）
