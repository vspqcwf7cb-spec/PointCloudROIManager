# 16 — 文档体系校验（合理性 / 科学性 / 高效性）

- 版本：v1.0
- 校验对象：00–15 全部控制文档
- 校验日期：2026-10-14

---

## 1. 模块目标

对 PointCloudROIManager 整套 16 份控制文档做一次结构化评审，从三个维度打分并指出问题：

- **合理性（Consistency）**：模块边界、命名、依赖方向是否自洽，有没有自相矛盾
- **科学性（Correctness）**：算法选型、数据结构、性能数字是否站得住
- **高效性（Efficiency）**：热点抓得对不对、预算是否现实、有没有浪费

校验原则：**不打满分，只挑问题**。文档不是宣传材料，是给人挑刺用的。

## 2. 总体评分

| 维度 | 评分 | 说明 |
| --- | --- | --- |
| 合理性 | B+ | 主结构自洽，但有 4 处跨文档不一致 |
| 科学性 | B | 算法选型正确，性能预算偏乐观，2 处遗漏 |
| 高效性 | B- | 热点抓对，但并行模型 / 内存拷贝有低估 |
| 落地差距 | C | 文档体系未对齐当前 CMake / 代码现状 |

## 3. 合理性校验（Consistency）

### 3.1 已自洽的部分

- 模块拆分（Config / Parser / Geometry / Extractor / Manager / Operator）在 00 / 01 / 07 三份里描述一致
- 依赖方向（上层 → 下层单向）在 01 / 07 / 14 三处约束一致
- 错误码区段（Config 1xxx / Cloud 2xxx / Extractor 3xxx / Operator 4xxx）在 12 / 14 一致
- 公共头禁止 PCL / JSON 类型泄漏，在 01 / 06 / 08 / 14 / 15 五处反复强调

### 3.2 发现的不一致

| # | 位置 | 问题 | 修正建议 |
| --- | --- | --- | --- |
| C1 | 02 vs 05 | 02 说 Cylinder / Polygon 字段"以追加方式进入 ROIRegion"；05 说"扩展位单独建几何类型"。两边对扩展落点不一致 | 统一为：ROIRegion 内追加字段（不新建并行结构），ROIGeometry 提供 dispatch |
| C2 | 12 §5.1 | `ExtractorNoHit = 3003` 命名为 ErrorCode，但语义是"不算错误"。混入错误码表会让 MES 误报 | 把 NoHit 从 RoiErrorCode 拆出，单独定义 `RoiWarnCode`，或在错误码表加注 `level=warn` |
| C3 | 07 vs 02 | 07 说"coordinate 不一致 → 整批失败"，但 02 写 `PointCloudData.coordinate` 是非必填字符串。空字符串算"不一致"还是"跳过"？ | 02 增补：coordinate 为空时，Manager 跳过坐标系校验并打 warn；非空时才比对 |
| C4 | 00 vs 09 | 00 写"可视化 / 保存迁移到下游 ResultExporter"，但 09 Pipeline 没有定义这个 Operator，现有 `drawVisual` / `makeCropCloudFile` 在新体系里无处安放 | 09 增补一节"ResultExporter Operator"位置，或显式声明"v1 阶段暂留在 ROIExtractor 周边工具类" |

## 4. 科学性校验（Correctness）

### 4.1 算法选型正确

- PCL Octree 做 Box 查询：复杂度 O(log N + k)，对 100 万点 / 10 ROI 场景是行业标准做法（参考 PCL 官方 boxSearch demo）
- 平台 POD 类型（PointXYZI / ROIRegion）与 PCL 类型隔离：是动态库 ABI 稳定的标准做法
- 闭区间 `<=` 判定：胶道边界点不能漏，工程上正确

### 4.2 性能数字偏乐观或遗漏

