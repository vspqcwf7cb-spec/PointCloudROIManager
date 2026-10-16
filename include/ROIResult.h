/**
 * @file ROIResult.h
 * @brief 单 ROI 裁剪结果（02 §5.6）
 * @details cloud 字段必须是 shared_ptr<const PointCloudData>，禁止值语义（跨 DLL ABI 约束）。
 */

#ifndef SBT_ROI_RESULT_H_
#define SBT_ROI_RESULT_H_

#include <cstddef>
#include <memory>
#include <string>

#include "PointCloudData.h"

namespace sbt {
namespace proc {

/** @brief 单个 ROI 的裁剪结果 */
struct ROIResult {
    std::string name;                                ///< ROI 名称
    std::shared_ptr<const PointCloudData> cloud;     ///< 裁剪点云（跨 DLL 共享，禁止值拷贝）
    std::size_t pointCount = 0;                      ///< == cloud->points.size()
    bool success = false;                            ///< 是否成功（0 命中仍为 true，见 RoiWarnCode::NoHit）
    std::string message;                             ///< 失败原因 / 备注
};

}
}

#endif // SBT_ROI_RESULT_H_
