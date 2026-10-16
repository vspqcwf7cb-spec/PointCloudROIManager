/**
 * @file ROIGeometry.cpp
 * @brief ROIGeometry 实现（05 模块）
 * @details 纯几何判定，无 I/O / 日志副作用（非法 region 已在 ROIConfigManager::validateConfig
 *          阶段被 isValidBoxRegion 拦截，此处仅做快速纯函数兜底）。
 */

#include "ROIGeometry.h"

#include <cmath>

#include "ROIRegion.h"

namespace sbt {
namespace proc {

namespace {

bool finiteVec3(const Eigen::Vector3f& v) {
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}

bool finiteRegion(const ROIRegion& r) {
    return finiteVec3(r.min) && finiteVec3(r.max);
}

// 闭区间 Box 判定（05 §8.1，边界用 <=，避免胶道边界点漏掉）
bool boxContains(const Eigen::Vector3f& p, const ROIRegion& r) {
    return p.x() >= r.min.x() && p.x() <= r.max.x() &&
           p.y() >= r.min.y() && p.y() <= r.max.y() &&
           p.z() >= r.min.z() && p.z() <= r.max.z();
}

}  // namespace

bool ROIGeometry::contains(const Eigen::Vector3f& p, const ROIRegion& region) {
    if (!finiteVec3(p) || !finiteRegion(region) || region.type != ROIType::Box) {
        return false;
    }
    return boxContains(p, region);
}

std::vector<int> ROIGeometry::filterInside(const std::vector<Eigen::Vector3f>& pts,
                                           const ROIRegion& region) {
    std::vector<int> out;
    if (pts.empty() || !finiteRegion(region) || region.type != ROIType::Box) {
        return out;
    }
    out.reserve(pts.size() / 4 + 1);  // 经验预分配，命中率未知时取 1/4
    for (std::size_t i = 0; i < pts.size(); ++i) {
        if (finiteVec3(pts[i]) && boxContains(pts[i], region)) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

bool ROIGeometry::intersectsAabb(const BoxBounds& aabb, const ROIRegion& region) {
    if (region.type != ROIType::Box || !finiteRegion(region)) {
        return false;
    }
    if (!finiteVec3(aabb.min) || !finiteVec3(aabb.max)) {
        return false;
    }
    // 分离轴判定（05 §8.2）：任一轴分离即不相交
    return !(region.max.x() < aabb.min.x() || region.min.x() > aabb.max.x() ||
             region.max.y() < aabb.min.y() || region.min.y() > aabb.max.y() ||
             region.max.z() < aabb.min.z() || region.min.z() > aabb.max.z());
}

}  // namespace proc
}  // namespace sbt
