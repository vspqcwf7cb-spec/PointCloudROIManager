# 05 — ROI 几何模块（ROIGeometry）

- 版本：v1.0
- 对应实现：`include/ROIGeometry.h` + `src/ROIGeometry.cpp`

---

## 1. 模块目标

定义"一个点是否落在某个 ROI 空间内"的几何判定逻辑。本身不查点云，只做**纯几何运算**，方便单独单测。

## 2. 工业应用场景

- Box ROI：零件局部区域裁剪
- 后续 Cylinder：螺栓孔、销轴柱面检测
- 后续 Path：沿焊缝 / 胶道的带状 ROI

## 3. 架构位置

```
ROIExtractor
    └── 对每个候选点调用 ROIGeometry::contains(point, region)
            │
            ▼
        ROIRegion（只读）
```

ROIGeometry 是纯函数集合，无状态，可被多个 Extractor 共享。

## 4. 输入输出定义

| 方向 | 类型 | 说明 |
| --- | --- | --- |
| 输入 | `Eigen::Vector3f point` | 单候选点（mm） |
| 输入 | `const ROIRegion& region` | ROI 空间描述 |
| 输出 | `bool` | 是否在 ROI 内 |
| 输出 | （批量）`std::vector<int>` | 批量查询时返回命中索引 |

## 5. 数据结构

```cpp
// v1 仅 Box 判定
struct BoxBounds {
    Eigen::Vector3f min;
    Eigen::Vector3f max;
};
```

`ROIRegion` 与 `BoxBounds` 之间的转换在 ROIGeometry 内完成，对外只暴露 `contains`。

## 6. 类设计

```cpp
class ROIGeometry {
public:
    // 单点判定
    static bool contains(const Eigen::Vector3f& p, const ROIRegion& region);

    // 批量判定：返回命中的点索引
    static std::vector<int> filterInside(const std::vector<Eigen::Vector3f>& pts,
                                         const ROIRegion& region);

    // 包围盒快速剪枝：返回点云整体 AABB 与 ROI 是否相交（Octree 层先用）
    static bool intersectsAabb(const BoxBounds& aabb, const ROIRegion& region);
};
```

## 7. 接口设计

| 接口 | 用途 |
| --- | --- |
| `contains` | 单点几何判定，单元测试主力 |
| `filterInside` | 批量判定，供朴素实现（无 Octree 时兜底） |
| `intersectsAabb` | 粗筛：Octree 节点与 ROI 是否相交，决定是否下钻 |

## 8. 算法流程

### 8.1 Box contains

```
p ∈ [min, max]  ⇔  min.x ≤ p.x ≤ max.x
                  min.y ≤ p.y ≤ max.y
                  min.z ≤ p.z ≤ max.z
（闭区间，闭边界）
```

### 8.2 AABB 相交

```
相交 ⇔  NOT ( roi.max.x < aabb.min.x
             OR roi.min.x > aabb.max.x
             OR roi.max.y < aabb.min.y
             OR roi.min.y > aabb.max.y
             OR roi.max.z < aabb.min.z
             OR roi.min.z > aabb.max.z )
```

### 8.3 扩展位

新增 ROI 类型时，判定函数追加到本类，字段追加到 `ROIRegion`（见 02 §5.4），不新建派生几何类：

```
Cylinder contains:  dist(p - axisBottom, axisDir) ≤ radius
                    AND  height(p, axis) ∈ [0, height]
Sphere   contains:  |p - center| ≤ radius
Polygon  contains:  pointInPolygon(xy) AND min.z ≤ p.z ≤ max.z
```

## 9. 异常处理

- region 为 NaN / Inf：`contains` 直接返回 false，并在 ROIGeometry 内记录一次 warn 日志（不抛）
- region.type 未知（未来新加但未实现）：返回 false + 日志 `"unsupported roi type"`
- 批量输入空 vector：返回空 vector，不抛

## 10. 测试方案

| 用例 | 期望 |
| --- | --- |
| 点在盒内 | true |
| 点在盒外（x 方向越界） | false |
| 点在盒边界上 | true（闭区间） |
| min > max（非法 region） | false + 日志 |
| NaN 点 | false |
| 批量：1000 点中 300 命中 | 返回 300 个索引 |
| AABB 相交 / 不相交 6 个轴方向 | 全部覆盖 |

性能：单点 contains < 10 ns（无分支预测失败），批量 100 万点 < 5 ms。

## 11. AI 实现要求

- 全部函数 `static`，类无成员，无状态
- 用 `Eigen::Vector3f`，避免 `pcl::PointXYZ` 进入公共头
- 闭区间边界用 `<=` 而非 `<`，避免胶道边界点被漏掉
- 不允许在 ROIGeometry 内做任何 I/O、日志之外的副作用
- 数值比较不做 epsilon 容忍（输入是 mm 级整数坐标，无需模糊比较）

## 12. 后续扩展

- 增加 `frustum`（视锥 ROI，用于相机视野裁剪）
- 增加 `obb`（有向包围盒，零件斜放场景）
- 批量 SIMD 优化（Eigen::Vector3f 数组化，SSE/AVX）
- 新增类型的字段一律追加到 `ROIRegion`（02 §5.4），本类只追加 `contains` 重载
