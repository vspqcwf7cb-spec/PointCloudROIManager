# 00 — AI 辅助开发主控文档（Master Control）

- 版本：v1.0
- 项目：Automotive Glue 3D Measurement Vision Platform
- 模块：PointCloudROIManager
- 编写：Vision Algorithm Architecture Team
- 语言：C++17
- 框架：PCL + Eigen + nlohmann/json

---

## 1. 文档目的

控制 AI 辅助开发 PointCloudROIManager 模块。该模块是汽车涂胶 3D 视觉测量系统第一阶段的核心模块。

主要任务：将整车扫描得到的大规模点云，根据 JSON 配置文件定义的 ROI 区域，自动切割生成多个局部检测点云，供下游涂胶特征提取与测量使用。

## 2. 系统位置

```
PointCloud Input
    ↓
PointCloudLoaderOperator
    ↓
════════════════════════════
PointCloudROIManager        ← 本模块
════════════════════════════
    ↓
Local ROI PointCloud
    ↓
PointCloudPreprocessOperator
    ↓
GlueFeatureExtraction
    ↓
Measurement
```

PointCloudROIManager 只做**空间区域分割**。明确不负责：

- 点云滤波
- 法向计算
- 胶区域识别
- 尺寸测量
- 缺陷判断

## 3. 开发目标

### 3.1 通用性

支持场景：汽车涂胶、焊缝检测、零件尺寸检测、激光扫描检测。模块不得对"涂胶"做任何硬编码假设。

### 3.2 插件化

最终封装为 `PointCloudROIManagerOperator`，作为 VisionAlgorithmPlatform 的插件运行，不依赖具体可执行程序。

### 3.3 参数配置化

禁止代码硬编码 ROI。所有 ROI 必须来自 JSON/YAML 配置文件。示例：

```json
{
  "coordinate": "robot",
  "rois": [
    {
      "name": "door_glue",
      "type": "box",
      "min": { "x": 100, "y": 200, "z": 300 },
      "max": { "x": 150, "y": 260, "z": 350 }
    }
  ]
}
```

## 4. AI 开发规则（强制）

禁止行为：

- 直接生成完整工程
- 跳过架构设计直接写代码
- 一个 `.cpp` 包含全部功能
- 单个函数越出模块职责范围

必须按顺序执行：

```
需求分析 → 模块设计 → 数据结构设计 → 接口设计
        → 头文件 → cpp 实现 → 单元测试 → 性能优化
```

## 5. 模块架构

```
PointCloudROIManager
    ├── ROIConfigManager      配置生命周期
    ├── ROIParser             JSON → C++ 对象
    ├── ROIExtractor          空间裁剪（BoxROIExtractor 基于 PCL Octree）
    └── ROIResultManager      结果封装
```

## 6. 模块职责边界

| 组件 | 负责 | 不负责 |
| --- | --- | --- |
| ROIConfigManager | 配置加载、参数校验、生命周期 | 点云处理 |
| ROIParser | JSON → ROIRegion | 字段语义校验以外的逻辑 |
| ROIExtractor | 根据 ROI 空间提取点云 | ROI 配置解析 |
| PointCloudROIManager | 整体流程调度 | 单点云处理细节 |

Manager 调度流程：

```
Load Config → Create Extractor → Execute ROI Extraction → Collect Result
```

## 7. 核心数据流

输入 `PointCloudData`：`{ XYZ, Intensity, Coordinate, Timestamp }`

```
PointCloud ──┐
             ├──▶ Spatial Search ──▶ ROI Cloud ──▶ ROIResult { name, cloud, pointCount, status }
ROI Config ──┘
```

## 8. 开发步骤（强制不跳步）

| Step | 主题 | 文档 | 目标产物 |
| --- | --- | --- | --- |
| 01 | 数据结构设计 | 02_Data_Structure_Design.md | ROIRegion / ROIConfig / ROIResult / PointCloudData |
| 02 | ROI 配置模块 | 03_ROI_Config_Module.md | 配置加载、校验、缓存 |
| 03 | JSON 解析 | 04_ROI_JSON_Parser_Module.md | JSON → ROIRegion |
| 04 | ROI 几何 | 05_ROI_Geometry_Module.md | 空间 ROI 点判定 |
| 05 | ROI 提取 | 06_ROI_Extractor_Module.md | PCL Octree 点云裁剪 |
| 06 | Manager 核心 | 07_PointCloudROIManager_Module.md | 调度类 |
| 07 | Operator 封装 | 08_ROI_Manager_Operator.md | 平台插件接口 |
| 08 | Pipeline 接入 | 09_Pipeline_Integration.md | 完整涂胶检测流程打通 |

## 9. 编码总要求

遵循 `14_Code_Standard.md`：C++17、PCL、Eigen、RAII、智能指针、异常安全。

## 10. 目录标准

```
PointCloudROIManager/
├── include/
│   ├── PointCloudROIManager.h
│   ├── ROIConfig.h
│   ├── ROIParser.h
│   ├── ROIExtractor.h
│   └── ROIResult.h
├── src/
│   ├── PointCloudROIManager.cpp
│   ├── ROIParser.cpp
│   └── ROIExtractor.cpp
├── tests/
│   └── test_roi_manager.cpp
└── config/
    └── roi_config.json
```

## 11. 测试要求

每个模块必须提供测试：

- 正常：单 ROI、多 ROI
- 异常：JSON 错误、空点云、ROI 越界、空 ROI 列表
- 性能：100 万点 / 10 个 ROI / **目标 < 100 ms**

## 12. AI 输出格式要求

每次代码输出必须包含六部分，禁止只贴代码：

1. 设计说明
2. 文件结构
3. `.h` 文件
4. `.cpp` 文件
5. 测试代码
6. 编译说明

## 13. 未来扩展

ROI 类型：BOX / CYLINDER / SPHERE / POLYGON / PATH。
应用方向：涂胶检测、焊缝检测、激光测量、三维尺寸检测。

---

### 现有代码迁移映射

当前 `pointCloudCropProcess.h/.cpp` 已包含：点云加载、JSON/YAML ROI 读取、多 ROI、PCL Octree 裁剪、裁剪点云保存、俯视投影可视化。

重构映射：

```
pointCloudCropProcess (单体函数)
        ↓ 拆分为
ROIParser          ← JSON/YAML 解析部分
BoxROIExtractor     ← PCL Octree 裁剪部分
ROIResultManager    ← PointCloudRoiInfo / Output 封装
可视化 / 保存       ← 迁移到下游 ResultExporter，不再属于 Manager
```
