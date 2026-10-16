/**
 * @file OpConfig.h
 * @brief 算子层配置（L2，13 §5.1 / 08 §5）
 */

#ifndef SBT_OP_CONFIG_H_
#define SBT_OP_CONFIG_H_

#include <string>

namespace sbt {
namespace proc {

/** @brief 平台下发的算子配置 */
struct OpConfig {
    std::string opName;                 ///< "PointCloudROIManager"
    std::string configPath;             ///< roi_config.json 路径（必填）
    float octreeResolutionMM = 0.0f;    ///< 覆盖 L3 分辨率；0 = 用 L3 默认或内置 5.0
    int threadCount = 0;                ///< 0 = 硬件并发（v1 预留）
    bool hotReload = false;             ///< 是否开启热重载（v1 预留）
};

}
}

#endif // SBT_OP_CONFIG_H_
