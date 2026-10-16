# 3DJianCeTuJiao

3D 点云检测 / 测量算子工程。当前已接入算子：

| 算子 | 头文件 | 说明 |
| --- | --- | --- |
| `pointCloudCropProcess` | `include/pointCloudCropProcess.h` | 基于三维包围盒 ROI 的点云裁剪，支持 pcd/ply 输入、JSON/YAML 多 ROI 配置、裁剪点云保存、无窗口俯视投影可视化 |

## 目录结构

```
├── 3rdparty/      # 本地第三方依赖（vcpkg 未覆盖的私有库放这里）
├── build_sbt/     # CMake 中间产物（不入库）
├── cmake/         # 自定义 FindXXX.cmake
├── data/          # 测试点云 + ROI 配置示例
├── docs/          # 算子接口文档 / 现场调试记录
├── include/       # 算子公共头文件（对外暴露的 API）
├── install/       # 构建产物输出（dll / exe）
├── src/
│   ├── pointCloudCropProcess.cpp        # 算子实现（PCL octree 包围盒裁剪）
│   └── pointCloudCropProcessDemo.cpp     # 命令行 Demo 入口
├── build_operator.bat                  # Windows 一键构建脚本
├── CMakeLists.txt
└── README.md
```

## 依赖

Windows / Visual Studio 2022 / CMake >= 3.15 / vcpkg

```bat
vcpkg install pcl[core,io,octree]:x64-windows ^
             opencv[core,imgproc,imgcodecs]:x64-windows ^
             nlohmann-json:x64-windows
set VCPKG_ROOT=D:\vcpkg
```

## 构建

```bat
:: Release（默认）
build_operator.bat

:: Debug
build_operator.bat Debug
```

产物：
- `install/sbt_cv_ops.dll` —— 算子动态库
- `install/sbt_cv_ops.lib` —— 导入库
- `install/pointCloudCropProcessDemo.exe` —— Demo

## 运行 Demo

Demo 里写死的相对路径是 `../data/1.ply` 与 `../data/1.json`，所以**必须从 `install/` 目录启动**：

```bat
cd /d install
pointCloudCropProcessDemo.exe
```

输出：
- `install/pointCloudCropProcess_output/` 下按 ROI 保存的裁剪点云（`<label>_<idx>.ply`）
- `install/visual.jpg` 俯视投影可视化图

## ROI 配置格式（JSON）

参考 `data/sample_rois.json`：

```json
{
  "multiroi": {
    "rois": [
      {
        "name": "roi_left",
        "min_point": { "x": -100.0, "y": -50.0, "z": 0.0 },
        "max_point": { "x":    0.0, "y":  50.0, "z": 200.0 }
      },
      {
        "name": "roi_right",
        "min_point": { "x": 0.0, "y": -50.0, "z": 0.0 },
        "max_point": { "x": 100.0, "y": 50.0, "z": 200.0 }
      }
    ]
  }
}
```

也支持 YAML 格式，参考 `data/sample_rois.yaml`。

## 接入新算子约定

1. 头文件放 `include/<opName>.h`，用 `SBT_<OPNAME>_API` 宏控制 dllexport/dllimport（参考 `pointCloudCropProcess.h`）。
2. 实现放 `src/<opName>.cpp`，在根 `CMakeLists.txt` 的 `sbt_cv_ops` 库源文件列表里追加。
3. 对外不暴露 PCL 类型，公共头里只用 `cv::Point3d` / `cv::Mat` / 基础类型。
4. 新 Demo 放 `src/<opName>Demo.cpp`，在根 `CMakeLists.txt` 里加 `add_executable`。
