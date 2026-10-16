/**
 * @file ManagerState.h
 * @brief PointCloudROIManager 生命周期状态机枚举
 * @details 被 PointCloudROIManager 与 Operator 的 state() 接口共同引用，独立成头避免循环依赖。
 */

#ifndef SBT_MANAGER_STATE_H_
#define SBT_MANAGER_STATE_H_

namespace sbt {
namespace proc {

/** @brief Manager 生命周期状态 */
enum class ManagerState {
    Uninitialized,   ///< 未 init
    Ready,           ///< init 成功，等待 execute
    Executing,       ///< execute 进行中
    Done,            ///< execute 成功完成
    Failed           ///< 任一阶段失败
};

}
}

#endif // SBT_MANAGER_STATE_H_
