/**
 * @file ROIResultManager.h
 * @brief ROI 结果封装（00 §5 / 01 §7）
 * @details 收集各 ROI 结果并组装整批 ROIManagerResult。
 */

#ifndef SBT_ROI_RESULT_MANAGER_H_
#define SBT_ROI_RESULT_MANAGER_H_

#include <vector>

#include "Export.h"
#include "ROIManagerResult.h"

namespace sbt {
namespace proc {

/**
 * @brief ROI 结果管理器
 */
class SBT_OPS_API ROIResultManager {
public:
    ROIResultManager();
    ~ROIResultManager();

    /** @brief 收集整批 ROI 结果 */
    void collect(std::vector<ROIResult> results);

    /** @brief 组装 ROIManagerResult（整批 success = 全部 ROI success 且非空） */
    ROIManagerResult snapshot() const;

    /** @brief 清空 */
    void clear();

private:
    std::vector<ROIResult> results_;
};

}
}

#endif // SBT_ROI_RESULT_MANAGER_H_
