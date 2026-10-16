# 03 — ROI 配置模块（ROIConfigManager）

- 版本：v1.0
- 对应实现：`include/ROIConfigManager.h` + `src/ROIConfigManager.cpp`

---

## 1. 模块目标

对 ROI 配置文件做生命周期管理：加载、校验、缓存、版本查询。它是配置的唯一持有点，ROIParser 只负责"解析成对象"，ROIConfigManager 负责"对象是否可用"。

## 2. 工业应用场景

- 产线换型：同一台车的多个车型配置切换，Manager 缓存已加载配置，避免反复读盘
- 现场调试：工程师修改 JSON 后热重载，不重启进程
- 多任务并行：不同工位持有各自的 Manager 实例，配置互不干扰

## 3. 架构位置

```
PointCloudROIManager
    └── owns ──▶ ROIConfigManager
                       │ 调用
                       ▼
                    ROIParser
                       │ 返回
                       ▼
                    ROIConfig (缓存)
```

Manager 不直接持有 ROIParser，而是通过 ROIConfigManager 间接使用。

## 4. 输入输出定义

| 方向 | 类型 | 说明 |
| --- | --- | --- |
| 输入 | `std::string configPath` | 配置文件绝对路径 |
| 输入 | `bool hotReload` | 是否开启变更监听 |
| 输出 | `const ROIConfig&` | 当前生效配置 |
| 输出 | `bool loadResult` | 加载是否成功 |
| 输出 | `std::string error` | 失败原因 |

## 5. 数据结构

```cpp
struct ConfigStatus {
    bool        loaded = false;
    std::string path;
    std::uint64_t lastModifiedMs = 0;
    std::size_t roiCount = 0;
    std::string coordinate;
};
```

## 6. 类设计

```cpp
class ROIConfigManager {
public:
    ROIConfigManager();
    ~ROIConfigManager();

    // 加载并校验；失败返回 false，errOut 写明原因
    bool load(const std::string& path, std::string& errOut);

    // 若文件 mtime 变化则重新加载；热重载用
    bool reloadIfChanged(std::string& errOut);

    // 当前配置只读访问
    const ROIConfig& getConfig() const;
    const ConfigStatus& getStatus() const;

    // 按 name 查找 ROI；找不到返回 nullptr
    const ROIRegion* findByName(const std::string& name) const;

private:
    std::unique_ptr<ROIParser> parser_;
    ROIConfig                  cachedConfig_;
    ConfigStatus               status_;

    bool validateConfig(const ROIConfig& cfg, std::string& errOut);
};
```

## 7. 接口设计

| 接口 | 语义 |
| --- | --- |
| `load` | 一次性加载 + 校验 + 落缓存 |
| `reloadIfChanged` | 基于文件 mtime 增量重载 |
| `getConfig` | 只读引用，禁止调用方修改 |
| `findByName` | 多 ROI 场景下按 label 反查 |
| `getStatus` | 暴露运行时状态给日志 / 监控 |

## 8. 算法流程

```
load(path)
  ├── status_.path = path
  ├── parser_->parseFile(path)  → ROIConfig
  ├── validateConfig(cfg)
  │     ├── coordinate 非空
  │     ├── roiCount > 0
  │     └── 每个 ROIRegion 调用 isValidBoxRegion
  ├── cachedConfig_ = cfg
  └── status_.loaded = true

reloadIfChanged()
  ├── stat(path).mtime vs status_.lastModifiedMs
  ├── 未变 → 直接 return true
  └── 已变 → 走 load 流程
```

## 9. 异常处理

- 文件不存在：返回 false，`errOut = "config file not found: <path>"`
- JSON 语法错：透传 ROIParser 的错误信息
- ROI 列表为空：返回 false，`errOut = "rois is empty"`
- 两个 ROI 同名：返回 false，`errOut = "duplicate roi name: <name>"`
- 文件权限 / 占用：捕获 `std::ifstream` 异常，转为错误字符串

## 10. 测试方案

| 用例 | 输入 | 期望 |
| --- | --- | --- |
| 正常加载 | 合法 JSON，2 个 ROI | loaded=true，roiCount=2 |
| 文件不存在 | 错误路径 | loaded=false，错误含路径 |
| 重复 name | 两个 door_glue | 失败，错误含 duplicate |
| 空 ROI 列表 | `"rois": []` | 失败 |
| 热重载 | 修改文件 mtime 后调 reloadIfChanged | 重新加载，status 更新 |
| 按名反查 | findByName("door_glue") | 非空指针 |

性能：load 10 个 ROI 配置 < 5 ms。

## 11. AI 实现要求

- 类内禁止暴露 `ROIConfig&`（非 const），外部不可修改缓存
- `parser_` 用 `std::unique_ptr`，前置声明即可，避免头文件传染 JSON 类型
- 文件 mtime 用 `std::filesystem::last_write_time`，跨平台
- 错误字符串必须可直接进入产线日志，不要写中文 emoji / 占位符

## 12. 后续扩展

- 增加 YAML 路径支持（ROIParser 多态）
- 增加配置版本号字段，Manager 校验最低兼容版本
- 增加配置加密（产线配方保护）
