/**
 * @file PointCloudData.h
 * @brief 跨模块点云容器（02 §5.2）
 * @details 跨 DLL 边界一律用 std::shared_ptr<const PointCloudData>，禁止值拷贝（02 §5.6）。
 */

#ifndef SBT_POINT_CLOUD_DATA_H_
#define SBT_POINT_CLOUD_DATA_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ROITypes.h"

namespace sbt {
namespace proc {

/** @brief 平台无关点云容器 */
struct PointCloudData {
    std::vector<PointXYZI> points;   ///< 点集
    std::string coordinate;          ///< 坐标系："robot" / "vehicle" / "camera" / "world"
    std::uint64_t timestamp = 0;     ///< 采集时间，单位 ms
};

}
}

#endif // SBT_POINT_CLOUD_DATA_H_
