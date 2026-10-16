# 02 — 数据结构设计

- 版本：v1.0
- 模块：PointCloudROIManager

---

## 1. 模块目标

定义跨 Operator 共享的全部数据类型。所有类型**禁止依赖 PCL**，只使用 STL / Eigen / 基础类型，保证公共头可在 Loader、ROIManager、Preprocess、Measurement 之间无泄漏传递。

## 2. 工业应用场景

- 跨进程 / 跨模块传递：算法库是动态库，公共 ABI 必须稳定
- 序列化：ROI 配置可落盘，结果可回溯
- 多语言绑定预留：未来可能被 Python / C# 调用

## 3. 架构位置

数据结构处于最底层，被所有上层模块引用。自身不包含任何业务逻辑。

## 4. 输入输出定义

本模块不直接处理 IO，仅提供类型。以下类型将被后续模块使用：

| 类型 | 被谁写入 | 被谁读取 |
| --- | --- | --- |
| `PointCloudData` | LoaderOperator | ROIManager / Preprocess / Measurement |
| `ROIConfig` | ROIParser | ROIExtractor |
| `ROIResult` | ROIExtractor | ROIResultManager / 下游 |
| `ROIManagerResult` | ROIResultManager | PipelineEngine / Operator |

## 5. 数据结构（完整定义）

### 5.1 点类型 PointXYZI

```cpp
struct PointXYZI {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float intensity = 0.0f;
};
```

单位 mm。默认 0，避免野点。仅几何 + 强度，不含法向 / RGB。

### 5.2 点云容器 PointCloudData

```cpp
struct PointCloudData {
    std::vector<PointXYZI> points;
    std::string  coordinate;     // "robot" / "vehicle" / "camera" / "world"
    std::uint64_t timestamp = 0; // 采集时间 ms
};
```

跨模块传递一律用 `std::shared_ptr<const PointCloudData>`，禁止值拷贝。

### 5.3 ROI 类型枚举

```cpp
enum class ROIType {
    Box = 0,
    Cylinder,   // reserved
    Sphere,     // reserved
    Polygon,    // reserved
    Path        // reserved
};
```

v1 仅实现 Box。

### 5.4 ROI 空间描述 ROIRegion

**扩展策略（强制约定）**：所有新增 ROI 类型（Cylinder / Sphere / Polygon / Path）的字段，一律以"追加成员"方式写入本结构体，**不新建并行结构**（如 `CylinderROIRegion` / `SphereROIRegion`）。理由：

- 公共头 ABI 稳定，跨 DLL 只暴露一个 `ROIRegion` 类型
- ROIParser / ROIGeometry / ROIExtractor 通过 `type` 字段分发，无需 dynamic_cast
- 旧 JSON 解析器不破坏旧配置文件

```cpp
struct ROIRegion {
    std::string name;                          // 唯一标签，如 "door_glue"
    ROIType    type = ROIType::Box;

    // ---- Box 字段（v1 使用）----
    Eigen::Vector3f min = Eigen::Vector3f::Zero();   // mm
    Eigen::Vector3f max = Eigen::Vector3f::Zero();   // mm

    // ---- 预留字段（后续类型扩展使用，v1 不赋值）----
    // Cylinder: axisBottom / axisTop / radius
    // Sphere:   center / radius
    // Polygon:  polygonXY / zMin / zMax
    // Path:     pathPoints / width
    // 新增字段一律追加在此注释下方，禁止改名 / 改顺序
};
```

### 5.5 ROI 配置 ROIConfig

```cpp
struct ROIConfig {
    std::string coordinate;                  // 配置文件声明的坐标系
    std::vector<ROIRegion> regions;
};
```

### 5.6 单 ROI 结果 ROIResult

**跨 DLL ABI 约束（强制）**：`cloud` 字段必须是 `shared_ptr<const PointCloudData>`，**禁止值语义**。

