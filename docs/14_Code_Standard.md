# 14 — C++ 代码规范

- 版本：v1.0
- 适用：PointCloudROIManager 全部 `.h / .cpp`

---

## 1. 模块目标

统一 PointCloudROIManager 代码风格，使任何工程师（含 AI）接手后，代码读起来像同一个人写的。规范即门禁，CI 不通过等于代码合不进来。

## 2. 工业应用场景

- 多人协作 + AI 协同开发，风格漂移会让 review 成本翻倍
- 产线代码常年维护，可读性优先于炫技
- 静态检查（clang-tidy / cpplint）必须能自动执行

## 3. 架构位置

本规范是横向约束，不对应任何模块。CI 流水线每一次 PR 都强制检查。

## 4. 输入输出定义

- 输入：代码 diff
- 输出：编译警告 = 0、clang-tidy 警告 = 0、格式（clang-format）通过

## 5. 数据结构（约束清单）

### 5.1 语言

- 标准：**C++17**，禁止使用 C++20 特性（`<concepts>` / `ranges` / `std::format`）
- 编译器：MSVC 2022 / GCC 11+，所有代码必须两边都编过
- 禁止：`using namespace std;`（头文件与实现文件都禁止）
- 禁止：全局变量、裸 `new` / `delete`、裸 `malloc`

### 5.2 命名

| 对象 | 规则 | 示例 |
| --- | --- | --- |
| 类 / 结构体 | PascalCase | `PointCloudROIManager` |
| 函数 / 方法 | camelCase | `extractAll` |
| 变量 | camelCase | `pointCount` |
| 私有成员变量 | 后缀 `_` | `cachedConfig_` |
| 常量 / 枚举值 | PascalCase | `ROIType::Box` |
| 文件 / 头文件守卫 | `SBT_<DIR>_<FILE>_H_` | `SBT_ROI_MANAGER_H_` |
| 命名空间 | 全小写 | `sbt::proc` |

### 5.3 头文件组织

```cpp
// 1. 头文件守卫
#ifndef SBT_ROI_MANAGER_H_
#define SBT_ROI_MANAGER_H_

// 2. 前置声明（优先）
#include <memory>
#include <string>

namespace sbt { namespace proc {
class ROIConfig;
class ROIRegion;
}}

// 3. 类声明
namespace sbt { namespace proc {
class PointCloudROIManager { ... };
}}

#endif
```

- 头文件只 include 真正需要的类型，其他全部前置声明
- 公共头**禁止** include `<pcl/...>` / `<json.hpp>` / `<opencv2/...>`

### 5.4 内存与 RAII

- 所有权明确：`unique_ptr` 优先，`shared_ptr` 仅在跨模块共享时使用
- 跨模块传递点云一律 `shared_ptr<const PointCloudData>`
- 容器先 `reserve` 再循环 `push_back`
- 禁止在析构函数里抛异常

### 5.5 const 正确性

- 只读成员方法必须 `const`
- 只读参数必须 `const T&`
- 只读指针必须 `const T*` / `shared_ptr<const T>`
- 局部变量能 `const` 就 `const`

### 5.6 错误处理

- 不返回 magic number，返回 `bool + std::string& errOut` 或 `std::expected`（v1 用前者）
- 不抛业务异常；系统异常（bad_alloc）在顶层 catch
- 错误字符串必须结构化：`"<field>: <expect>, got <actual>"`

## 6. 类设计约束

- 单个类职责单一（SRP）
- 类成员变量全部私有
- 接口最小化：头文件 public 方法 ≤ 10 个
- 函数行数 ≤ 60 行，超过拆私有 helper
- 圈复杂度 ≤ 10（clang-tidy `readability-function-cognitive-complexity`）

## 7. 接口设计

- 头文件里所有 public 方法必须有 Doxygen 注释：`/** ... */`
- 单位注释必须写（mm / ms / count）
- 参数顺序：输入 → 输入/输出 → 输出
- 禁止通过非 const 引用返回内部状态（返回 const 引用 + 显式 setter）

## 8. 算法流程（CI 门禁）

```
pre-commit:
  ├── clang-format -i（自动格式化）
  ├── clang-tidy（强制 0 warning）
  ├── cpplint（强制 0 error）
  └── 快速单测（Parser / Geometry）

CI:
  ├── MSVC / GCC 全编（警告 = 0）
  ├── ctest 全量
  ├── Asan / UBSan
  └── 性能基线对比
```

## 9. 异常处理（规范层）

- `noexcept` 用于移动构造 / 析构
- 构造函数不做业务初始化（用 `init()` 返回 bool）
- 所有外部资源（文件 / 网络 / PCL 对象）封装在 RAII 类内
- catch 顺序：具体异常 → `const std::exception&` → 不 catch `...`

## 10. 测试要求

- 新功能必须带测试，行覆盖率 ≥ 90%
- Bug 修复必须先写复现测试
- 测试命名：`ModuleName_Scenario_Expectation`，如 `Parser_InvalidSyntax_ReturnsFalse`

## 11. AI 实现要求

- AI 生成的代码必须直接通过 clang-format / clang-tidy，不允许"人工再修"
- 头文件里出现 `#include <pcl/` 直接打回
- 出现 `using namespace` 直接打回
- 出现裸 `new` / `new[]` 直接打回
- 注释用中文即可，但 Doxygen 标签（`@brief` / `@param`）必须保留英文
- 任何 magic number 必须抽常量或 enum

## 12. 后续扩展

- 接入 clang-format 配置文件（`.clang-format`）
- 接入 pre-commit 钩子
- 引入 `include-what-you-use` 自动梳理头文件
- 生成 Doxygen 文档站
