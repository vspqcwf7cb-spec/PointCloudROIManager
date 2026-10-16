/**
 * @file BoxROIExtractor.cpp
 * @brief BoxROIExtractor 实现（06 模块）
 * @details 唯一 include <pcl/...> 的位置。基于 OctreePointCloudSearch 做 Box 裁剪。
 *          pcl::PointXYZI 与平台 PointXYZI 的转换集中在本 .cpp，不泄漏头文件。
 */

#include "ROIExtractor.h"

#include <pcl/octree/octree_search.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <new>

namespace sbt {
namespace proc {

namespace {

using PclPoint = pcl::PointXYZI;
using PclCloud = pcl::PointCloud<PclPoint>;
using PclOctree = pcl::octree::OctreePointCloudSearch<PclPoint>;

}  // namespace

struct BoxROIExtractor::Impl {
    float resolutionMM = 5.0f;                 ///< Octree 叶子尺寸，单位 mm
    int threadCount = 0;                       ///< 预留：0=自动；v1 串行
    std::shared_ptr<const PointCloudData> source;   ///< 只读源点云
    const PointCloudData* builtCloudPtr = nullptr;  ///< Octree 复用判定（11 §8.2）
    std::size_t builtPointCount = 0;
    PclCloud::Ptr pclCloud;
    std::unique_ptr<PclOctree> octree;
    bool built = false;
};

BoxROIExtractor::BoxROIExtractor() : impl_(std::make_unique<Impl>()) {}

BoxROIExtractor::BoxROIExtractor(float octreeResolutionMM) : impl_(std::make_unique<Impl>()) {
    setOctreeResolution(octreeResolutionMM);
}

BoxROIExtractor::~BoxROIExtractor() = default;

void BoxROIExtractor::setThreadCount(int n) {
    impl_->threadCount = (n < 1) ? 0 : n;
}

void BoxROIExtractor::setOctreeResolution(float mm) {
    // <=0 或非有限值 clamp 到 1 mm（06 §9）
    impl_->resolutionMM = (!std::isfinite(mm) || mm <= 0.0f) ? 1.0f : mm;
}

bool BoxROIExtractor::build(std::shared_ptr<const PointCloudData> cloud) {
    if (!cloud || cloud->points.empty()) {
        impl_->built = false;
        return false;  // InputCloudEmpty（2001）
    }

    // Octree 复用：同一点云指针 + 相同点数 → 跳过重建（11 §8.2 / role §6.5）
    if (impl_->built && impl_->builtCloudPtr == cloud.get() &&
        impl_->builtPointCount == cloud->points.size()) {
        return true;
    }

    impl_->source = std::move(cloud);
    const auto& src = impl_->source->points;

    // hot path: resize + 逐字段拷贝（11 §8.1）
    PclCloud::Ptr pclCloud = std::make_shared<PclCloud>();
    pclCloud->points.resize(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
        pclCloud->points[i].x = src[i].x;
        pclCloud->points[i].y = src[i].y;
        pclCloud->points[i].z = src[i].z;
        pclCloud->points[i].intensity = src[i].intensity;
    }
    pclCloud->width = static_cast<std::uint32_t>(pclCloud->points.size());
    pclCloud->height = 1;
    pclCloud->is_dense = true;

    try {
        auto octree = std::make_unique<PclOctree>(impl_->resolutionMM);
        octree->setInputCloud(pclCloud);
        octree->addPointsFromInputCloud();
        impl_->pclCloud = std::move(pclCloud);
        impl_->octree = std::move(octree);
    } catch (const std::bad_alloc&) {
        impl_->built = false;
        return false;  // InputCloudOOM（2003）
    }

    impl_->builtCloudPtr = impl_->source.get();
    impl_->builtPointCount = impl_->source->points.size();
    impl_->built = true;
    return true;
}

ROIResult BoxROIExtractor::extract(const ROIRegion& region) {
    ROIResult result;
    result.name = region.name;

    if (!impl_->built || !impl_->octree) {
        result.success = false;
        result.message = "extractor not built";  // ExtractorNotBuilt（3001）
        return result;
    }
    if (region.type != ROIType::Box) {
        result.success = false;
        result.message = "unsupported roi type";
        return result;
    }

    std::vector<int> indices;
    indices.reserve(256);
    // PCL boxSearch 签名：boxSearch(const Eigen::Vector3f&, const Eigen::Vector3f&, Indices&)
    impl_->octree->boxSearch(region.min, region.max, indices);

    if (indices.empty()) {
        result.success = true;   // 0 命中不算错误（NoHit warn 由 Manager 统一打）
        result.pointCount = 0;
        result.message = "no hit";
        return result;
    }

    // 反向拷贝：PCL indices → 平台 PointCloudData（11 §8.4 S1，reserve 预分配）
    auto roiCloud = std::make_shared<PointCloudData>();
    roiCloud->coordinate = impl_->source->coordinate;
    roiCloud->timestamp = impl_->source->timestamp;
    roiCloud->points.reserve(indices.size());
    for (int idx : indices) {
        const auto& s = impl_->source->points[static_cast<std::size_t>(idx)];
        PointXYZI p;
        p.x = s.x;
        p.y = s.y;
        p.z = s.z;
        p.intensity = s.intensity;
        roiCloud->points.push_back(p);
    }

    result.cloud = roiCloud;  // shared_ptr<PointCloudData> → shared_ptr<const PointCloudData>
    result.pointCount = indices.size();
    result.success = true;
    return result;
}

std::vector<ROIResult> BoxROIExtractor::extractAll(const std::vector<ROIRegion>& regions) {
    std::vector<ROIResult> results;
    results.reserve(regions.size());

    // v1 串行：PCL OctreePointCloudSearch::boxSearch 内部通过 const_cast 复用共享搜索缓冲，
    // 并非线程安全（文档 06 §8.3 的"多线程无写冲突"在 PCL 实现下不成立），串行保证正确性。
    for (const auto& region : regions) {
        results.push_back(extract(region));
    }
    return results;
}

}  // namespace proc
}  // namespace sbt
