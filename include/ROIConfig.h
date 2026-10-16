/**
 * @file ROIConfig.h
 * @brief ROI 配置容器（02 §5.5 + 13 §5.2）
 * @details 含 coordinate / octreeResolutionMM / regions 三部分，来自 roi_config.json。
 */

#ifndef SBT_ROI_CONFIG_H_
#define SBT_ROI_CONFIG_H_

#include <string>
#include <vector>

#include "Export.h"
#include "ROIRegion.h"

namespace sbt {
namespace proc {

struct PointCloudData;

/** @brief ROI 配置（L3 业务层） */
struct ROIConfig {
    std::string coordinate;             ///< 配置文件声明的坐标系
    float octreeResolutionMM = 5.0f;    ///< Octree 叶子尺寸，单位 mm
    std::vector<ROIRegion> regions;     ///< ROI 列表
};

/**
 * @brief 校验配置坐标系与点云坐标系一致
 * @param cfg [in] ROI 配置
 * @param cloud [in] 输入点云
 * @param errOut [out] 不一致时的结构化原因
 * @return true=一致或任一方为空（空则跳过比对）
 */
SBT_OPS_API bool isCoordinateMatched(const ROIConfig& cfg, const PointCloudData& cloud,
                                     std::string& errOut);

}
}

#endif // SBT_ROI_CONFIG_H_
