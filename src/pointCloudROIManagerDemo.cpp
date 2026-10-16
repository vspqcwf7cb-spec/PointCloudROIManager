/**
 * @file pointCloudROIManagerDemo.cpp
 * @brief PointCloudROIManager 命令行演示
 * @details 加载整车点云（默认 ../data/4.ply）+ ROI 配置（默认 ../config/roi_config.json），
 *          执行 ROI 裁剪并输出每个 ROI 命中点数与端到端耗时。
 * @usage pointCloudROIManagerDemo [cloudFile] [configPath]
 */

#include <pcl/io/ply_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

#include "PointCloudData.h"
#include "PointCloudROIManager.h"
#include "ROIManagerResult.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::string cloudFile = "../data/4.ply";
    std::string configPath = "../config/roi_config.json";
    if (argc > 1) cloudFile = argv[1];
    if (argc > 2) configPath = argv[2];

    // 加载点云（LoaderOperator 职责，这里用 PCL 直接读）
    auto pclCloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    std::cout << "loading " << cloudFile << " ..." << std::endl;
    if (pcl::io::loadPLYFile(cloudFile, *pclCloud) < 0 || pclCloud->empty()) {
        std::cerr << "load failed: " << cloudFile << std::endl;
        return -1;
    }
    std::cout << "loaded " << pclCloud->size() << " points" << std::endl;

    // 转为平台 PointCloudData（只读，跨 DLL 用 shared_ptr）
    auto cloud = std::make_shared<sbt::proc::PointCloudData>();
    cloud->coordinate = "robot";
    cloud->points.resize(pclCloud->size());
    for (std::size_t i = 0; i < pclCloud->size(); ++i) {
        cloud->points[i].x = (*pclCloud)[i].x;
        cloud->points[i].y = (*pclCloud)[i].y;
        cloud->points[i].z = (*pclCloud)[i].z;
        cloud->points[i].intensity = (*pclCloud)[i].intensity;
    }

    sbt::proc::PointCloudROIManager manager;
    if (!manager.init(configPath)) {
        std::cerr << "init failed: " << manager.getLastError() << std::endl;
        return -2;
    }

    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = manager.execute(cloud);
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    const auto& result = manager.getResult();
    std::cout << "execute: " << (ok ? "OK" : "FAIL") << ", time " << ms << " ms" << std::endl;
    std::cout << "batch success=" << (result.success ? "true" : "false")
              << ", roi count=" << result.results.size() << std::endl;
    for (const auto& r : result.results) {
        std::cout << "  [" << r.name << "] points=" << r.pointCount
                  << " success=" << (r.success ? "true" : "false");
        if (!r.message.empty()) {
            std::cout << " msg=" << r.message;
        }
        std::cout << std::endl;
    }

    return ok ? 0 : -3;
}
