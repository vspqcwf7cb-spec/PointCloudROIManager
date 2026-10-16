/**
 * @file ROIParser.cpp
 * @brief ROIParser 实现（04 模块）
 * @details 唯一 include <json.hpp> 的位置。解析新 schema：
 *          { "coordinate"?, "octreeResolutionMM"?, "rois": [ {name, type?, min:{x,y,z}, max:{x,y,z}} ] }
 */

#include "ROIParser.h"

#include <json.hpp>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

#include "ROIConfig.h"
#include "ROIRegion.h"

namespace sbt {
namespace proc {

namespace {

// 去掉 UTF-8 BOM（04 §10：带 BOM 的 JSON 能解析）
std::string stripBom(std::string text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return text;
}

// type 字符串 → 枚举；未知值通过 errOut 报错
ROIType parseType(const std::string& s, std::string& errOut) {
    if (s.empty() || s == "box") return ROIType::Box;
    if (s == "cylinder") return ROIType::Cylinder;
    if (s == "sphere") return ROIType::Sphere;
    if (s == "polygon") return ROIType::Polygon;
    if (s == "path") return ROIType::Path;
    errOut = "type: unknown value, got '" + s + "'";
    return ROIType::Box;
}

// 解析 x/y/z 三元组，统一 NaN / 类型错误（04 §7）
bool parseVec3(const nlohmann::json& item, const char* key, const std::string& tag,
               Eigen::Vector3f& out, std::string& errOut) {
    if (!item.is_object() || !item.contains(key) || !item[key].is_object()) {
        errOut = tag + "." + key + " must be object {x,y,z}";
        return false;
    }
    const auto& v = item[key];
    const char* axes[3] = {"x", "y", "z"};
    for (int i = 0; i < 3; ++i) {
        if (!v.contains(axes[i])) {
            errOut = tag + "." + key + "." + axes[i] + ": missing";
            return false;
        }
        if (!v[axes[i]].is_number()) {
            errOut = tag + "." + key + "." + axes[i] + " must be number, got " +
                     std::string(v[axes[i]].type_name());
            return false;
        }
        const float val = v[axes[i]].get<float>();
        if (!std::isfinite(val)) {
            errOut = tag + "." + key + "." + axes[i] + " is not finite";
            return false;
        }
        out[i] = val;
    }
    return true;
}

// 单个 ROI 解析（04 §8）
bool parseRoiItem(const nlohmann::json& item, std::size_t idx, ROIRegion& out,
                  std::string& errOut) {
    const std::string tag = "rois[" + std::to_string(idx) + "]";

    if (!item.is_object() || !item.contains("name")) {
        errOut = tag + ": missing \"name\"";
        return false;
    }
    if (!item["name"].is_string()) {
        errOut = tag + ".name must be string";
        return false;
    }
    out.name = item["name"].get<std::string>();

    if (item.contains("type")) {
        if (!item["type"].is_string()) {
            errOut = tag + ".type must be string";
            return false;
        }
        std::string typeErr;
        out.type = parseType(item["type"].get<std::string>(), typeErr);
        if (!typeErr.empty()) {
            errOut = tag + "." + typeErr;
            return false;
        }
    } else {
        out.type = ROIType::Box;
    }

    if (!parseVec3(item, "min", tag, out.min, errOut)) return false;
    if (!parseVec3(item, "max", tag, out.max, errOut)) return false;
    return true;
}

}  // namespace

ROIParser::ROIParser() = default;
ROIParser::~ROIParser() = default;

bool ROIParser::parseFile(const std::string& path, ROIConfig& out, std::string& errOut) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        errOut = "cannot open: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return parseString(ss.str(), out, errOut);
}

bool ROIParser::parseString(const std::string& text, ROIConfig& out, std::string& errOut) {
    out = ROIConfig();

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(stripBom(text), nullptr, true);
    } catch (const nlohmann::json::parse_error& e) {
        errOut = "json parse error: " + std::string(e.what()) + " at byte " +
                 std::to_string(e.byte);
        return false;
    } catch (const std::exception& e) {
        errOut = "json parse error: " + std::string(e.what());
        return false;
    }

    if (!root.is_object()) {
        errOut = "root must be object";
        return false;
    }

    // coordinate 可选，缺省 "world"（04 §5）
    if (root.contains("coordinate")) {
        if (!root["coordinate"].is_string()) {
            errOut = "coordinate must be string";
            return false;
        }
        out.coordinate = root["coordinate"].get<std::string>();
    } else {
        out.coordinate = "world";
    }

    // octreeResolutionMM 可选，缺省 5.0（13 §5.2）
    if (root.contains("octreeResolutionMM")) {
        if (!root["octreeResolutionMM"].is_number()) {
            errOut = "octreeResolutionMM must be number";
            return false;
        }
        const float v = root["octreeResolutionMM"].get<float>();
        if (!std::isfinite(v) || v <= 0.0f) {
            errOut = "octreeResolutionMM must be > 0";
            return false;
        }
        out.octreeResolutionMM = v;
    }

    // rois 必填且为数组（04 §5）
    if (!root.contains("rois") || !root["rois"].is_array()) {
        errOut = "missing \"rois\" array";
        return false;
    }

    const auto& rois = root["rois"];
    out.regions.reserve(rois.size());
    for (std::size_t i = 0; i < rois.size(); ++i) {
        ROIRegion region;
        if (!parseRoiItem(rois[i], i, region, errOut)) {
            return false;
        }
        out.regions.push_back(std::move(region));
    }
    return true;
}

}  // namespace proc
}  // namespace sbt
