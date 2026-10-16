# 09 — Pipeline 接入设计

- 版本：v1.0
- 对应实现：`config/pipeline_glue_detection.json` + 平台侧调度

---

## 1. 模块目标

把 PointCloudROIManagerOperator 嵌入完整汽车涂胶检测 Pipeline，定义上下游数据契约、坐标系统一、错误回退策略。

## 2. 工业应用场景

- 整车扫描工位：机器人触发 → 取点云 → ROI 切割 → 胶道特征 → 测量 → OK/NG
- 异常时必须能定位到具体是哪一步失败，不能整批崩溃
- 多车型切换时，仅 ROI 配置文件不同，Operator 二进制复用

## 3. 架构位置

```
[Robot Trigger]
       ↓
PointCloudLoaderOperator        (pcd/ply 读取, coordinate="robot")
       ↓
PointCloudROIManagerOperator    (本模块, coordinate 校验)
       ↓
PointCloudPreprocessOperator     (滤波 / 法向)
       ↓
GlueFeatureOperator              (胶道分割 / 中心线)
       ↓
MeasurementOperator              (宽度 / 高度 / 体积)
       ↓
InspectionOperator               (OK/NG 判定)
```

## 4. 输入输出定义

整条 Pipeline 的 `DataBag` 流转契约：

| 阶段 | 写入键 | 读出键 |
| --- | --- | --- |
| Loader | `input_cloud` | — |
| ROIManager | — | `input_cloud` → 写 `roi_result` / `roi_clouds` |
| Preprocess | — | `roi_clouds` → 写 `preprocessed_clouds` |
| Feature | — | `preprocessed_clouds` → 写 `glue_centerlines` |
| Measurement | — | `glue_centerlines` → 写 `measurements` |
| Inspection | — | `measurements` → 写 `final_decision` |

## 5. 数据结构

Pipeline 配置示例：

```json
{
  "pipeline": "glue_detection_v1",
  "operators": [
    {
      "name": "PointCloudLoaderOperator",
      "params": { "cloudDir": "D:/scan/latest.ply" }
    },
    {
      "name": "PointCloudROIManagerOperator",
      "params": {
        "configPath": "config/roi_config.json",
        "octreeResolutionMM": 5.0
      }
    },
    {
      "name": "PointCloudPreprocessOperator",
      "params": { "voxelSizeMM": 1.0 }
    }
  ]
}
```

## 6. 类设计

本模块不新增类，只规定 Operator 的接入契约。平台层提供 `PipelineEngine`，本模块只交付：

- `PointCloudROIManagerOperator.dll`
- 一份可被平台加载的 pipeline JSON
- 一份 ROI 配置示例 `config/roi_config.json`

## 7. 接口设计

约定 Platform Engine 调用顺序：

```
for op in operators: op.init(params)
loop every frame:
    bag = DataBag{}
    for op in operators:
        if not op.execute(bag_in, bag_out):
            engine.handleError(op.name, bag_out.error)
            break
    engine.report(bag)
for op in operators: op.release()
```

## 8. 算法流程

一帧完整时序：

```
t=0     Robot 触发，LoaderOperator 读 ply，构造 PointCloudData(coordinate="robot")
t=5ms   ROIManagerOperator.init 已在启动时完成；execute 命中缓存 Octree 不需要重建
t=10ms  10 个 ROI 全部裁剪完毕，roi_clouds 就绪
t=...   下游 Preprocess / Feature / Measurement 顺序消费
t=N     Inspection 输出 OK/NG，写回 MES
```

性能预算（100 万点 / 10 ROI）：

| 阶段 | 预算 |
| --- | --- |
| Loader | ≤ 50 ms |
| ROIManager | ≤ 100 ms |
| Preprocess | ≤ 80 ms |
| Feature | ≤ 120 ms |
| Measurement | ≤ 60 ms |
| 整帧 | ≤ 450 ms |

## 9. 异常处理

| 故障 | 策略 |
| --- | --- |
| ROI 配置文件被误改 | ROIManager.init 失败 → 平台标记整线维护，不进入测量 |
| 单 ROI 命中 0 点 | 该 ROI 写 NG="no glue"，不阻塞其他 ROI |
| 整帧点云为空 | Loader 失败 → Pipeline 中止，本帧跳过，不写脏数据 |
| Octree build OOM | ROIManager.execute 失败 → 该帧重试一次，仍失败则报设备异常 |

## 10. 测试方案

| 用例 | 期望 |
| --- | --- |
| 整流程跑通（ply + 合法 ROI） | final_decision 有值 |
| 故意给错 ROI 配置 | Pipeline 在 ROIManager 阶段中止，error 可定位 |
| 故意给空点云 | Loader 阶段中止 |
| 单 ROI 0 命中 | 该 ROI=NG，其他 ROI 正常出结果 |
| 连续跑 1000 帧 | 无内存泄漏，帧间状态干净 |
| 性能基准 | 整帧 ≤ 450 ms |

## 11. AI 实现要求

- 不允许在 ROIManager 内部写死 pipeline 其他 Operator 的类型名
- 数据流键名（`input_cloud` / `roi_result` / `roi_clouds`）必须与本表一致，写在常量头中
- pipeline JSON 中的 Operator 顺序即执行顺序，不允许运行时动态调序
- 错误回退策略必须在 `handleError` 内实现，不允许 Operator 自己决定整帧是否继续

## 12. 后续扩展

- 多车型：按 VIN 动态加载不同 ROI 配置
- 多线体：同一 Operator 二进制跑多条产线，配置文件按工位隔离
- 热切换：不重启 Pipeline 更新 ROI 配置
- 灰度发布：新 Operator 与旧 Operator 并行跑，结果 diff 监控
