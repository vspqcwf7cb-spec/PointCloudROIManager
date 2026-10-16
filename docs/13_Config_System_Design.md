# 13 — 配置体系设计

- 版本：v1.0

---

## 1. 模块目标

定义 PointCloudROIManager 的**全部**配置来源、字段、默认值、优先级、热更新策略。配置体系是产线稳定的根基，禁止在代码里散落 `if (xxx == "door")` 这种硬编码分支。

## 2. 工业应用场景

- 多车型：不同 VIN 对应不同 ROI 配置
- 多工位：同一算法在 A01 / A02 工位跑，Octree 分辨率不同
- 现场调试：工程师改 JSON 即生效，不重编译
- 版本兼容：老配置文件在新版本算法上仍能跑

## 3. 架构位置

```
启动参数（命令行 / 平台）
        │
        ▼
   OpConfig（Operator 层）
        │ 指定 configPath
        ▼
   roi_config.json（ROI 层）
        │
        ▼
   ROIConfigManager → ROIConfig → ROIRegion[]
```

## 4. 输入输出定义

配置分三层：

| 层级 | 来源 | 示例字段 |
| --- | --- | --- |
| L1 平台层 | Pipeline JSON | Operator 顺序、工位 ID |
| L2 算子层 | OpConfig | configPath、octreeResolutionMM、threadCount |
| L3 业务层 | roi_config.json | coordinate、rois[] |

优先级：L2 > L3 同名字段（L2 覆盖 L3）。

## 5. 数据结构

### 5.1 OpConfig（L2）

```cpp
struct OpConfig {
    std::string opName;
    std::string configPath;            // 必填
    float octreeResolutionMM = 0.0f;   // 0 表示用 L3 默认或内置默认 5.0
    int   threadCount = 0;             // 0 表示硬件并发
    bool  hotReload = false;
};
```

### 5.2 roi_config.json（L3，完整 schema）

```json
{
  "coordinate": "robot",
  "octreeResolutionMM": 5.0,
  "rois": [
    {
      "name": "door_glue_left",
      "type": "box",
      "min": { "x": 100.0, "y": 200.0, "z": 300.0 },
      "max": { "x": 150.0, "y": 260.0, "z": 350.0 }
    }
  ]
}
```

字段表：

| 字段 | 类型 | 必填 | 默认 | 说明 |
| --- | --- | --- | --- | --- |
| coordinate | string | 否 | world | 配置坐标系 |
| octreeResolutionMM | float | 否 | 5.0 | Octree 叶子尺寸 |
| rois | array | 是 | — | 至少 1 个 |
| rois[].name | string | 是 | — | 唯一 |
| rois[].type | string | 否 | box | box/cylinder/sphere/polygon |
| rois[].min/max | vec3 | box 必填 | — | mm |

## 6. 类设计

```cpp
class ROIConfigManager {
    // 见 03 模块
    // 本模块补充：配置合并逻辑
    void mergeOpConfig(const OpConfig& op);   // L2 覆盖 L3
};
```

## 7. 接口设计

- `load(path)`：从 L3 文件读入
- `mergeOpConfig(op)`：L2 覆盖 L3 同名字段
- `getEffectiveResolution()`：返回最终 Octree 分辨率
- `getEffectiveThreadCount()`：返回最终线程数

## 8. 算法流程

```
启动
  ├── 平台下发 OpConfig（L2）
  ├── ROIConfigManager.load(op.configPath)   // L3
  ├── ROIConfigManager.mergeOpConfig(L2)
  │     ├── op.octreeResolutionMM > 0 → 覆盖 config.octreeResolutionMM
  │     └── op.threadCount > 0 → 覆盖 extractor 线程数
  └── 得到有效配置，写入日志"effective config"
```

热更新：

```
if op.hotReload:
    后台每 1s 检查 config.mtime
    变化 → 重新 load + merge → extractor 重建
    失败 → 保留旧配置 + 写 error 日志
```

## 9. 异常处理

- L3 文件缺失：init 直接失败
- L3 字段类型错：使用默认值 + warn 日志（不致命）
- L2 与 L3 同时缺省：使用内置默认 5.0 mm
- 配置版本过低（schemaVersion < 1）：warn 并尝试前向兼容解析

## 10. 测试方案

| 用例 | 期望 |
| --- | --- |
| L2 覆盖 L3 | 最终 resolution = L2 值 |
| L2 为 0 时使用 L3 | 最终 resolution = L3 值 |
| L2 / L3 都缺 | 使用默认 5.0 |
| 热 reload 改文件 | 1s 内生效 |
| 热 reload 改坏文件 | 保留旧配置 + error 日志 |

## 11. AI 实现要求

- 所有可配置字段必须有默认值，禁止"未设置就崩溃"
- 默认值集中写在 `ROIConfigDefaults.h`，不散落
- 不允许在算法代码里读 `std::getenv`，所有配置走 OpConfig / JSON
- 配置变更必须打 info 日志：`config reloaded: resolution=5.0, roiCount=10`
- 字段单位注释必须写（mm / ms / count）

## 12. 后续扩展

- 配置版本号 `schemaVersion: 2`，Manager 校验最低版本
- 配置加密（产线配方保护）
- 远程配置中心（HTTP 拉取）
- 配置 diff 工具：工程师改完 JSON 后自动对比旧版本，提示变更 ROI 数
