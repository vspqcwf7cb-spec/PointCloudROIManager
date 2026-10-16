/**
 * @file: pointCloudCropProcess.h
 * @brief: 点云裁剪处理算子 API 接口
 * @details: 基于三维包围盒 ROI 对 PCD/PLY 点云进行裁剪，支持从 JSON/YAML 配置文件读取多个 ROI，
 *           可按需输出裁剪点云数据、保存裁剪点云并输出无窗口俯视投影可视化结果。
 * @author: Zhang Wenbo
 * @date: 2026/08/25
 * @version: v1.0.0
 * @history: v1.0.0 初始版本，支持多 ROI 三维包围盒点云裁剪。
 */

#ifndef SBT_POINT_CLOUD_CROP_PROCESS_H_
#define SBT_POINT_CLOUD_CROP_PROCESS_H_

#ifdef SBT_CV_OPS_EXPORTS
#define SBT_POINT_CLOUD_CROP_PROCESS_API __declspec(dllexport)
#else
#define SBT_POINT_CLOUD_CROP_PROCESS_API __declspec(dllimport)
#endif

#include <string>
#include <vector>
#include "opencv2/core.hpp"

namespace sbt {

    namespace proc {

        /**
         * @struct PointCloudPoint
         * @brief 点云点数据结构
         * @details 用于在 API 层输出裁剪后的点云数据，避免在公共头文件中暴露 PCL 类型。
         * @member x 点 X 坐标，单位 mm
         * @member y 点 Y 坐标，单位 mm
         * @member z 点 Z 坐标，单位 mm
         * @member intensity 点强度值
         */
        struct PointCloudPoint {
            float x = 0.0f;             ///< 点 X 坐标，单位 mm
            float y = 0.0f;             ///< 点 Y 坐标，单位 mm
            float z = 0.0f;             ///< 点 Z 坐标，单位 mm
            float intensity = 0.0f;     ///< 点强度值
        };

        /**
         * @struct PointCloudRoiInfo
         * @brief 点云 ROI 信息和裁剪结果
         * @member label ROI 标签名称
         * @member minPt 三维包围盒最小点，单位 mm
         * @member maxPt 三维包围盒最大点，单位 mm
         * @member pointCount 裁剪后的点数
         * @member cropCloud 裁剪后的点云数据，isCrop=false 时为空
         * @member cropCloudFile 裁剪点云保存路径，未保存时为空
         */
        struct PointCloudRoiInfo {
            std::string label;              ///< ROI 标签名称
            cv::Point3d minPt;              ///< 三维包围盒最小点，单位 mm
            cv::Point3d maxPt;              ///< 三维包围盒最大点，单位 mm
            int pointCount = 0;                         ///< 裁剪后的点数
            std::vector<PointCloudPoint> cropCloud;        ///< 裁剪后的点云数据
            std::string cropCloudFile;                  ///< 裁剪点云保存路径
        };

        /**
         * @struct PointCloudCropProcessInput
         * @brief 点云裁剪处理输入参数
         * @member cloudFile 输入点云文件路径，支持 pcd/ply
         * @member roiFile ROI 配置文件路径，支持 JSON/YAML
         * @member isCrop 是否输出每个 ROI 的裁剪点云数据
         * @member isSave 是否保存每个 ROI 的裁剪点云
         * @member saveDir 裁剪点云保存目录，为空时保存到当前目录
         * @member isVisual 是否生成无窗口俯视投影可视化图像
         */
        struct PointCloudCropProcessInput {
            std::string cloudFile;          ///< 输入点云文件路径，支持 pcd/ply
            std::string roiFile;            ///< ROI 配置文件路径，支持 JSON/YAML
            bool isCrop = false;             ///< 是否输出每个 ROI 的裁剪点云数据
            bool isSave = true;              ///< 是否保存每个 ROI 的裁剪点云
            std::string saveDir;            ///< 裁剪点云保存目录，为空时保存到当前目录
            bool isVisual = false;          ///< 是否生成无窗口俯视投影可视化图像
        };

        /**
         * @struct PointCloudCropProcessOutput
         * @brief 点云裁剪处理输出结果
         * @member processFlag 处理状态码，0=成功，非0=失败
         * @member processTime 处理耗时，单位 ms
         * @member processMessage 处理消息
         * @member rois ROI 裁剪结果列表
         * @member visual 无窗口可视化图像
         */
        struct PointCloudCropProcessOutput {
            int processFlag = -1;                       ///< 处理状态码，0=成功，非0=失败
            float processTime = 0.0f;                   ///< 处理耗时，单位 ms
            std::string processMessage;                 ///< 处理消息
            std::vector<PointCloudRoiInfo> rois;        ///< ROI 裁剪结果列表
            cv::Mat visual;                             ///< 无窗口可视化图像
        };

        /**
         * @brief 点云裁剪处理算子接口
         * @details 加载输入点云，解析 ROI 配置文件，按三维包围盒逐个裁剪点云，并按需输出裁剪点云数据、保存裁剪点云和可视化图像。
         * @param pointCloudCropProcessInput [in] 点云裁剪处理输入参数
         * @param pointCloudCropProcessOutput [out] 点云裁剪处理输出结果
         * @return int 0=成功，非0=失败
         */
        extern "C++" SBT_POINT_CLOUD_CROP_PROCESS_API int pointCloudCropProcess(
            const PointCloudCropProcessInput& pointCloudCropProcessInput,
            PointCloudCropProcessOutput& pointCloudCropProcessOutput
        );

    }
}

#endif // SBT_POINT_CLOUD_CROP_PROCESS_H_
