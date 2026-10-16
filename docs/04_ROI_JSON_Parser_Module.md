# 04 — ROI JSON 解析模块（ROIParser）

- 版本：v1.0
- 对应实现：`include/ROIParser.h` + `src/ROIParser.cpp`

---

## 1. 模块目标

把 JSON 文件转换为平台无关的 `ROIConfig` 对象。**只做格式解析，不做业务校验**——合法性由 ROIConfigManager 负责。

## 2. 工业应用场景

- 现场工程师用任意文本编辑器写 JSON 即可定义 ROI
- 与标注软件（如 RViz / 自研标定工具）导出的 JSON 对齐
- 单元测试中直接构造 JSON 字符串，无需落盘

## 3. 架构位置

```
ROIConfigManager
      │ 调用 parse
      ▼
   ROIParser ──▶ nlohmann/json（仅本模块 include）
      │
      ▼
   ROIConfig（返回给上层）
```

**强制约束**：除 ROIParser.cpp 外，其他模块不得 `#include <json.hpp>`。

## 4. 输入输出定义

| 方向 | 类型 | 说明 |
| --- | --- | --- |
| 输入 | 文件路径 | `.json` / `.yaml`（v1 仅 JSON） |
| 输入 | 字符串 | 测试用，直接解析 JSON 文本 |
| 输出 | `ROIConfig` | 解析后的内存对象 |
| 输出 | `bool ok` | 解析是否成功 |
| 输出 | `std::string errOut` | 含行号 / 字段名的错误 |

## 5. 数据结构

输入 JSON schema（v1）：

```json
{
  "coordinate": "robot",
  "rois": [
    {
      "name": "door_glue",
      "type": "box",
      "min": { "x": 100.0, "y": 200.0, "z": 300.0 },
      "max": { "x": 150.0, "y": 260.0, "z": 350.0 }
    }
  ]
}
```

字段约定：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| coordinate | string | 否 | 缺省为 "world" |
| rois | array | 是 | 至少 1 个元素 |
| rois[].name | string | 是 | 唯一标签 |
| rois[].type | string | 否 | 缺省 "box" |
| rois[].min / max | object | 是 | 含 x / y / z，数值 |

## 6. 类设计

```cpp
class ROIParser {
public:
    ROIParser();
    ~ROIParser();

    bool parseFile(const std::string& path, ROIConfig& out, std::string& errOut);
    bool parseString(const std::string& text, ROIConfig& out, std::string& errOut);

private:
    bool parseRoiItem(const nlohmann::json& item, ROIRegion& out, std::string& errOut);
    bool parseVec3(const nlohmann::json& obj, const char* key, Eigen::Vector3f& out, std::string& errOut);
};
```

## 7. 接口设计

| 接口 | 说明 |
| --- | --- |
| `parseFile` | 读盘 + 解析 |
| `parseString` | 内存解析，便于测试 |
| `parseRoiItem` | 单 ROI 解析，内部复用 |
| `parseVec3` | 数值三元组解析，统一 NaN / 类型错误 |

## 8. 算法流程

```
parseFile(path)
  └── read text via std::ifstream (binary mode)
      └── parseString(text)

parseString(text)
  ├── nlohmann::json::parse(text)  异常 → errOut 含 what()
  ├── root["coordinate"] 可空
  ├── 检查 root["rois"].is_array()
  ├── for each item:
  │     ├── name 必填且为 string
  │     ├── type → ROIType 枚举映射
  │     ├── min / max → parseVec3
  │     └── 组装 ROIRegion 推入 out.regions
  └── 完成
```

## 9. 异常处理

| 错误场景 | 错误信息模板 |
| --- | --- |
| 文件读不到 | `cannot open: <path>` |
| JSON 语法错 | `json parse error: <msg> at byte <offset>` |
| 缺 rois 数组 | `missing "rois" array` |
| 单个 ROI 缺 name | `rois[i]: missing "name"` |
| name 不是字符串 | `rois[i].name must be string` |
| min / max 不是对象 | `rois[i].min must be object {x,y,z}` |
| 数值为字符串 | `rois[i].min.x must be number, got string` |
| NaN / Inf | `rois[i].min.x is not finite` |

## 10. 测试方案

| 用例 | 输入 | 期望 |
| --- | --- | --- |
| 完整合法 | 2 个 ROI | ok=true，regions 大小 2 |
| 缺 coordinate | 无 coordinate | coordinate="world"（默认） |
| 缺 type | 无 type | type=Box（默认） |
| 语法错 | `{ "rois": [ }` | ok=false，含 byte offset |
| name 缺失 | `{ "name": 123 }` | ok=false |
| min.x 为字符串 | `"x": "100"` | ok=false |
| 空文件 | "" | ok=false |
| 注释 / BOM | 带 UTF-8 BOM 的 JSON | 能解析 |

性能：10 个 ROI 配置 < 2 ms。

## 11. AI 实现要求

- `#include <json.hpp>` 只能出现在 `ROIParser.cpp`
- 禁止在头文件前置声明 nlohmann 类型
- 错误字符串必须结构化（字段名 + 期望值 + 实际值），便于现场定位
- 用 `nlohmann::json::parse(text, nullptr, true)` 开启异常模式，再 try/catch 转字符串
- 数值用 `item["x"].get<float>()`，不要用 `item["x"] == 100` 这种宽松判断

## 12. 后续扩展

- 支持 YAML（派生 `ROIYamlParser`，Manager 按扩展名分发）
- 支持注释（JSON5）
- 支持 include 其他配置文件（多车型分文件）
