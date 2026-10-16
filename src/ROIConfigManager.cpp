/**
 * @file ROIConfigManager.cpp
 * @brief ROIConfigManager 实现（03 模块）
 * @details 配置加载 / 校验 / 缓存 / 热重载（事务语义：失败不改动已缓存配置）。
 */

#include "ROIConfigManager.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_set>

#include "ROIConfig.h"
#include "ROIParser.h"
#include "ROIRegion.h"
#include "RoiLogger.h"

namespace sbt {
namespace proc {

namespace {

// 文件 mtime（单位 ms）；失败返回 ok=false
std::uint64_t fileMtimeMs(const std::string& path, bool& ok) {
    std::error_code ec;
    const auto ftime = std::filesystem::last_write_time(path, ec);
    if (ec) {
        ok = false;
        return 0;
    }
    ok = true;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(ftime.time_since_epoch()).count());
}

}  // namespace

ROIConfigManager::ROIConfigManager() : parser_(std::make_unique<ROIParser>()) {}
ROIConfigManager::~ROIConfigManager() = default;

bool ROIConfigManager::load(const std::string& path, std::string& errOut) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        errOut = "config file not found: " + path;
        RoiLogger::instance().error(RoiErrorCode::ConfigFileNotFound, errOut);
        return false;
    }

    ROIConfig cfg;
    if (!parser_->parseFile(path, cfg, errOut)) {
        RoiLogger::instance().error(RoiErrorCode::ConfigJsonSyntaxError, errOut);
        return false;
    }
    if (!validateConfig(cfg, errOut)) {
        return false;  // 校验失败，不改动缓存
    }

    // 全部成功后才提交（事务语义，保证热重载失败可回退）
    cachedConfig_ = std::move(cfg);

    status_ = ConfigStatus();
    status_.path = path;
    status_.loaded = true;
    status_.roiCount = cachedConfig_.regions.size();
    status_.coordinate = cachedConfig_.coordinate;
    bool ok = false;
    status_.lastModifiedMs = fileMtimeMs(path, ok);
    return true;
}

bool ROIConfigManager::validateConfig(const ROIConfig& cfg, std::string& errOut) {
    if (cfg.coordinate.empty()) {
        errOut = "coordinate is empty";
        RoiLogger::instance().error(RoiErrorCode::ConfigCoordinateEmpty, errOut);
        return false;
    }
    if (cfg.regions.empty()) {
        errOut = "rois is empty";
        RoiLogger::instance().error(RoiErrorCode::ConfigMissingRois, errOut);
        return false;
    }

    std::unordered_set<std::string> names;
    names.reserve(cfg.regions.size());
    for (const auto& region : cfg.regions) {
        std::string reason;
        if (!isValidBoxRegion(region, reason)) {
            errOut = "roi \"" + region.name + "\": " + reason;
            RoiLogger::instance().error(RoiErrorCode::ConfigInvalidBox, errOut);
            return false;
        }
        if (!names.insert(region.name).second) {
            errOut = "duplicate roi name: " + region.name;
            RoiLogger::instance().error(RoiErrorCode::ConfigDuplicateName, errOut);
            return false;
        }
    }
    return true;
}

bool ROIConfigManager::reloadIfChanged(std::string& errOut) {
    if (!status_.loaded) {
        errOut = "reload before load";
        return false;
    }
    bool ok = false;
    const std::uint64_t mtime = fileMtimeMs(status_.path, ok);
    if (!ok || mtime == status_.lastModifiedMs) {
        return true;  // 未变化，直接返回
    }

    if (!load(status_.path, errOut)) {
        RoiLogger::instance().warn(RoiWarnCode::HotReloadFailed, errOut);
        return false;  // 旧配置保留（load 事务语义）
    }
    RoiLogger::instance().info("config reloaded: roiCount=" + std::to_string(status_.roiCount));
    return true;
}

const ROIConfig& ROIConfigManager::getConfig() const { return cachedConfig_; }

const ConfigStatus& ROIConfigManager::getStatus() const { return status_; }

const ROIRegion* ROIConfigManager::findByName(const std::string& name) const {
    for (const auto& region : cachedConfig_.regions) {
        if (region.name == name) {
            return &region;
        }
    }
    return nullptr;
}

}  // namespace proc
}  // namespace sbt
