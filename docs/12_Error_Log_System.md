# 12 — 错误与日志系统

- 版本：v1.0

---

## 1. 模块目标

为 PointCloudROIManager 提供统一的错误码、错误信息、运行日志规范。产线排障时，工程师必须能从一段日志里**5 分钟内定位到**：是配置错、点云错、还是算法错。

## 2. 工业应用场景

- 现场凌晨报警，工程师远程拉日志就要定位
- 多工位共用一套算法库，错误必须带工位 / 帧号 / ROI 名
- 错误码要能直接对接 MES 报警系统

## 3. 架构位置

```
所有模块 ──▶ ErrorCode + LogEvent
                │
                ▼
          日志后端（spdlog / 文件 / 平台监控）
```

模块内只产生结构化事件，不直接 `fprintf`。

## 4. 输入输出定义

| 方向 | 类型 | 说明 |
| --- | --- | --- |
| 输入 | 错误场景枚举 | 代码内抛出 |
| 输出 | 结构化日志 JSON 行 | 落盘 / 上报 |

## 5. 数据结构

### 5.1 错误码（致命 / 整帧失败）

错误码仅用于**整帧失败**或**不可恢复异常**。可恢复的业务异常（如单 ROI 0 命中）走 §5.2 的 warn 通道，不进错误码表。

```cpp
enum class RoiErrorCode : int {
    Ok = 0,

    // Config / Parser  1xxx
    ConfigFileNotFound      = 1001,
    ConfigJsonSyntaxError   = 1002,
    ConfigMissingRois      = 1003,
    ConfigDuplicateName    = 1004,
    ConfigCoordinateEmpty   = 1005,
    ConfigInvalidBox       = 1006,

    // Cloud / Input   2xxx
    InputCloudEmpty        = 2001,
    InputCoordinateMismatch = 2002,
    InputCloudOOM          = 2003,

    // Extractor       3xxx
    ExtractorNotBuilt      = 3001,
    ExtractorOctreeFail    = 3002,
    // 注：原 3003 ExtractorNoHit 已迁出，见 RoiWarnCode

    // Operator        4xxx
    OperatorMissingBagKey  = 4001,
    OperatorTypeMismatch   = 4002,
};
```

### 5.2 警告码（可恢复 / 业务标记）

```cpp
enum class RoiWarnCode : int {
    NoHit                  = 3003,   // 单 ROI 0 命中，不算错误
    ConfigFieldDefaulted   = 1101,   // 配置缺字段，使用默认值
    CoordinateSkipped      = 2101,   // coordinate 为空，跳过一致性校验
    HotReloadFailed        = 1102,   // 热重载失败，沿用旧配置
};
```

错误码与警告码在日志 JSON 中分字段上报：`"level": "ERROR"` vs `"level": "WARN"`，MES 只对 ERROR 触发报警。

### 5.2 日志事件

```cpp
struct LogEvent {
    std::uint64_t frameId;        // 帧号
    std::string   stationId;       // 工位
    std::string   roiName;         // 可选
    RoiErrorCode  code;
    std::string   message;
    std::chrono::system_clock::time_point ts;
};
```

## 6. 类设计

```cpp
class RoiLogger {
public:
    static RoiLogger& instance();

    void setStationId(const std::string& id);
    void setFrameId(std::uint64_t id);

    void info (const std::string& msg);
    void warn (RoiErrorCode code, const std::string& msg);
    void error(RoiErrorCode code, const std::string& msg);

private:
    std::string stationId_;
    std::uint64_t frameId_ = 0;
};
```

## 7. 接口设计

| 接口 | 语义 |
| --- | --- |
| `info` | 正常流程打点（每帧 ROI 数 / 耗时） |
| `warn` | 可恢复异常（单 ROI 0 命中、配置缺字段用默认值） |
| `error` | 整帧失败，需要 MES 报警 |

错误码 → 人类可读映射表在 `RoiLogger.cpp` 内集中维护。

## 8. 算法流程

```
任意模块失败
  ├── 构造 LogEvent（frameId / stationId 由 Logger 单例注入）
  ├── RoiLogger::instance().error(code, msg)
  ├── 同时把 code 写入 ROIManagerResult.error / ROIResult.message
  └── 不抛异常给平台
```

日志输出格式（JSON 行）：

```json
{"ts":"2026-10-14T10:23:45.123Z","station":"A01","frame":88123,"level":"ERROR","code":2002,"roi":null,"msg":"coordinate mismatch: config=robot, cloud=world"}
```

## 9. 异常处理

- 日志后端写盘失败：吞掉，不影响主流程（降级为 stderr）
- `frameId` 未设置：打 `frame=0`，不抛
- 日志文件滚动：按天切，保留 30 天
- 敏感数据（点云本体）禁止进日志

## 10. 测试方案

| 用例 | 期望 |
| --- | --- |
| 触发 ConfigFileNotFound | 日志 code=1001，message 含路径 |
| 触发 coordinate mismatch | code=2002，message 含双方坐标 |
| 单 ROI 0 命中 | code=3003 + level=WARN，不触发 MES 报警 |
| 日志落盘失败 | 主流程不崩，stderr 有降级输出 |

## 11. AI 实现要求

- 业务代码中禁止直接 `printf` / `std::cout`
- 错误字符串必须先 `to_string(code)` 再拼接，禁止裸字符串
- 热点路径（extract / boxSearch）不打 info，只在异常时打 warn
- 错误码区段固定：Config 1xxx / Cloud 2xxx / Extractor 3xxx / Operator 4xxx，新增不跨段
- 警告码独立区段：业务标记 1xxx 下段（1101+）/ 输入类 2xxx 下段（2101+），与错误码不混
- 日志字段顺序固定（ts / station / frame / level / code / roi / msg），便于 grep

## 12. 后续扩展

- 对接 ELK / Loki 远程采集
- 错误码与 MES 报警等级映射表
- 增加 trace 事件（每阶段耗时），用于全链路追踪
