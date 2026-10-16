/**
 * @file ROITypes.cpp
 * @brief 数据结构模块的自由函数实现（02 §6）
 * @details isValidBoxRegion / isCoordinateMatched。
 */

#include "ROIConfig.h"
#include "PointCloudData.h"
#include "ROIRegion.h"

#include <cmath>
#include <string>

namespace sbt {
namespace proc {

namespace {

bool isFiniteVec3(const Eigen::Vector3f& v) {
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}

std::string vec3ToString(const Eigen::Vector3f& v) {
    return "(" + std::to_string(v.x()) + ", " + std::to_string(v.y()) + ", " +
           std::to_string(v.z()) + ")";
}

}  // namespace

bool isValidBoxRegion(const ROIRegion& region, std::string& errOut) {
    if (region.name.empty()) {
        errOut = "name: non-empty required, got ''";
        return false;
    }
    if (!isFiniteVec3(region.min)) {
        errOut = "min: finite required, got " + vec3ToString(region.min);
        return false;
    }
    if (!isFiniteVec3(region.max)) {
        errOut = "max: finite required, got " + vec3ToString(region.max);
        return false;
    }
    if (region.min.x() > region.max.x() || region.min.y() > region.max.y() ||
        region.min.z() > region.max.z()) {
        errOut = "box: min <= max required, got min=" + vec3ToString(region.min) +
                 ", max=" + vec3ToString(region.max);
        return false;
    }
    return true;
}

bool isCoordinateMatched(const ROIConfig& cfg, const PointCloudData& cloud,
                         std::string& errOut) {
    // 任一方为空则跳过比对（Manager 侧打 CoordinateSkipped warn）
    if (cfg.coordinate.empty() || cloud.coordinate.empty()) {
        return true;
    }
    if (cfg.coordinate != cloud.coordinate) {
        errOut = "coordinate: mismatch, config=" + cfg.coordinate +
                 ", cloud=" + cloud.coordinate;
        return false;
    }
    return true;
}

}  // namespace proc
}  // namespace sbt
