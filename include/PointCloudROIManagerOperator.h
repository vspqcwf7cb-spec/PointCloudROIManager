/**
 * @file PointCloudROIManagerOperator.h
 * @brief Operator 封装（08 模块）
 * @details 把 PointCloudROIManager 适配为平台 Operator；只转发，不做业务逻辑。
 */

#ifndef SBT_POINT_CLOUD_ROI_MANAGER_OPERATOR_H_
#define SBT_POINT_CLOUD_ROI_MANAGER_OPERATOR_H_

#include <memory>
#include <string>

#include "Export.h"
#include "IOperator.h"

namespace sbt {
namespace proc {

class PointCloudROIManager;

/**
 * @brief 平台 Operator 适配器
 */
class SBT_OPS_API PointCloudROIManagerOperator : public IOperator {
public:
    PointCloudROIManagerOperator();
    ~PointCloudROIManagerOperator() override;

    bool init(const OpConfig& config) override;
    bool execute(const DataBag& input, DataBag& output) override;
    void release() override;

    std::string name() const override { return "PointCloudROIManager"; }
    ManagerState state() const override;

private:
    std::unique_ptr<PointCloudROIManager> manager_;
    OpConfig opConfig_;
};

}
}

// 平台工厂函数（08 §11）—— 跨 DLL 边界的唯一裸 new/delete（平台约定，所有权同库内转移）
extern "C" SBT_OPS_API sbt::proc::IOperator* CreateROIManagerOperator();
extern "C" SBT_OPS_API void DestroyOperator(sbt::proc::IOperator* op);

#endif // SBT_POINT_CLOUD_ROI_MANAGER_OPERATOR_H_
