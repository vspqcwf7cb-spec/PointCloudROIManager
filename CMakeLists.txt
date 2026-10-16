cmake_minimum_required(VERSION 3.15)
project(3DJianCeTuJiao LANGUAGES CXX)

# ---------- Global settings ----------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# All artifacts (dll / lib / exe) drop into install/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/install)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/install)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/install)

# 多配置生成器（Visual Studio）默认在输出目录后追加 Release/Debug 子目录，
# 这里强制平铺到 install/，与 README / build_operator.bat 的约定一致
foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} ${CMAKE_SOURCE_DIR}/install)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg} ${CMAKE_SOURCE_DIR}/install)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg} ${CMAKE_SOURCE_DIR}/install)
endforeach()

# Custom cmake modules + 3rdparty local search path（本地自包含依赖，无需 vcpkg）
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/3rdparty)

if(MSVC)
    # /wd4251：导出类含 STL 成员（string/vector/unique_ptr）的标准 DLL 模式告警，
    # 单次 MSVC 构建下 ABI 稳定（02 §5.6 已接受 std::string 跨 ABI），属可接受噪声
    add_compile_options(/utf-8 /EHsc /MP /W3 /wd4251)
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX)
endif()

# ---------- Dependencies ----------
# PCL 由 3rdparty/PCL 1.14.0 提供（自带 Eigen / Boost / FLANN / VTK）
# Eigen 通过 PCL_INCLUDE_DIRS 提供（公共头 ROIRegion.h / ROIGeometry.h 依赖 <Eigen/Core>）
# nlohmann/json 由 3rdparty/json.hpp 单头文件提供（仅 ROIParser.cpp 使用）
find_package(PCL REQUIRED COMPONENTS common io octree)
add_definitions(${PCL_DEFINITIONS})

# =====================================================================
#  sbt_cv_ops  —— 算子动态库（模块化 PointCloudROIManager）
#  说明：旧单体 pointCloudCropProcess（依赖 OpenCV 可视化）已移出构建，
#        按文档 00/09 将可视化迁到下游 ResultExporter。
# =====================================================================
add_library(sbt_cv_ops SHARED
    src/ROITypes.cpp
    src/RoiLogger.cpp
    src/ROIParser.cpp
    src/ROIGeometry.cpp
    src/ROIConfigManager.cpp
    src/BoxROIExtractor.cpp
    src/ROIResultManager.cpp
    src/PointCloudROIManager.cpp
    src/PointCloudROIManagerOperator.cpp
)

# 公共头对消费方可见；Eigen 随 PCL_INCLUDE_DIRS PUBLIC 透出；json.hpp 仅内部使用
target_include_directories(sbt_cv_ops
    PUBLIC  ${CMAKE_SOURCE_DIR}/include
            ${PCL_INCLUDE_DIRS}
    PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty
)

target_compile_definitions(sbt_cv_ops
    PRIVATE SBT_CV_OPS_EXPORTS          # turns dllexport on inside this lib
    PUBLIC  ${PCL_DEFINITIONS}
)

target_link_libraries(sbt_cv_ops
    PRIVATE ${PCL_LIBRARIES}
)

# =====================================================================
#  pointCloudROIManagerDemo —— 命令行演示（加载 ../data/4.ply + ../config/roi_config.json）
# =====================================================================
add_executable(pointCloudROIManagerDemo
    src/pointCloudROIManagerDemo.cpp
)
target_link_libraries(pointCloudROIManagerDemo
    PRIVATE
        sbt_cv_ops
        ${PCL_LIBRARIES}
)
target_include_directories(pointCloudROIManagerDemo PRIVATE ${PCL_INCLUDE_DIRS})

set_target_properties(pointCloudROIManagerDemo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/install
)

# =====================================================================
#  Unit tests (GoogleTest, guarded by availability)
# =====================================================================
enable_testing()
add_subdirectory(tests)
