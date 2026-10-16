/**
 * @file RoiLogger.h
 * @brief 统一日志出口（12 §6）
 * @details 模块内只产生结构化事件，不直接 printf / std::cout。v1 输出 JSON 行到 stderr，不引入 spdlog。
 */

#ifndef SBT_ROI_LOGGER_H_
#define SBT_ROI_LOGGER_H_

#include <chrono>
#include <cstdint>
#include <string>

#include "ErrorCode.h"
#include "Export.h"

namespace sbt {
namespace proc {

/**
 * @brief 日志事件（12 §5.2）
 */
struct LogEvent {
    std::uint64_t frameId = 0;                      ///< 帧号
    std::string stationId;                          ///< 工位
    std::string roiName;                            ///< 可选，ROI 名
    RoiErrorCode code = RoiErrorCode::Ok;
    std::string message;
    std::chrono::system_clock::time_point ts;       ///< 时间戳
};

/**
 * @brief 进程级日志单例
 */
class SBT_OPS_API RoiLogger {
public:
    static RoiLogger& instance();

    /** @brief 设置工位 ID（注入到每条日志） */
    void setStationId(const std::string& id);

    /** @brief 设置当前帧号（注入到每条日志） */
    void setFrameId(std::uint64_t id);

    /** @brief 正常流程打点 */
    void info(const std::string& msg);

    /** @brief 可恢复异常（单 ROI 0 命中 / 配置缺字段用默认值） */
    void warn(RoiWarnCode code, const std::string& msg);

    /** @brief 整帧失败，需 MES 报警 */
    void error(RoiErrorCode code, const std::string& msg);

    RoiLogger(const RoiLogger&) = delete;
    RoiLogger& operator=(const RoiLogger&) = delete;

private:
    RoiLogger() = default;

    std::string stationId_;
    std::uint64_t frameId_ = 0;
};

}
}

#endif // SBT_ROI_LOGGER_H_
