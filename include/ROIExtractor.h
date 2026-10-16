/**
 * @file ROIExtractor.h
 * @brief ROI 空间提取抽象接口 + Box 实现（06 模块）
 * @details 强制约束：#include <pcl/...> 只出现在 BoxROIExtractor.cpp，本头不暴露 PCL 类型
 *          （BoxROIExtractor 内部用 pimpl 持有 Octree）。
 */

#ifndef SBT_ROI_EXTRACTOR_H_
#define SBT_ROI_EXTRACTOR_H_

#include <memory>
#include <vector>

#include "Export.h"
#include "PointCloudData.h"
#include "ROIRegion.h"
#include "ROIResult.h"

namespace sbt {
namespace proc {

/**
 * @brief ROI 提取抽象接口
 */
class SBT_OPS_API ROIExtractor {
public:
    virtual ~ROIExtractor() = default;

    /**
     * @brief 构建搜索结构；同一点云多次 build 不重复构建（11 §8.2）
     * @param cloud [in] 完整点云（只读）
     * @return true=构建成功；false=点云为空 / OOM
     */
    virtual bool build(std::shared_ptr<const PointCloudData> cloud) = 0;

    /** @brief 对单个 ROI 提取 */
    virtual ROIResult extract(const ROIRegion& region) = 0;

    /** @brief 一次性对全部 ROI 提取 */
    virtual std::vector<ROIResult> extractAll(const std::vector<ROIRegion>& regions) = 0;
};

/**
 * @brief 基于 PCL Octree 的 Box ROI 提取器（v1 唯一实现）
 */
class SBT_OPS_API BoxROIExtractor : public ROIExtractor {
public:
    BoxROIExtractor();
    explicit BoxROIExtractor(float octreeResolutionMM);
    ~BoxROIExtractor() override;

    bool build(std::shared_ptr<const PointCloudData> cloud) override;
    ROIResult extract(const ROIRegion& region) override;
    std::vector<ROIResult> extractAll(const std::vector<ROIRegion>& regions) override;

    /** @brief 设置 ROI 并行线程数（v1 预留：串行，见实现注释） */
    void setThreadCount(int n);

    /** @brief 设置 Octree 叶子尺寸；<=0 或非有限值 clamp 到 1 mm */
    void setOctreeResolution(float mm);

private:
    struct Impl;                     ///< 持有 PCL Octree，头文件不暴露 PCL 类型
    std::unique_ptr<Impl> impl_;
};

}
}

#endif // SBT_ROI_EXTRACTOR_H_
