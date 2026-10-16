/**
 * @file test_roi_manager.cpp
 * @brief PointCloudROIManager 轻量自测（不依赖 GTest）
 * @details 加载 data/4.ply（整车点云）+ data/roi_single.json（单 ROI），
 *          验证 init / execute / 单 ROI 裁剪结果，打印 PASS/FAIL 并以退出码反馈。
 * @usage 在 install/ 目录下运行（相对路径 ../data/... 生效）。
 */

#include <pcl/io/ply_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

#include "PointCloudData.h"
#include "PointCloudROIManager.h"
#include "ROIManagerResult.h"

namespace {

int failures = 0;

void check(bool ok, const std::string& msg) {
    std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << msg << std::endl;
    if (!ok) {
        ++failures;
    }
}

}  // namespace

int main() {
    const std::string cloudFile = "../data/4.ply";
    const std::string configPath = "../data/roi_single.json";

    // 1. 加载整车点云（PCL 读 PLY，LoadOperator 职责）
    auto pclCloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
    std::cout << "loading " << cloudFile << " ..." << std::endl;
    if (pcl::io::loadPLYFile(cloudFile, *pclCloud) < 0 || pclCloud->empty()) {
        std::cerr << "load failed: " << cloudFile << std::endl;
        return 2;
    }
    std::cout << "loaded " << pclCloud->size() << " points" << std::endl;

    // 转为平台 PointCloudData
    auto cloud = std::make_shared<sbt::proc::PointCloudData>();
    cloud->coordinate = "robot";
    cloud->points.resize(pclCloud->size());
    for (std::size_t i = 0; i < pclCloud->size(); ++i) {
        cloud->points[i].x = (*pclCloud)[i].x;
        cloud->points[i].y = (*pclCloud)[i].y;
        cloud->points[i].z = (*pclCloud)[i].z;
        cloud->points[i].intensity = (*pclCloud)[i].intensity;
    }

    // 2. init 单 ROI 配置
    sbt::proc::PointCloudROIManager manager;
    std::cout << "init " << configPath << " ..." << std::endl;
    check(manager.init(configPath), "init loads single-ROI config");

    // 3. execute 裁剪
    const bool ok = manager.execute(cloud);
    check(ok, "execute succeeds");
    check(manager.getState() == sbt::proc::ManagerState::Done, "state == Done");

    // 4. 校验结果：恰好 1 个 ROI，且命中点数 > 0
    const sbt::proc::ROIManagerResult& result = manager.getResult();
    check(result.success, "batch success");
    check(result.results.size() == 1, "exactly one ROI result");

    if (!result.results.empty()) {
        const auto& roi = result.results[0];
        std::cout << "  ROI [" << roi.name << "] points=" << roi.pointCount
                  << " success=" << (roi.success ? "true" : "false") << std::endl;
        check(roi.success, "ROI success");
        check(roi.pointCount > 0, "ROI pointCount > 0");
        check(roi.cloud && roi.cloud->points.size() == roi.pointCount,
              "ROI cloud size == pointCount");
    }

    std::cout << (failures == 0 ? "RESULT: ALL PASSED" : "RESULT: FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