| # | 位置 | 问题 | 修正建议 |
| --- | --- | --- | --- |
| S1 | 11 §8.4 | 端到端预算 90 ms 只算了"平台 → PCL 一次正向拷贝"，没算反向拷贝（PCL indices → 平台 `PointCloudData`）。实测反向拷贝与正向同量级，应再 +5 ms | 预算改为：点转换 5 + Octree build 30 + boxSearch 50 + 反向回填 5 + 结果组装 5 = 95 ms，留 5 ms 余量到 100 ms |
| S2 | 09 §8.4 | Pipeline 整帧预算加总 360 ms，但结论写"≤ 450 ms"，中间 90 ms 没解释 | 写明 90 ms 是 I/O（ply 读盘）+ MES 上报 + 调度抖动 buffer |
| S3 | 06 §8.1 | 100 万点从 `std::vector<PointXYZI>` 到 `pcl::PointCloud<PointXYZI>` 的转换，代码示例用逐字段赋值。16 MB 顺序拷贝理论 < 2 ms，5 ms 偏保守但可接受 | 备注：实际应以 VTune 实测为准，5 ms 是上界 |
| S4 | 11 §8.3 | `std::execution::par_unseq` 对 10 ROI 并行，加速比 ≥ 2.5x 的假设建立在"boxSearch 是 CPU-bound"上。但 10 ROI 总工作 50 ms，线程调度本身 ~1 ms，开销可接受 | 保留，但补一句"ROI 数 < 4 时不建议并行，串行更快" |

### 4.3 ABI 风险（真实问题）

| # | 位置 | 问题 | 修正建议 |
| --- | --- | --- | --- |
| A1 | 02 §5.6 | `ROIResult` 直接持有 `PointCloudData cloud`（值语义，含 `std::vector` + `std::string`），跨 DLL 边界（MSVC Debug/Release、不同 VS 版本）会崩 | 跨 DLL 边界传递时改为 `shared_ptr<const PointCloudData>`；值语义仅在 DLL 内部使用 |
| A2 | 02 §5.1 | `static_assert(sizeof(PointXYZI) == 16)` 已锁死布局，正确；但 `std::string` 字段没锁布局 | 跨 ABI 的 ROI 配置建议用 C 接口（`const char*` + length）导出，参考现有 `extern "C++" SBT_..._API` 宏 |

## 5. 高效性校验（Efficiency）

### 5.1 热点抓得对

- 点类型转换、Octree build、boxSearch 三个热点在 06 / 11 都被点名
- 复用 Octree（同一点云多帧）在 11 §8.2 有明确策略
- 禁止在热点路径打日志（14 / 11 一致）

### 5.2 仍存在的浪费

| # | 位置 | 问题 | 修正建议 |
| --- | --- | --- | --- |
| E1 | 06 §8.2 | `pcl::copyPointCloud` 默认会做类型转换检查，对 PointXYZI → PointXYZI 是浪费 | 直接用 `pcl::copyPointCloud(cloud, indices, outCloud)` 的重载，已经是最快路径；但 `outCloud->points.resize(indices.size())` 可省去 |
| E2 | 07 §8 | `execute` 每帧构造 `ROIResult` 向量时 `reserve` 了，但 `ROIResult.cloud.points` 没 reserve（要等 extract 完才知道 size） | Extractor 内提前 `cloud.points.reserve(indices.size())` |
| E3 | 11 §8.3 | `std::execution::par_unseq` 要求循环体无共享状态。当前 `results[idx]` 写入虽然不冲突，但 ROIResult 构造含 std::string 分配，多线程分配器锁竞争可能拖慢 | 用 `tbb::flow_graph` 或预分配 string buffer；实测验证 |

## 6. 落地差距（真实工程问题）

文档体系是给未来重构用的，但**当前仓库状态**还没对齐：

