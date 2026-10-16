/**
 * @file ROIParser.h
 * @brief JSON → ROIConfig 解析器（04 模块）
 * @details 只做格式解析，不做业务校验（校验由 ROIConfigManager 负责）。
 *          强制约束：nlohmann/json 只在本模块 .cpp 内 include，头文件不暴露。
 */

#ifndef SBT_ROI_PARSER_H_
#define SBT_ROI_PARSER_H_

#include <string>

#include "Export.h"
#include "ROIConfig.h"

namespace sbt {
namespace proc {

/**
 * @brief ROI 配置解析器（v1 仅 JSON）
 * @details parseRoiItem / parseVec3 以 .cpp 内匿名命名空间自由函数实现，
 *          避免 nlohmann::json 类型泄漏进公共头（04 §11）。
 */
class SBT_OPS_API ROIParser {
public:
    ROIParser();
    ~ROIParser();

    /**
     * @brief 读取文件并解析为 ROIConfig
     * @param path [in] .json 文件路径
     * @param out [out] 解析结果
     * @param errOut [out] 含字段名 / 字节偏移的结构化错误
     * @return true=成功
     */
    bool parseFile(const std::string& path, ROIConfig& out, std::string& errOut);

    /**
     * @brief 解析内存 JSON 文本（便于单测，无需落盘）
     */
    bool parseString(const std::string& text, ROIConfig& out, std::string& errOut);
};

}
}

#endif // SBT_ROI_PARSER_H_
