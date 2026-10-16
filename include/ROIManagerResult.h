/**
 * @file ROIManagerResult.h
 * @brief 整批输出结果（02 §5.7）
 */

#ifndef SBT_ROI_MANAGER_RESULT_H_
#define SBT_ROI_MANAGER_RESULT_H_

#include <string>
#include <vector>

#include "ROIResult.h"

namespace sbt {
namespace proc {

/** @brief 整批 ROI 裁剪输出 */
struct ROIManagerResult {
    bool success = false;              ///< 整批是否可交付
    std::vector<ROIResult> results;    ///< 各 ROI 结果
    std::string error;                 ///< 整批级错误（配置读不到 / 点云为空）
};

}
}

#endif // SBT_ROI_MANAGER_RESULT_H_
