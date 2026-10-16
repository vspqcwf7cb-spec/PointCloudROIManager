/**
 * @file PointCloudROIManager.h
 * @brief 对外门面（Facade），调度 Config/Parser/Extractor/ResultManager（07 模块）
 * @details Manager 不做具体算法，只做调度、状态机与错误聚合。
 *          头文件禁止出现 PCL / JSON / 具体 Extractor 类型，全部用前置声明 + unique_ptr。
 */

#ifndef SBT_POINT_CLOUD_ROI_MANAGER_H_
#define SBT_POINT_CLOUD_ROI_MANAGER_H_

#include <memory>
#include <mutex>
#include <string>

#include "Export.h"
#include "ManagerState.h"
#include "ROIManagerResult.h"

namespace sbt {
namespace proc {

class ROIConfigManager;
class ROIExtractor;
class ROIResultManager;
struct PointCloudData;

/**
 * @brief 点云 ROI 管理核心
 */
class SBT_OPS_API PointCloudROIManager {
public:
    PointCloudROIManager();
    ~PointCloudROIManager();

    /**
     * @brief 加载配置、构建 Extractor，状态进入 Ready（幂等，可重复调用）
     * @param configPath [in] ROI 配置文件路径
     * @return true=成功
     */
    bool init(const std::string& configPath);

    /**
     * @brief 执行整车点云 → 全部 ROI 点云的完整调用
     * @param cloud [in] 整车点云（shared_ptr<const>，跨 DLL 共享）
     * @return true=整批成功
     */
    bool execute(std::shared_ptr<const PointCloudData> cloud);

    /** @brief 只读整批结果引用 */
    const ROIManagerResult& getResult() const;

    /** @brief 状态查询 */
    ManagerState getState() const;

    /** @brief 最近一次失败原因 */
    std::string getLastError() const;

    /** @brief L2 覆盖 L3：Octree 分辨率覆盖（13 §8）；<=0 忽略，取配置值 */
    void setOctreeResolution(float mm);

private:
    std::unique_ptr<ROIConfigManager> configManager_;
    std::unique_ptr<ROIExtractor> extractor_;
    std::unique_ptr<ROIResultManager> resultManager_;

    ManagerState state_ = ManagerState::Uninitialized;
    ROIManagerResult lastResult_;
    std::string lastError_;
    float resolutionOverrideMM_ = -1.0f;   ///< L2 分辨率覆盖，<=0 未设置
    std::mutex executeMutex_;              ///< 串行化 execute，防 Octree 重入 build
};

}
}

#endif // SBT_POINT_CLOUD_ROI_MANAGER_H_