| # | 现状 | 文档要求 | 差距 |
| --- | --- | --- | --- |
| G1 | `CMakeLists.txt` 只有 `sbt_cv_ops` 单体库 + Demo | 文档要求拆成 ROIConfigManager / ROIParser / ROIExtractor / Manager / Operator 多个 target | 需要在重构 Step 02–08 逐步建 target |
| G2 | 现有 `pointCloudCropProcess.cpp` 把 Parser / Extractor / Visual 全写在一个 `.cpp` 24 KB | 文档要求"一个 cpp 不超过一个职责" | 按 00 末尾的迁移映射拆文件 |
| G3 | 现有公共头暴露了 `cv::Mat visual` | 文档要求公共头不泄漏 OpenCV 类型（虽然 OpenCV 不是 PCL，但跨 DLL 传 cv::Mat 同样有 ABI 风险） | 重构时把 visual 移出公共结构，或改为裸像素 buffer |
| G4 | `data/` 下只有 sample JSON，没有点云 | 测试体系要求合成点云生成器 | 在 `tests/utils/synthetic_cloud.h` 实现 |

## 7. 修正清单（按优先级）

### P0（已修正 ✅ 2026-10-14）

1. **C1**：统一 ROIRegion 扩展落点 —— 已在 02 §5.4 / §12、05 §8.3 / §12 回写，明确"字段追加到 ROIRegion 内，不新建并行结构，不用继承体系"
2. **A1**：ROIResult 跨 DLL 边界改 shared_ptr —— 已在 02 §5.6、06 §8.2、07 §8 回写，`cloud` 类型改为 `std::shared_ptr<const PointCloudData>`，并说明 DLL 内部值语义例外
3. **C2**：ExtractorNoHit 从 error code 拆出 —— 已在 12 §5.1 / §5.2 / §11 回写，新增 `RoiWarnCode`（NoHit=3003 迁到 warn，配 ConfigFieldDefaulted / CoordinateSkipped / HotReloadFailed），MES 只对 ERROR 报警

### P1（应该改，影响工程正确性）

4. **S1**：性能预算加反向拷贝 5 ms
5. **G3**：cv::Mat 从公共头移除
6. **C3**：coordinate 空字符串语义明确 —— 已在 07 §8 execute 流程中回写：双方非空且不一致才报 2002，任一方为空则跳过校验并打 warn（RoiWarnCode::CoordinateSkipped）

### P2（建议改，长期收益）

7. **C4**：ResultExporter 位置在 09 补一节
8. **E2**：Extractor 内提前 reserve
9. **S2**：Pipeline 整帧 90 ms buffer 写明来源

## 8. AI 后续执行约束

按本校验文档，AI 在 Step 02（ROI Config Module）开始写代码前，必须：

1. ~~先回写本文档中 P0 三项修正到对应模块文档（02 / 12）~~ ✅ 已完成（见 §7 P0 标记）
2. 重构现有 `pointCloudCropProcess` 时，公共头不再暴露 `cv::Mat`（P1-G3，进入代码阶段时处理）
3. 性能数字以实测为准，文档预算仅作上界（P1-S1，进入代码阶段时通过 profiling 校准）

## 9. 修正记录（2026-10-14）

本次 P0 修正涉及文件：

| 文件 | 改动 |
| --- | --- |
| 02_Data_Structure_Design.md | §5.4 ROIRegion 加扩展策略注释；§5.6 ROIResult.cloud 改 shared_ptr；§12 扩展位统一到 ROIRegion 内追加 |
| 05_ROI_Geometry_Module.md | §8.3 扩展位加"字段追加到 ROIRegion"说明；§12 后续扩展加约束 |
| 06_ROI_Extractor_Module.md | §8.2 extract 流程补 shared_ptr 构造；§9 异常表 NoHit 改 warn 通道 |
| 07_PointCloudROIManager_Module.md | §8 execute 坐标系校验分三种情况（不一致 / 空字符串跳过 / 一致）；results 类型标注 shared_ptr |
| 12_Error_Log_System.md | §5.1 错误码表删 3003；新增 §5.2 RoiWarnCode；§11 警告码区段规则 |
| 16_Document_Review.md | 本节 |

剩余 P1 / P2 项在 Step 02 代码阶段一并处理，文档体系可进入代码阶段。
