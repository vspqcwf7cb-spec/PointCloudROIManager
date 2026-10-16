/**
 * @file RoiLogger.cpp
 * @brief RoiLogger 实现（12 §6 / §8）
 * @details v1 将结构化日志输出为 JSON 行到 stderr（不依赖 spdlog）。日志后端写盘失败被吞掉，不影响主流程。
 *          错误码 → 人类可读映射表集中维护在本文件（12 §7）。
 */

#include "RoiLogger.h"

#include <cstdio>
#include <ctime>
#include <iostream>

namespace sbt {
namespace proc {

namespace {

const char* errorCodeName(RoiErrorCode code) {
    switch (code) {
        case RoiErrorCode::Ok: return "Ok";
        case RoiErrorCode::ConfigFileNotFound: return "ConfigFileNotFound";
        case RoiErrorCode::ConfigJsonSyntaxError: return "ConfigJsonSyntaxError";
        case RoiErrorCode::ConfigMissingRois: return "ConfigMissingRois";
        case RoiErrorCode::ConfigDuplicateName: return "ConfigDuplicateName";
        case RoiErrorCode::ConfigCoordinateEmpty: return "ConfigCoordinateEmpty";
        case RoiErrorCode::ConfigInvalidBox: return "ConfigInvalidBox";
        case RoiErrorCode::InputCloudEmpty: return "InputCloudEmpty";
        case RoiErrorCode::InputCoordinateMismatch: return "InputCoordinateMismatch";
        case RoiErrorCode::InputCloudOOM: return "InputCloudOOM";
        case RoiErrorCode::ExtractorNotBuilt: return "ExtractorNotBuilt";
        case RoiErrorCode::ExtractorOctreeFail: return "ExtractorOctreeFail";
        case RoiErrorCode::OperatorMissingBagKey: return "OperatorMissingBagKey";
        case RoiErrorCode::OperatorTypeMismatch: return "OperatorTypeMismatch";
    }
    return "UnknownError";
}

const char* warnCodeName(RoiWarnCode code) {
    switch (code) {
        case RoiWarnCode::NoHit: return "NoHit";
        case RoiWarnCode::ConfigFieldDefaulted: return "ConfigFieldDefaulted";
        case RoiWarnCode::CoordinateSkipped: return "CoordinateSkipped";
        case RoiWarnCode::HotReloadFailed: return "HotReloadFailed";
    }
    return "UnknownWarn";
}

// 输出 ISO8601 UTC 毫秒时间戳（12 §8 示例格式）
std::string formatIso8601(const std::chrono::system_clock::time_point& tp) {
    const auto msTotal = std::chrono::duration_cast<std::chrono::milliseconds>(
                             tp.time_since_epoch())
                             .count();
    const std::time_t sec = static_cast<std::time_t>(msTotal / 1000);
    const int milli = static_cast<int>(msTotal % 1000);

    std::tm tmUtc{};
#ifdef _WIN32
    gmtime_s(&tmUtc, &sec);
#else
    gmtime_r(&sec, &tmUtc);
#endif
    char buf[32] = {0};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmUtc);

    char out[40] = {0};
    std::snprintf(out, sizeof(out), "%s.%03dZ", buf, milli);
    return std::string(out);
}

// 字段顺序固定：ts / station / frame / level / code / roi / msg（12 §11）
void emit(const char* level, int code, const char* codeName, const std::string& station,
          std::uint64_t frame, const std::string& msg) {
    try {
        std::string full = msg;
        if (codeName[0] != '\0') {
            full = std::string("[") + codeName + "] " + msg;
        }
        std::string json =
            "{\"ts\":\"" + formatIso8601(std::chrono::system_clock::now()) +
            "\",\"station\":\"" + station + "\",\"frame\":" + std::to_string(frame) +
            ",\"level\":\"" + level + "\",\"code\":" + std::to_string(code) +
            ",\"roi\":null,\"msg\":\"" + full + "\"}";
        std::cerr << json << std::endl;
    } catch (...) {
        // 日志后端失败吞掉，降级不输出，不影响主流程（12 §9）
    }
}

}  // namespace

RoiLogger& RoiLogger::instance() {
    static RoiLogger logger;
    return logger;
}

void RoiLogger::setStationId(const std::string& id) { stationId_ = id; }
void RoiLogger::setFrameId(std::uint64_t id) { frameId_ = id; }

void RoiLogger::info(const std::string& msg) {
    emit("INFO", 0, "", stationId_, frameId_, msg);
}

void RoiLogger::warn(RoiWarnCode code, const std::string& msg) {
    emit("WARN", static_cast<int>(code), warnCodeName(code), stationId_, frameId_, msg);
}

void RoiLogger::error(RoiErrorCode code, const std::string& msg) {
    emit("ERROR", static_cast<int>(code), errorCodeName(code), stationId_, frameId_, msg);
}

}  // namespace proc
}  // namespace sbt
