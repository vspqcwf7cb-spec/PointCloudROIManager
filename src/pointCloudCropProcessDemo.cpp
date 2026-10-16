/**
 * @file: pointCloudCropProcessDemo.cpp
 * @brief: 点云裁剪处理算子演示程序
 * @details: 展示点云文件和多 ROI JSON 标注文件输入，执行三维包围盒裁剪，输出裁剪点数、点云文件和可视化图。
 * @author: Zhang Wenbo
 * @date: 2026/08/25
 * @version: v1.0.0
 */

#include "pointCloudCropProcess.h"

#include <opencv2/imgcodecs.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

#include <filesystem>
#include <iomanip>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    sbt::proc::PointCloudCropProcessInput pointCloudCropProcessInput;
    pointCloudCropProcessInput.cloudFile = "../data/1.ply";
    pointCloudCropProcessInput.roiFile = "../data/1.json";
    pointCloudCropProcessInput.isCrop = true;
    pointCloudCropProcessInput.isSave = true;
    pointCloudCropProcessInput.saveDir = "./pointCloudCropProcess_output";
    pointCloudCropProcessInput.isVisual = true;


    sbt::proc::PointCloudCropProcessOutput pointCloudCropProcessOutput;
    sbt::proc::pointCloudCropProcess(pointCloudCropProcessInput, pointCloudCropProcessOutput);

    std::cout << "SBT_POINT_CLOUD_CROP_PROCESS: v1.0.0" << std::endl;
    std::cout << "processFlag: " << pointCloudCropProcessOutput.processFlag << std::endl;
    std::cout << "processTime: " << std::fixed << std::setprecision(3) << pointCloudCropProcessOutput.processTime << " ms" << std::endl;
    std::cout << "processMessage: " << pointCloudCropProcessOutput.processMessage << std::endl;
    std::cout << "rois: " << pointCloudCropProcessOutput.rois.size() << std::endl;

    for (size_t i = 0; i < pointCloudCropProcessOutput.rois.size(); ++i) {
        const auto& roi = pointCloudCropProcessOutput.rois[i];
        std::cout << "[" << i << "] " << roi.label
                  << ", points: " << roi.pointCount
                  << ", min: [" << roi.minPt.x << ", " << roi.minPt.y << ", " << roi.minPt.z << "]"
                  << ", max: [" << roi.maxPt.x << ", " << roi.maxPt.y << ", " << roi.maxPt.z << "]"
                  << ", cropCloud: " << roi.cropCloud.size()
                  << ", file: " << roi.cropCloudFile << std::endl;
    }

    if (!pointCloudCropProcessOutput.visual.empty()) {
        cv::imwrite("./visual.jpg", pointCloudCropProcessOutput.visual);
    }

    return pointCloudCropProcessOutput.processFlag;
}
