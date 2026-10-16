/**
 * @file PointCloudROIManagerOperator.cpp
 * @brief PointCloudROIManagerOperator 实现（08 模块）
 * @details 只做类型转发与错误传递，业务逻辑全部委托 PointCloudROIManager。
 */

#include "PointCloudROIManagerOperator.h"

#include <memory>
#include <vector>

#include "PointCloudData.h"
#include "PointCloudROIManager.h"
#include "ROIManagerResult.h"

namespace sbt {
namespace proc {

PointCloudROIManagerOperator::PointCloudROIManagerOperator() = default;
PointCloudROIManagerOperator::~PointCloudROIManagerOperator() = default;

bool PointCloudROIManagerOperator::init(const OpConfig& config) {
    if (config.configPath.empty()) {
        return false;
    }
    opConfig_ = config;

    manager_ = std::make_unique<PointCloudROIManager>();
    if (opConfig_.octreeResolutionMM > 0.0f) {
        manager_->setOctreeResolution(opConfig_.octreeResolutionMM);  // L2 覆盖 L3
    }
    if (!manager_->init(config.configPath)) {
        manager_.reset();
        return false;
    }
    return true;
}

bool PointCloudROIManagerOperator::execute(const DataBag& input, DataBag& output) {
    std::shared_ptr<const PointCloudData> cloud;
    if (!input.get("input_cloud", cloud) || !cloud) {
        output.setError("missing input_cloud");  // OperatorMissingBagKey（4001）
        return false;
    }
    if (!manager_) {
        output.setError("operator not initialized");
        return false;
    }

    if (!manager_->execute(cloud)) {
        output.setError(manager_->getLastError());
        return false;
    }

    const ROIManagerResult& result = manager_->getResult();
    output.set("roi_result", result);

    // roi_clouds：按 ROI 顺序的裁剪点云（P0 修正后为 shared_ptr<const PointCloudData>）
    std::vector<std::shared_ptr<const PointCloudData>> roiClouds;
    roiClouds.reserve(result.results.size());
    for (const auto& r : result.results) {
        roiClouds.push_back(r.cloud);
    }
    output.set("roi_clouds", roiClouds);

    return result.success;
}

void PointCloudROIManagerOperator::release() { manager_.reset(); }

ManagerState PointCloudROIManagerOperator::state() const {
    return manager_ ? manager_->getState() : ManagerState::Uninitialized;
}

}  // namespace proc
}  // namespace sbt

// 平台工厂函数（08 §11）—— extern "C" 边界，所有权在库内创建 / 销毁
sbt::proc::IOperator* CreateROIManagerOperator() {
    return new sbt::proc::PointCloudROIManagerOperator();
}

void DestroyOperator(sbt::proc::IOperator* op) {
    delete op;
}
