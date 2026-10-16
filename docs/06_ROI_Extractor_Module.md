# 06 — ROI 提取模块（ROIExtractor / BoxROIExtractor）

- 版本：v1.0
- 对应实现：`include/ROIExtractor.h` + `src/BoxROIExtractor.cpp`

---

## 1. 模块目标

给定完整点云与 ROI 列表，输出每个 ROI 命中的局部点云。v1 基于 PCL Octree 实现 Box 裁剪，是整个 Pipeline 中性能最敏感的节点。

## 2. 工业应用场景

- 整车扫描 100 万点，10 个 ROI，单帧 < 100 ms
- 裁剪结果直接喂给下游 Preprocess（滤波 / 法向）
- 同一 Octree 复用于多个 ROI，避免重复构建

## 3. 架构位置

```
PointCloudROIManager
    └── owns ──▶ ROIExtractor（抽象接口）
                       │ 实现
                       ▼
                  BoxROIExtractor ──▶ pcl::octree::OctreePointCloudSearch
```

**强制约束**：`#include <pcl/...>` 只出现在 `BoxROIExtractor.cpp`，头文件不暴露 PCL 类型。

## 4. 输入输出定义

| 方向 | 类型 | 说明 |
| --- | --- | --- |
| 输入 | `std::shared_ptr<const PointCloudData> cloud` | 完整点云，只读 |
| 输入 | `const ROIRegion& region` | 单个 ROI |
| 输出 | `ROIResult` | 裁剪结果（含命中点云 / 点数 / 状态） |

## 5. 数据结构

```cpp
class ROIExtractor {
public:
    virtual ~ROIExtractor() = default;

    // 构建搜索结构；同一点云多次 extract 不重复 build
    virtual bool build(std::shared_ptr<const PointCloudData> cloud) = 0;

    // 对单个 ROI 提取
    virtual ROIResult extract(const ROIRegion& region) = 0;

    // 一次性对全部 ROI 提取（批量，内部可并行）
    virtual std::vector<ROIResult> extractAll(const std::vector<ROIRegion>& regions) = 0;
};
```

## 6. 类设计

```cpp
class BoxROIExtractor : public ROIExtractor {
public:
    BoxROIExtractor();
    explicit BoxROIExtractor(float octreeResolutionMM);

    bool build(std::shared_ptr<const PointCloudData> cloud) override;
    ROIResult extract(const ROIRegion& region) override;
    std::vector<ROIResult> extractAll(const std::vector<ROIRegion>& regions) override;

private:
    float resolutionMM_ = 5.0f;   // Octree 叶子尺寸，mm
    std::shared_ptr<const PointCloudData> source_;
    std::unique_ptr<pcl::octree::OctreePointCloudSearch<pcl::PointXYZI>> octree_;

    ROIResult makeEmptyResult(const ROIRegion& r, const std::string& msg);
};
```

## 7. 接口设计

| 接口 | 说明 |
| --- | --- |
| `build` | 把 `PointCloudData` 转成 `pcl::PointCloud<pcl::PointXYZI>::Ptr` 并构建 Octree |
| `extract` | 单 ROI 调用，内部走 `boxSearch` |
| `extractAll` | 批量调用，ROI 间只读 Octree，可并行 |

分辨率 `resolutionMM_` 由 Manager 从配置文件读入，v1 默认 5 mm（汽车涂胶场景）。

## 8. 算法流程

### 8.1 build

```
PointCloudData → pcl::PointCloud<pcl::PointXYZI>
    ├── 逐点拷贝 x/y/z/intensity（避免 PCL 类型泄漏到公共头）
    └── octree_->setInputCloud(cloud)
        octree_->addPointsFromInputCloud()
```

### 8.2 extract（单 ROI）

```
boxSearch(minPt, maxPt, indices)
    ├── indices 为空 → makeEmptyResult("no hit")
    │       └── success=true, pointCount=0, 打 warn 日志（RoiWarnCode::NoHit）
    ├── 构造 shared_ptr<const PointCloudData>（跨 DLL 共享，见 02 §5.6）
    ├── pcl::copyPointCloud(*source, indices, roiCloud)
    ├── 转回平台 PointCloudData（x/y/z/intensity 拷贝回 vector）
    └── 组装 ROIResult { name, cloud=shared_ptr, pointCount, success=true }
```

### 8.3 extractAll（批量）

```
for each region (可并行):
    result[i] = extract(region)
```

并行安全：Octree 在 build 后只读，多线程 boxSearch 无写冲突。

## 9. 异常处理

| 场景 | 行为 |
| --- | --- |
| 未 build 直接 extract | 返回 `success=false, message="extractor not built"`，错误码 3001 |
| 输入点云 size=0 | build 返回 false，错误码 2001 |
| ROI 越界 / 0 命中 | **不算错误**，返回 `success=true, pointCount=0`，打 **warn 级日志** `RoiWarnCode::NoHit`（见 12 §5.2），不进 error 通道 |
| PCL 抛 `std::bad_alloc` | 捕获，转 ROIResult.error = "oom during crop"，错误码 2003 |
| resolutionMM_ ≤ 0 | 构造时 assert 或 clamp 到 1 mm |

## 10. 测试方案

| 用例 | 期望 |
| --- | --- |
| 1000 点整云，ROI 覆盖全部 | pointCount=1000 |
| ROI 不命中 | pointCount=0，success=true |
| ROI 命中边界点 | 边界点被包含（闭区间） |
| 多 ROI 重复 extractAll | 每个 ROI 独立，互不覆盖 |
| 未 build 直接 extract | success=false，message 含 "not built" |
| 性能：100 万点 / 10 ROI | < 100 ms（Release，i7 级别） |
| 并行：10 ROI / 4 线程 | 相对单线程加速 ≥ 2.5x |

## 11. AI 实现要求

- `pcl::PointXYZI` 与平台 `PointXYZI` 的转换集中在 `.cpp` 内，不暴露头文件
- Octree 用 `std::unique_ptr`，禁止裸 `new`
- `boxSearch` 返回的 `indices` 是 `vector<int>`，注意 PCL 用 int 而非 size_t
- 分辨率 `resolutionMM_` 必须有注释（单位 mm）
- 性能关键路径（点拷贝）用 `reserve(indices.size())` 预分配
- 不允许在 extract 内做日志 I/O，日志由 Manager 层统一打

## 12. 后续扩展

- 派生 `CylinderROIExtractor` / `SphereROIExtractor` / `PolygonROIExtractor`
- 增加 VoxelGrid 下采样版本（下游不需要全分辨率时提速）
- 增加 GPU Octree（OptiX / Open3D）以应对 1000 万点场景
