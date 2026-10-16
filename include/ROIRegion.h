/**
 * @file ROIRegion.h
 * @brief ROI 空间描述结构（02 §5.4）
 * @details 扩展策略（强制）：所有新增 ROI 类型字段一律以"追加成员"写入本结构体，
 *          禁止新建并行结构或派生类，保证跨 DLL ABI 稳定。
 */

#ifndef SBT_ROI_REGION_H_
#define SBT_ROI_REGION_H_

#include <string>

#include <Eigen/Core>

#include "Export.h"
#include "ROITypes.h"

namespace sbt {
namespace proc {

/** @brief 单个 ROI 空间描述 */
struct ROIRegion {
    std::string name;                          ///< 唯一标签，如 "door_glue"
    ROIType type = ROIType::Box;               ///< ROI 类型，v1 仅 Box

    // ---- Box 字段（v1 使用）----
    Eigen::Vector3f min = Eigen::Vector3f::Zero();   ///< 包围盒最小点，单位 mm
    Eigen::Vector3f max = Eigen::Vector3f::Zero();   ///< 包围盒最大点，单位 mm

    // ---- 预留字段（后续类型扩展追加在此注释下方，禁止改名 / 改顺序）----
    // Cylinder: axisBottom / axisTop / radius
    // Sphere:   center / radius
    // Polygon:  polygonXY / zMin / zMax
    // Path:     pathPoints / width
};

/**
 * @brief 校验 ROI 包围盒合理性
 * @param region [in] ROI 描述
 * @param errOut [out] 失败原因（结构化字符串）
 * @return true=合法（name 非空、min<=max、所有分量有限）
 */
SBT_OPS_API bool isValidBoxRegion(const ROIRegion& region, std::string& errOut);

}
}

#endif // SBT_ROI_REGION_H_
