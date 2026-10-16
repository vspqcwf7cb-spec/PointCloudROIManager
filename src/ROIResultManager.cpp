/**
 * @file ROIResultManager.cpp
 * @brief ROIResultManager 实现
 */

#include "ROIResultManager.h"

namespace sbt {
namespace proc {

ROIResultManager::ROIResultManager() = default;
ROIResultManager::~ROIResultManager() = default;

void ROIResultManager::collect(std::vector<ROIResult> results) {
    results_ = std::move(results);
}

ROIManagerResult ROIResultManager::snapshot() const {
    ROIManagerResult r;
    r.results = results_;
    r.success = !results_.empty();
    for (const auto& res : results_) {
        if (!res.success) {
            r.success = false;
            break;
        }
    }
    return r;
}

void ROIResultManager::clear() { results_.clear(); }

}  // namespace proc
}  // namespace sbt
