/**
 * @file ROIConfigManager.h
 * @brief ROI 配置生命周期管理（03 模块）
 * @details 配置唯一持有点：加载、校验、缓存、热重载、按名反查。
 */

#ifndef SBT_ROI_CONFIG_MANAGER_H_
#define SBT_ROI_CONFIG_MANAGER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "Export.h"
#include "ROIConfig.h"

namespace sbt {
namespace proc {

class ROIParser;

/**
 * @brief 配置运行时状态（03 §5）
 */
struct ConfigStatus {
    bool loaded = false;               ///< 是否已加载成功
    std::string path;                  ///< 配置文件路径
    std::uint64_t lastModifiedMs = 0;  ///< 文件 mtime，单位 ms
    std::size_t roiCount = 0;          ///< ROI 数量
    std::string coordinate;            ///< 坐标系
};

/**
 * @brief ROI 配置管理器
 */
class SBT_OPS_API ROIConfigManager {
public:
    ROIConfigManager();
    ~ROIConfigManager();

    /**
     * @brief 一次性加载 + 校验 + 落缓存
     * @param path [in] 配置文件路径
     * @param errOut [out] 失败原因
     * @return true=成功；失败不改动已缓存配置（事务语义，供热重载回退）
     */
    bool load(const std::string& path, std::string& errOut);

    /**
     * @brief 基于文件 mtime 增量重载
     * @param errOut [out] 失败原因
     * @return true=未变化或重载成功；重载失败返回 false 且保留旧配置
     */
    bool reloadIfChanged(std::string& errOut);

    /** @brief 当前配置只读访问 */
    const ROIConfig& getConfig() const;

    /** @brief 运行时状态只读访问 */
    const ConfigStatus& getStatus() const;

    /** @brief 按 name 查找 ROI，找不到返回 nullptr */
    const ROIRegion* findByName(const std::string& name) const;

private:
    bool validateConfig(const ROIConfig& cfg, std::string& errOut);

    std::unique_ptr<ROIParser> parser_;
    ROIConfig cachedConfig_;
    ConfigStatus status_;
};

}
}

#endif // SBT_ROI_CONFIG_MANAGER_H_
