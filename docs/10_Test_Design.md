# 10 — 测试体系设计

- 版本：v1.0
- 对应实现：`tests/test_roi_manager.cpp` + `tests/fixtures/`

---

## 1. 模块目标

为 PointCloudROIManager 全链路（Config / Parser / Geometry / Extractor / Manager / Operator）建立统一的测试体系，覆盖功能、异常、性能三类，保证每次重构不破坏产线行为。

## 2. 工业应用场景

- 产线代码改一行就要能在 CI 里跑完整回归
- 现场 bug 必须能复现到单元测试级别
- 性能退化（100 万点从 80 ms 涨到 200 ms）要在合入前告警

## 3. 架构位置

```
PointCloudROIManager/
└── tests/
    ├── CMakeLists.txt
    ├── test_parser.cpp
    ├── test_geometry.cpp
    ├── test_extractor.cpp
    ├── test_manager.cpp
    ├── test_operator.cpp
    ├── perf_benchmark.cpp
    └── fixtures/
        ├── valid_rois.json
        ├── invalid_syntax.json
        ├── empty_cloud.ply
        └── synthetic_1m.ply   （生成脚本生成，不入库）
```

测试框架：GoogleTest 1.12+（vcpkg 安装）。性能测试用 Google Benchmark。

## 4. 输入输出定义

- 输入：fixtures 中的 JSON / PLY + 代码内构造的合成点云
- 输出：测试报告（XML / JUnit）、性能基线（JSON）

## 5. 数据结构

合成点云生成器：

```cpp
// tests/utils/synthetic_cloud.h
struct SyntheticCloudSpec {
    std::size_t pointCount = 1'000'000;
    Eigen::Vector3f bboxMin = {0, 0, 0};
    Eigen::Vector3f bboxMax = {1000, 1000, 500};
    float intensityMean = 100.0f;
    float intensityStd = 20.0f;
    unsigned int seed = 42;
};
PointCloudData generateSyntheticCloud(const SyntheticCloudSpec& spec);
```

## 6. 类设计

不新增生产类。测试专用 fixture：

```cpp
class ROIManagerTest : public ::testing::Test {
protected:
    void SetUp() override;
    std::shared_ptr<PointCloudData> cloud_;
    std::string validConfigPath_;
};
```

## 7. 接口设计（用例清单）

### 7.1 Parser（test_parser.cpp）

- 合法 JSON 2 ROI
- 缺 coordinate 默认 world
- 缺 type 默认 box
- 语法错 / 缺 rois / name 类型错 / min.x 字符串
- BOM / 注释兼容

### 7.2 Geometry（test_geometry.cpp）

- 点在盒内 / 盒外 / 边界
- min > max 非法 region
- NaN 点
- AABB 相交 6 个方向

### 7.3 Extractor（test_extractor.cpp）

- 全命中 / 0 命中 / 边界命中
- 未 build 直接 extract
- 多 ROI 并行 vs 串行结果一致
- 100 万点 / 10 ROI 性能 ≤ 100 ms

### 7.4 Manager（test_manager.cpp）

- init 合法 / 非法路径
- execute 空点云 / 坐标系不一致
- 单 ROI 0 命中不阻塞整批
- 重复 init 不泄漏

### 7.5 Operator（test_operator.cpp）

- init 缺 configPath 失败
- execute 缺 input_cloud
- 动态库加载 / 卸载 100 次无泄漏

## 8. 算法流程（CI）

```
CI 流水线:
  ├── cmake --build (Debug + ASan)
  ├── ctest -L "unit"       （功能 + 异常）
  ├── ctest -L "perf"       （性能基线对比）
  ├── clang-tidy / iwyu
  └── Asan / UBSan 报告
```

性能基线文件 `tests/baseline.json`：

```json
{
  "extractor_1m_10roi_ms": 85.0,
  "manager_end2end_ms": 95.0,
  "parser_10roi_ms": 1.5
}
```

超过基线 15% 即告警。

## 9. 异常处理

- fixture 文件读不到 → 测试 fail（不是跳过）
- 性能测试在 Debug 下跳过，只跑 Release
- 随机点云用固定 seed，保证可复现
- 单测超时 30 秒自动 fail，防止死循环

## 10. 测试方案（本模块自身）

- 用例覆盖行 ≥ 90%，分支 ≥ 80%（lcov / llvm-cov）
- 关键模块（Geometry / Extractor）必须 100% 分支覆盖
- 每个 fixture 文件 < 1 MB，大文件用生成脚本

## 11. AI 实现要求

- 禁止 `ASSERT_EQ` 之后还继续读空指针，先 ASSERT 再用
- 性能用例用 `BENCHMARK_MAIN()`，不要塞进普通 TEST
- 测试中构造的 `PointCloudData` 必须用 `generateSyntheticCloud`，不允许手搓 100 万个点
- 跨模块集成测试只走 Operator 接口，不直接 new Manager 内部组件
- 所有 fixture 路径用 `std::filesystem::current_path()` 拼接，不硬编码绝对路径

## 12. 后续扩展

- 增加属性测试（rapidcheck）：随机 ROI 与随机点云不变量验证
- 增加模糊测试（libFuzzer）：JSON Parser 对畸形输入鲁棒性
- 增加录制回放：现场一帧点云加密后回灌到测试