理由：`PointCloudData` 含 `std::vector<PointXYZI>` 与 `std::string`，直接按值跨 DLL 传递时，MSVC Debug/Release、不同 VS 版本的内存分配器不兼容，会在跨边界析构时崩溃。通过 `shared_ptr<const>` 共享所有权，删除动作在 DLL 内部完成，外部只读。

```cpp
struct ROIResult {
    std::string name;
    std::shared_ptr<const PointCloudData> cloud;   // 跨 DLL 共享，禁止值拷贝
    std::size_t pointCount = 0;                      // == cloud->points.size()
    bool success = false;
    std::string message;                              // 失败原因 / 备注
};
```

**内部使用例外**：仅在同一 DLL 内部、且不跨边界传递时，允许用值语义 `PointCloudData` 作为临时变量（如 Extractor 内的中转 buffer）。

### 5.7 整批输出 ROIManagerResult

```cpp
struct ROIManagerResult {
    bool success = false;          // 整批是否可交付
    std::vector<ROIResult> results;
    std::string error;            // 整批级错误（配置读不到 / 点云为空）
};
```

## 6. 类设计

数据结构模块不定义行为类，仅定义 POD 类型。配套两个自由函数：

```cpp
// 校验 ROI 包围盒合理性（min <= max，所有分量有限）
bool isValidBoxRegion(const ROIRegion& region, std::string& errOut);

// 校验配置坐标系与点云坐标系一致
bool isCoordinateMatched(const ROIConfig& cfg, const PointCloudData& cloud,
                         std::string& errOut);
```

## 7. 接口设计

公共头文件清单（位于 `include/`）：

```
ROITypes.h          // PointXYZI / ROIType
PointCloudData.h
ROIRegion.h
ROIConfig.h
ROIResult.h
ROIManagerResult.h
```

禁止把以上类型合并到一个巨型头。每个类型独立成头，降低重编译开销。

## 8. 算法流程

无算法。本模块唯一"流程"是字段约定：

```
ROIConfig → ROIRegion[] → ROIExtractor → ROIResult[] → ROIManagerResult
```

## 9. 异常处理

- POD 类型不抛异常
- `isValidBoxRegion` / `isCoordinateMatched` 通过返回值 + errOut 字符串反馈，不抛
- 数值字段全部有限性检查：拒绝 `NaN / Inf` 进入 ROI 数据结构

## 10. 测试方案

- `isValidBoxRegion`：min > max、分量为 NaN / Inf、空 name 全部覆盖
- `isCoordinateMatched`：cfg.coordinate 为空、与 cloud 不一致
- 内存布局：`sizeof(PointXYZI) == 16`，便于跨 ABI；用 `static_assert` 锁死

```cpp
static_assert(sizeof(PointXYZI) == 16, "PointXYZI must stay 16 bytes for ABI");
```

## 11. AI 实现要求

- 所有 POD 字段必须有类内默认值
- 禁止在结构体中放 `std::function` / 虚函数（破坏 ABI）
- 字符串字段允许 `std::string`（Windows MSVC 下 ABI 稳定即可，跨编译器再考虑 `char*`）
- 单位注释（mm / ms）必须写在每个字段上

## 12. 后续扩展

所有新类型字段**直接追加到 `ROIRegion`**（见 §5.4 扩展策略），不新建并行结构体：

- Cylinder：`Eigen::Vector3f axisBottom; Eigen::Vector3f axisTop; float radius;`
- Sphere：`Eigen::Vector3f center; float radius;`
- Polygon：`std::vector<Eigen::Vector2f> polygonXY; float zMin; float zMax;`
- Path：`std::vector<Eigen::Vector3f> path; float width;`

ROIGeometry / ROIExtractor 通过 `type` 字段分发到对应判定分支，禁止用继承体系（`class BoxROIRegion : public ROIRegion` 之类），因为公共头 ABI 会被破坏。
