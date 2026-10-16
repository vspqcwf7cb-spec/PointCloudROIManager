/**
 * @file ErrorCode.h
 * @brief 错误码 / 警告码定义（12 §5.1 / §5.2）
 * @details 错误码用于整帧失败或不可恢复异常；可恢复业务标记走 RoiWarnCode，MES 只对 ERROR 报警。
 *          区段：Config 1xxx / Cloud 2xxx / Extractor 3xxx / Operator 4xxx（新增不跨段）。
 */

#ifndef SBT_ERROR_CODE_H_
#define SBT_ERROR_CODE_H_

namespace sbt {
namespace proc {

/** @brief 致命 / 整帧失败错误码 */
enum class RoiErrorCode : int {
    Ok = 0,

    // Config / Parser 1xxx
    ConfigFileNotFound   = 1001,
    ConfigJsonSyntaxError = 1002,
    ConfigMissingRois    = 1003,
    ConfigDuplicateName  = 1004,
    ConfigCoordinateEmpty = 1005,
    ConfigInvalidBox     = 1006,

    // Cloud / Input 2xxx
    InputCloudEmpty      = 2001,
    InputCoordinateMismatch = 2002,
    InputCloudOOM        = 2003,

    // Extractor 3xxx
    ExtractorNotBuilt    = 3001,
    ExtractorOctreeFail  = 3002,

    // Operator 4xxx
    OperatorMissingBagKey = 4001,
    OperatorTypeMismatch  = 4002,
};

/** @brief 可恢复 / 业务标记警告码（不触发 MES 报警） */
enum class RoiWarnCode : int {
    NoHit               = 3003,   // 单 ROI 0 命中，不算错误
    ConfigFieldDefaulted = 1101,  // 配置缺字段，使用默认值
    CoordinateSkipped   = 2101,   // coordinate 为空，跳过一致性校验
    HotReloadFailed     = 1102,   // 热重载失败，沿用旧配置
};

}
}

#endif // SBT_ERROR_CODE_H_
