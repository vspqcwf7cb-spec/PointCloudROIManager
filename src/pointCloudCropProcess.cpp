/**
 * @file: pointCloudCropProcess.cpp
 * @brief: 点云裁剪处理算子核心实现
 * @details: 读取 PCD/PLY 点云和 JSON/YAML 多 ROI 配置，基于 PCL 八叉树执行三维包围盒裁剪，
 *           输出每个 ROI 的点数、保存路径和可视化结果。
 * @author: Zhang Wenbo
 * @date: 2026/08/25
 * @version: v1.0.0
 */

#include "pointCloudCropProcess.h"

#include <pcl/common/common.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/octree/octree_search.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <json.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace sbt {

    namespace proc {

        namespace {

            using PointT = pcl::PointXYZI;
            using PointCloudT = pcl::PointCloud<PointT>;
            using PointCloudPtr = PointCloudT::Ptr;

            struct NamedRoi {
                std::string label;
                Eigen::Vector3f minPt = Eigen::Vector3f::Zero();
                Eigen::Vector3f maxPt = Eigen::Vector3f::Zero();
            };

            std::string toLower(std::string text) {
                std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });
                return text;
            }

            std::string trim(const std::string& text) {
                const size_t begin = text.find_first_not_of(" \t\r\n");
                if (begin == std::string::npos) {
                    return "";
                }
                const size_t end = text.find_last_not_of(" \t\r\n");
                return text.substr(begin, end - begin + 1);
            }

            std::string stripQuotes(const std::string& text) {
                std::string value = trim(text);
                if (value.size() >= 2 &&
                    ((value.front() == '"' && value.back() == '"') ||
                     (value.front() == '\'' && value.back() == '\''))) {
                    return value.substr(1, value.size() - 2);
                }
                return value;
            }

            std::string extensionOf(const std::string& path) {
                return toLower(fs::path(path).extension().string());
            }

            bool extractNumber(const std::string& line, const std::string& key, double& value) {
                const std::string pattern = key + ":";
                size_t pos = line.find(pattern);
                if (pos == std::string::npos) {
                    return false;
                }
                pos += pattern.size();
                while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
                    ++pos;
                }
                const size_t begin = pos;
                while (pos < line.size()) {
                    const char ch = line[pos];
                    if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E') {
                        ++pos;
                    }
                    else {
                        break;
                    }
                }
                if (begin == pos) {
                    return false;
                }
                try {
                    value = std::stod(line.substr(begin, pos - begin));
                }
                catch (...) {
                    return false;
                }
                return true;
            }

            cv::Point3d toCvPoint(const Eigen::Vector3f& point) {
                return cv::Point3d(point.x(), point.y(), point.z());
            }

            void normalizeRoi(NamedRoi& roi) {
                for (int i = 0; i < 3; ++i) {
                    if (roi.minPt[i] > roi.maxPt[i]) {
                        std::swap(roi.minPt[i], roi.maxPt[i]);
                    }
                }
            }

            bool loadRoisFromJson(const std::string& roiFile, std::vector<NamedRoi>& rois, std::string& errorMessage) {
                rois.clear();
                std::ifstream input(roiFile);
                if (!input.is_open()) {
                    errorMessage = "ROI 文件打开失败: " + roiFile;
                    return false;
                }

                try {
                    nlohmann::json root = nlohmann::json::parse(input);
                    if (!root.contains("multiroi") || !root["multiroi"].contains("rois") || !root["multiroi"]["rois"].is_array()) {
                        errorMessage = "JSON 缺少 multiroi.rois 数组: " + roiFile;
                        return false;
                    }
                    for (const auto& item : root["multiroi"]["rois"]) {
                        if (!item.contains("min_point") || !item.contains("max_point")) {
                            continue;
                        }
                        NamedRoi roi;
                        roi.label = item.value("name", "");
                        roi.minPt.x() = item["min_point"].value("x", 0.0f);
                        roi.minPt.y() = item["min_point"].value("y", 0.0f);
                        roi.minPt.z() = item["min_point"].value("z", 0.0f);
                        roi.maxPt.x() = item["max_point"].value("x", 0.0f);
                        roi.maxPt.y() = item["max_point"].value("y", 0.0f);
                        roi.maxPt.z() = item["max_point"].value("z", 0.0f);
                        normalizeRoi(roi);
                        rois.emplace_back(std::move(roi));
                    }
                }
                catch (const std::exception& e) {
                    errorMessage = std::string("JSON 解析失败: ") + e.what();
                    return false;
                }

                if (rois.empty()) {
                    errorMessage = "JSON 未解析到有效 ROI: " + roiFile;
                    return false;
                }
                return true;
            }

            bool loadRoisFromYaml(const std::string& roiFile, std::vector<NamedRoi>& rois, std::string& errorMessage) {
                rois.clear();
                std::ifstream input(roiFile);
                if (!input.is_open()) {
                    errorMessage = "ROI 文件打开失败: " + roiFile;
                    return false;
                }

                std::string line;
                bool inRois = false;
                bool hasCurrent = false;
                enum class PointMode { None, Min, Max } pointMode = PointMode::None;
                NamedRoi current;

                while (std::getline(input, line)) {
                    const size_t commentPos = line.find('#');
                    if (commentPos != std::string::npos) {
                        line.erase(commentPos);
                    }
                    std::string text = trim(line);
                    if (text.empty()) {
                        continue;
                    }
                    if (!inRois && text.rfind("rois", 0) == 0) {
                        inRois = true;
                        continue;
                    }
                    if (!inRois) {
                        continue;
                    }
                    if (text.front() == '-') {
                        if (hasCurrent) {
                            normalizeRoi(current);
                            rois.emplace_back(current);
                        }
                        current = NamedRoi();
                        hasCurrent = true;
                        pointMode = PointMode::None;
                        const size_t namePos = text.find("name");
                        if (namePos != std::string::npos) {
                            const size_t colonPos = text.find(':', namePos);
                            if (colonPos != std::string::npos) {
                                current.label = stripQuotes(text.substr(colonPos + 1));
                            }
                        }
                        continue;
                    }
                    if (!hasCurrent) {
                        continue;
                    }
                    if (text.find("name") != std::string::npos) {
                        const size_t colonPos = text.find(':');
                        if (colonPos != std::string::npos) {
                            current.label = stripQuotes(text.substr(colonPos + 1));
                        }
                    }
                    if (text.find("min_point") != std::string::npos) {
                        pointMode = PointMode::Min;
                    }
                    if (text.find("max_point") != std::string::npos) {
                        pointMode = PointMode::Max;
                    }

                    double value = 0.0;
                    if (pointMode != PointMode::None) {
                        Eigen::Vector3f& point = (pointMode == PointMode::Min) ? current.minPt : current.maxPt;
                        if (extractNumber(text, "x", value)) {
                            point.x() = static_cast<float>(value);
                        }
                        if (extractNumber(text, "y", value)) {
                            point.y() = static_cast<float>(value);
                        }
                        if (extractNumber(text, "z", value)) {
                            point.z() = static_cast<float>(value);
                        }
                    }
                }

                if (hasCurrent) {
                    normalizeRoi(current);
                    rois.emplace_back(current);
                }
                if (rois.empty()) {
                    errorMessage = "YAML 未解析到有效 ROI: " + roiFile;
                    return false;
                }
                return true;
            }

            bool loadRois(const std::string& roiFile, std::vector<NamedRoi>& rois, std::string& errorMessage) {
                if (extensionOf(roiFile) == ".json") {
                    return loadRoisFromJson(roiFile, rois, errorMessage);
                }
                return loadRoisFromYaml(roiFile, rois, errorMessage);
            }

            bool loadCloud(const std::string& cloudFile, PointCloudPtr& cloud, std::string& errorMessage) {
                cloud = std::make_shared<PointCloudT>();
                const std::string ext = extensionOf(cloudFile);
                int ret = -1;
                if (ext == ".ply") {
                    ret = pcl::io::loadPLYFile(cloudFile, *cloud);
                }
                else if (ext == ".pcd") {
                    ret = pcl::io::loadPCDFile(cloudFile, *cloud);
                }
                else {
                    errorMessage = "不支持的点云格式: " + ext;
                    return false;
                }
                if (ret < 0 || !cloud || cloud->empty()) {
                    errorMessage = "点云加载失败或为空: " + cloudFile;
                    return false;
                }
                return true;
            }

            std::string safeFileName(std::string text) {
                if (text.empty()) {
                    return "roi";
                }
                for (char& ch : text) {
                    if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') {
                        ch = '_';
                    }
                }
                return text;
            }

            std::string makeCropCloudFile(const PointCloudCropProcessInput& input, const std::string& label, size_t index) {
                fs::path saveDir = input.saveDir.empty() ? fs::path(".") : fs::path(input.saveDir);
                std::ostringstream name;
                name << safeFileName(label) << "_" << std::setw(2) << std::setfill('0') << index << ".ply";
                return (saveDir / name.str()).string();
            }

            bool saveCloudFile(const std::string& filePath, const PointCloudT& cloud) {
                std::error_code ec;
                fs::create_directories(fs::path(filePath).parent_path(), ec);
                if (ec) {
                    return false;
                }
                return pcl::io::savePLYFileBinary(filePath, cloud) >= 0;
            }

            std::vector<PointCloudPoint> toOutputCloud(const PointCloudT& cloud) {
                std::vector<PointCloudPoint> outputCloud;
                outputCloud.reserve(cloud.points.size());
                for (const PointT& point : cloud.points) {
                    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
                        continue;
                    }
                    PointCloudPoint outputPoint;
                    outputPoint.x = point.x;
                    outputPoint.y = point.y;
                    outputPoint.z = point.z;
                    outputPoint.intensity = point.intensity;
                    outputCloud.emplace_back(outputPoint);
                }
                return outputCloud;
            }
            cv::Point toCanvasPoint(double x, double y, double minX, double maxX, double minY, double maxY,
                                    const cv::Rect& area) {
                const double rangeX = std::max(maxX - minX, 1e-6);
                const double rangeY = std::max(maxY - minY, 1e-6);
                const int px = area.x + static_cast<int>((x - minX) / rangeX * area.width + 0.5);
                const int py = area.y + area.height - static_cast<int>((y - minY) / rangeY * area.height + 0.5);
                return cv::Point(px, py);
            }

            void drawText(cv::Mat& image, const std::string& text, const cv::Point& org, double scale,
                          const cv::Scalar& color, int thickness = 1) {
                cv::putText(image, text, org, cv::FONT_HERSHEY_SIMPLEX, scale, color, thickness, cv::LINE_AA);
            }

            cv::Mat drawVisual(const PointCloudT& cloud,
                               const std::vector<PointCloudRoiInfo>& roiInfos,
                               const std::vector<PointCloudT>& roiClouds) {
                const int width = 1200;
                const int height = 900;
                cv::Mat visual(height, width, CV_8UC3, cv::Scalar(245, 245, 245));
                cv::Rect plot(70, 135, width - 140, height - 225);
                cv::rectangle(visual, plot, cv::Scalar(200, 200, 200), 1, cv::LINE_AA);
                drawText(visual, "pointCloudCropProcess", cv::Point(70, 55), 0.8, cv::Scalar(30, 30, 30), 2);

                Eigen::Vector4f minPt, maxPt;
                pcl::getMinMax3D(cloud, minPt, maxPt);
                const double minX = minPt.x();
                const double maxX = maxPt.x();
                const double minY = minPt.y();
                const double maxY = maxPt.y();

                const size_t maxSamples = std::min<size_t>(cloud.size(), 220000);
                for (size_t i = 0; i < maxSamples; ++i) {
                    const size_t index = (i * 1103515245ULL + 12345ULL) % cloud.size();
                    const PointT& point = cloud.points[index];
                    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                        continue;
                    }
                    cv::Point p = toCanvasPoint(point.x, point.y, minX, maxX, minY, maxY, plot);
                    if (plot.contains(p)) {
                        cv::circle(visual, p, 1, cv::Scalar(165, 165, 165), -1, cv::LINE_AA);
                    }
                }

                static const cv::Scalar colors[] = {
                    cv::Scalar(0, 0, 255), cv::Scalar(0, 160, 0), cv::Scalar(255, 0, 0),
                    cv::Scalar(0, 160, 220), cv::Scalar(180, 0, 180), cv::Scalar(220, 120, 0)
                };

                for (size_t i = 0; i < roiClouds.size(); ++i) {
                    const cv::Scalar color = colors[i % (sizeof(colors) / sizeof(colors[0]))];
                    const PointCloudT& roiCloud = roiClouds[i];
                    const size_t roiSamples = std::min<size_t>(roiCloud.size(), 60000);
                    for (size_t j = 0; j < roiSamples; ++j) {
                        const size_t index = roiCloud.size() <= roiSamples
                            ? j
                            : (j * 1103515245ULL + 12345ULL) % roiCloud.size();
                        const PointT& point = roiCloud.points[index];
                        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                            continue;
                        }
                        cv::Point p = toCanvasPoint(point.x, point.y, minX, maxX, minY, maxY, plot);
                        if (plot.contains(p)) {
                            cv::circle(visual, p, 2, color, -1, cv::LINE_AA);
                        }
                    }
                }

                int legendX = 70;
                int legendY = 88;
                for (size_t i = 0; i < roiInfos.size(); ++i) {
                    const auto& roi = roiInfos[i];
                    const cv::Scalar color = colors[i % (sizeof(colors) / sizeof(colors[0]))];
                    cv::circle(visual, cv::Point(legendX + 8, legendY - 5), 5, color, -1, cv::LINE_AA);
                    drawText(visual, roi.label + ": " + std::to_string(roi.pointCount) + " pts",
                             cv::Point(legendX + 22, legendY), 0.45, color, 1);
                    legendX += 175;
                    if (legendX > width - 210) {
                        legendX = 70;
                        legendY += 24;
                    }
                }

                for (size_t i = 0; i < roiInfos.size(); ++i) {
                    const auto& roi = roiInfos[i];
                    const cv::Scalar color = colors[i % (sizeof(colors) / sizeof(colors[0]))];
                    cv::Point p1 = toCanvasPoint(roi.minPt.x, roi.minPt.y, minX, maxX, minY, maxY, plot);
                    cv::Point p2 = toCanvasPoint(roi.maxPt.x, roi.maxPt.y, minX, maxX, minY, maxY, plot);
                    cv::Rect rect(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::abs(p1.x - p2.x), std::abs(p1.y - p2.y));
                    rect &= plot;
                    if (rect.width > 0 && rect.height > 0) {
                        cv::rectangle(visual, rect, color, 2, cv::LINE_AA);
                    }
                }

                drawText(visual, "top view (X-Y), gray: sampled cloud, colored points/boxes: ROI crop result", cv::Point(70, height - 45), 0.55, cv::Scalar(80, 80, 80), 1);
                return visual;
            }

            void finishOutput(PointCloudCropProcessOutput& output, int flag, const std::string& message,
                              const std::chrono::steady_clock::time_point& begin) {
                output.processFlag = flag;
                output.processMessage = message;
                output.processTime = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - begin).count() / 1000.0);
            }

        }

        int pointCloudCropProcess(const PointCloudCropProcessInput& pointCloudCropProcessInput,
                                  PointCloudCropProcessOutput& pointCloudCropProcessOutput) {
            const auto begin = std::chrono::steady_clock::now();
            pointCloudCropProcessOutput = PointCloudCropProcessOutput();

            if (pointCloudCropProcessInput.cloudFile.empty()) {
                finishOutput(pointCloudCropProcessOutput, -1, "cloudFile 为空", begin);
                return pointCloudCropProcessOutput.processFlag;
            }
            if (pointCloudCropProcessInput.roiFile.empty()) {
                finishOutput(pointCloudCropProcessOutput, -1, "roiFile 为空", begin);
                return pointCloudCropProcessOutput.processFlag;
            }

            try {
                PointCloudPtr srcCloud;
                std::string errorMessage;
                if (!loadCloud(pointCloudCropProcessInput.cloudFile, srcCloud, errorMessage)) {
                    finishOutput(pointCloudCropProcessOutput, -1, errorMessage, begin);
                    return pointCloudCropProcessOutput.processFlag;
                }

                std::vector<NamedRoi> rois;
                if (!loadRois(pointCloudCropProcessInput.roiFile, rois, errorMessage)) {
                    finishOutput(pointCloudCropProcessOutput, -1, errorMessage, begin);
                    return pointCloudCropProcessOutput.processFlag;
                }

                Eigen::Vector4f cloudMin, cloudMax;
                pcl::getMinMax3D(*srcCloud, cloudMin, cloudMax);
                const float extent = std::max((cloudMax.head<3>() - cloudMin.head<3>()).norm(), 1.0f);
                const float octreeResolution = std::max(extent / 1024.0f, 0.05f);

                pcl::octree::OctreePointCloudSearch<PointT> octree(octreeResolution);
                octree.setInputCloud(srcCloud);
                octree.addPointsFromInputCloud();

                pointCloudCropProcessOutput.rois.reserve(rois.size());
                std::vector<PointCloudT> visualRoiClouds;
                if (pointCloudCropProcessInput.isVisual) {
                    visualRoiClouds.reserve(rois.size());
                }
                for (size_t i = 0; i < rois.size(); ++i) {
                    const NamedRoi& roi = rois[i];
                    std::vector<int> indices;
                    octree.boxSearch(roi.minPt, roi.maxPt, indices);

                    PointCloudT cropped;
                    cropped.points.reserve(indices.size());
                    for (int index : indices) {
                        if (index >= 0 && index < static_cast<int>(srcCloud->points.size())) {
                            cropped.points.emplace_back(srcCloud->points[index]);
                        }
                    }
                    cropped.width = static_cast<std::uint32_t>(cropped.points.size());
                    cropped.height = 1;
                    cropped.is_dense = srcCloud->is_dense;

                    PointCloudRoiInfo roiInfo;
                    roiInfo.label = roi.label.empty() ? ("roi_" + std::to_string(i + 1)) : roi.label;
                    roiInfo.minPt = toCvPoint(roi.minPt);
                    roiInfo.maxPt = toCvPoint(roi.maxPt);
                    roiInfo.pointCount = static_cast<int>(cropped.points.size());

                    if (pointCloudCropProcessInput.isCrop && !cropped.empty()) {
                        roiInfo.cropCloud = toOutputCloud(cropped);
                    }

                    if (pointCloudCropProcessInput.isSave && !cropped.empty()) {
                        roiInfo.cropCloudFile = makeCropCloudFile(pointCloudCropProcessInput, roiInfo.label, i);
                        if (!saveCloudFile(roiInfo.cropCloudFile, cropped)) {
                            roiInfo.cropCloudFile.clear();
                        }
                    }

                    if (pointCloudCropProcessInput.isVisual) {
                        visualRoiClouds.emplace_back(std::move(cropped));
                    }
                    pointCloudCropProcessOutput.rois.emplace_back(std::move(roiInfo));
                }

                if (pointCloudCropProcessInput.isVisual) {
                    pointCloudCropProcessOutput.visual = drawVisual(*srcCloud, pointCloudCropProcessOutput.rois, visualRoiClouds);
                }

                finishOutput(pointCloudCropProcessOutput, 0, "SUCCESS", begin);
                return pointCloudCropProcessOutput.processFlag;
            }
            catch (const std::exception& e) {
                finishOutput(pointCloudCropProcessOutput, -100, e.what(), begin);
                return pointCloudCropProcessOutput.processFlag;
            }
            catch (...) {
                finishOutput(pointCloudCropProcessOutput, -100, "UNKNOWN_ERROR", begin);
                return pointCloudCropProcessOutput.processFlag;
            }
        }

    }
}
