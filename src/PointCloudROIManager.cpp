/**
 * @file PointCloudROIManager.cpp
 * @brief PointCloudROIManager 实现（07 模块）
 * @details 调度 ConfigManager / Extractor / ResultManager；状态机 + 错误聚合；execute 用互斥锁串行化。
 */

#include "PointCloudROIManager.h"

#include <mutex>
#include <utility>

#include "PointCloudData.h"
#include "ROIConfig.h"
#include "ROIConfigManager.h"
#include "ROIExtractor.h"
#include "ROIRegion.h"
#include "ROIResultManager.h"
#include "RoiLogger.h"

namespace sbt {
namespace proc {

PointCloudROIManager::PointCloudROIManager()
    : configManager_(std::make_unique<ROIConfigManager>()),
      resultManager_(std::make_unique<ROIResultManager>()) {}

PointCloudROIManager::~PointCloudROIManager() = default;

bool PointCloudROIManager::init(const std::string& configPath) {
    std::string err;
    if (!configManager_->load(configPath, err)) {
        state_ = ManagerState::Failed;
        lastError_ = err;
        lastResult_ = ROIManagerResult();
        lastResult_.error = err;
        return false;
    }

    float resolution = configManager_->getConfig().octreeResolutionMM;
    if (resolutionOverrideMM_ > 0.0f) {
        resolution = resolutionOverrideMM_;  // L2 覆盖 L3（13 §8）
    }
    extractor_ = std::make_unique<BoxROIExtractor>(resolution);

    state_ = ManagerState::Ready;
    lastError_.clear();
    return true;
}

bool PointCloudROIManager::execute(std::shared_ptr<const PointCloudData> cloud) {
    std::lock_guard<std::mutex> lock(executeMutex_);

    // 前置检查：状态 + 点云非空（07 §8）
    if (state_ != ManagerState::Ready) {
        lastError_ = "execute: state not Ready";
        lastResult_ = ROIManagerResult();
        lastResult_.error = lastError_;
        state_ = ManagerState::Failed;
        return false;
    }
    if (!cloud || cloud->points.empty()) {
        lastError_ = "input cloud is empty";
        lastResult_ = ROIManagerResult();
        lastResult_.error = lastError_;
        state_ = ManagerState::Failed;
        RoiLogger::instance().error(RoiErrorCode::InputCloudEmpty, lastError_);
        return false;
    }

    // 坐标系校验：双方非空且不一致 → 2002；任一方为空 → 跳过并打 warn（07 §8）
    const ROIConfig& cfg = configManager_->getConfig();
    if (cfg.coordinate.empty() || cloud->coordinate.empty()) {
        RoiLogger::instance().warn(RoiWarnCode::CoordinateSkipped,
                                   "coordinate empty, skip check");
    } else {
        std::string coordErr;
        if (!isCoordinateMatched(cfg, *cloud, coordErr)) {
            lastError_ = coordErr;
            lastResult_ = ROIManagerResult();
            lastResult_.error = lastError_;
            state_ = ManagerState::Failed;
            RoiLogger::instance().error(RoiErrorCode::InputCoordinateMismatch, lastError_);
            return false;
        }
    }

    state_ = ManagerState::Executing;

    if (!extractor_->build(cloud)) {
        lastError_ = "octree build failed: empty cloud or oom";
        lastResult_ = ROIManagerResult();
        lastResult_.error = lastError_;
        state_ = ManagerState::Failed;
        RoiLogger::instance().error(RoiErrorCode::ExtractorOctreeFail, lastError_);
        return false;
    }

    std::vector<ROIResult> results = extractor_->extractAll(cfg.regions);

    // 单 ROI 0 命中打 warn（NoHit），不在 extract 内打日志（06 §11）
    for (const auto& r : results) {
        if (r.success && r.pointCount == 0) {
            RoiLogger::instance().warn(RoiWarnCode::NoHit, "roi \"" + r.name + "\": no hit");
        }
    }

    resultManager_->collect(std::move(results));
    lastResult_ = resultManager_->snapshot();
    state_ = ManagerState::Done;
    return true;
}

const ROIManagerResult& PointCloudROIManager::getResult() const { return lastResult_; }

ManagerState PointCloudROIManager::getState() const { return state_; }

std::string PointCloudROIManager::getLastError() const { return lastError_; }

void PointCloudROIManager::setOctreeResolution(float mm) { resolutionOverrideMM_ = mm; }

}  // namespace proc
}  // namespace sbt
