/**
 * @file ROITypes.h
 * @brief 基础点类型与 ROI 类型枚举（02 §5.1 / §5.3）
 * @details 公共头，禁止依赖 PCL。所有类型只使用 STL / Eigen / 基础类型。
 */

#ifndef SBT_ROI_TYPES_H_
#define SBT_ROI_TYPES_H_

#include <cstdint>

namespace sbt {
namespace proc {

/**
 * @brief 平台点类型（几何 + 强度，不含法向 / RGB）
 * @details 单位 mm。默认 0，避免野点。跨 ABI 布局用 static_assert 锁死。
 */
struct PointXYZI {
    float x = 0.0f;         ///< X 坐标，单位 mm
    float y = 0.0f;         ///< Y 坐标，单位 mm
    float z = 0.0f;         ///< Z 坐标，单位 mm
    float intensity = 0.0f; ///< 强度值
};

static_assert(sizeof(PointXYZI) == 16, "PointXYZI must stay 16 bytes for ABI");

/** @brief ROI 空间类型枚举；v1 仅实现 Box，其余为预留 */
enum class ROIType {
    Box = 0,
    Cylinder,   // reserved
    Sphere,     // reserved
    Polygon,    // reserved
    Path        // reserved
};

}
}

#endif // SBT_ROI_TYPES_H_
