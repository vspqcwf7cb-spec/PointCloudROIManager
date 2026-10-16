# 11 — 性能优化设计

- 版本：v1.0
- 目标：100 万点 / 10 ROI / 单帧 < 100 ms

---

## 1. 模块目标

在不改变算法正确性的前提下，把 PointCloudROIManager 从"能跑"压到"产线节拍"：100 万整车点云切 10 个 ROI，端到端 < 100 ms（Release，i7-12700 / 32 GB）。

## 2. 工业应用场景

- 整车扫描节拍 1~2 秒，ROI 切割必须在 100 ms 内完成，否则吃掉下游测量时间预算
- 现场升级 CPU 不能解决问题，必须算法层优化
- 性能退化 20% 以上会直接影响 MES 节拍，必须前置在 CI 里拦下来

## 3. 架构位置

性能优化集中在三个热点：

```
PointCloudData ──▶ pcl::PointCloud      热点 1：点类型转换
                       │
                       ▼
                  Octree build           热点 2：空间索引构建
                       │
                       ▼
                  boxSearch × 10         热点 3：ROI 查询
```

## 4. 输入输出定义

| 输入 | 输出 |
| --- | --- |
| 100 万点 PointCloudData + 10 ROI | 端到端耗时（ms）、内存峰值（MB） |

## 5. 数据结构

性能相关参数（从 ROI 配置读入）：

```cpp
struct PerfParams {
    float octreeResolutionMM = 5.0f;     // Octree 叶子尺寸
    int   extractThreadCount = 4;          // ROI 并行线程数
    bool  reuseOctreeAcrossFrames = true; // 同配置下复用上一帧 Octree
};
```

## 6. 类设计

不新增类，性能优化通过 `BoxROIExtractor` 的内部实现升级完成：

- 点类型转换：批量 `reserve` + 直接内存拷贝
- Octree：`setInputCloud` 后不重复 `addPointsFromInputCloud`
- ROI 查询：`std::execution::par_unseq` 并行 boxSearch

## 7. 接口设计

```cpp
class BoxROIExtractor {
public:
    void setThreadCount(int n);        // 默认硬件并发数
    void setOctreeResolution(float mm);
    bool reuseOctreeIfSameCloud(std::shared_ptr<const PointCloudData> cloud);
};
```

## 8. 算法流程（热点分解）

### 8.1 热点 1：点类型转换

朴素写法（慢）：

```cpp
for (auto& p : cloud->points) {       // 100 万次
    pclCloud->push_back(pt);
}
```

优化写法（快）：

```cpp
pclCloud->points.reserve(cloud->points.size());
pclCloud->points.resize(cloud->points.size());
for (size_t i = 0; i < cloud->points.size(); ++i) {
    const auto& s = cloud->points[i];
    auto& d = pclCloud->points[i];
    d.x = s.x; d.y = s.y; d.z = s.z; d.intensity = s.intensity;
}
```

目标：100 万点转换 ≤ 5 ms。

### 8.2 热点 2：Octree 构建

- 同一点云重复 build 是浪费。Manager 记录上一帧 `cloud.get()` 指针，未变则跳过
- 分辨率 5 mm 下，Octree 节点数约 20 万，build 时间目标 ≤ 30 ms
- 避免 `octree_->defineBoundingBox()` 反复调用

### 8.3 热点 3：boxSearch × 10

```cpp
std::vector<ROIResult> results(regions.size());
std::for_each(std::execution::par_unseq,
              regions.begin(), regions.end(),
              [&](const ROIRegion& r) {
                  auto idx = &r - &regions[0];
                  results[idx] = this->extract(r);
              });
```

目标：10 ROI 并行 ≤ 50 ms。

### 8.4 端到端预算

| 阶段 | 预算 |
| --- | --- |
| 点类型转换 | 5 ms |
| Octree build | 30 ms |
| 10 ROI boxSearch（并行） | 50 ms |
| 结果回填 | 5 ms |
| **合计** | **90 ms** |

## 9. 异常处理

- 并行线程数设为 0：clamp 到 1（串行）
- 并行中单个 ROI 抛异常：不影响其他 ROI，该 ROI result 标失败
- Octree 复用判定失败（点云 size 不同）：自动重建，不抛
- 内存不足（reserve 失败）：捕获 `bad_alloc`，降级为串行 + 小块处理

## 10. 测试方案

| 用例 | 目标 |
| --- | --- |
| 100 万点 / 10 ROI 端到端 | ≤ 100 ms |
| 50 万点 / 5 ROI | ≤ 40 ms |
| 1000 万点（极限压测） | ≤ 500 ms |
| 内存峰值 | ≤ 2 × 输入点云大小 |
| 并行 vs 串行结果一致性 | 命中索引集合完全相等 |

性能基线见 `10_Test_Design.md` 的 `baseline.json`，退化 15% 告警。

## 11. AI 实现要求

- 热点函数必须加注释：`// hot path: keep branch-free, reserve before loop`
- 不允许在热点路径里 `std::cout` / `spdlog::info`（I/O 会让 cache 失温）
- `std::execution::par_unseq` 必须用 `<algorithm>` 内算法，禁止手写裸线程
- 所有 `push_back` 在循环外 `reserve`
- 不允许在 `extract` 内构造 `std::string`，错误字符串延迟到最后一次性拼
- Profiler 数据（VTune / Tracy）作为 PR 附件提交

## 12. 后续扩展

- VoxelGrid 降采样前置：100 万 → 20 万点，Octree build 直接降 5x
- GPU Octree（Open3D / OptiX）：应对 1000 万点级扫描
- 内存映射点云（mmap PLY）：避免 read() 系统调用
- 增量 Octree：点云只追加时不重建
