/**
 * @file ROIGeometry.h
 * @brief ROI 空间几何判定（05 模块）
 * @details 纯函数集合，无状态，只做"点是否在 ROI 内"的几何运算，不查点云。
 */

#ifndef SBT_ROI_GEOMETRY_H_
#define SBT_ROI_GEOMETRY_H_

#include <vector>

#include <Eigen/Core>

#include "Export.h"
#include "ROIRegion.h"

namespace sbt {
namespace proc {

/**
 * @brief 点云整体 AABB（05 §5），Octree 粗筛用
 */
struct BoxBounds {
    Eigen::Vector3f min;   ///< 最小点，单位 mm
    Eigen::Vector3f max;   ///< 最大点，单位 mm
};

/**
 * @brief ROI 几何判定类（全部 static，无成员无状态）
 */
class SBT_OPS_API ROIGeometry {
public:
    /**
     * @brief 单点判定（Box 闭区间：min <= p <= max）
     * @param p [in] 候选点，单位 mm
     * @param region [in] ROI 空间描述
     * @return 是否在 ROI 内；非法 region（NaN/Inf/非 Box）返回 false
     */
    static bool contains(const Eigen::Vector3f& p, const ROIRegion& region);

    /**
     * @brief 批量判定，返回命中的点索引（朴素实现，无 Octree 时兜底）
     */
    static std::vector<int> filterInside(const std::vector<Eigen::Vector3f>& pts,
                                         const ROIRegion& region);

    /**
     * @brief AABB 相交粗筛：点云整体包围盒与 ROI 是否相交（决定是否下钻）
     */
    static bool intersectsAabb(const BoxBounds& aabb, const ROIRegion& region);
};

}
}

#endif // SBT_ROI_GEOMETRY_H_
