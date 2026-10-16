/**
 * @file IOperator.h
 * @brief 平台 Operator 抽象基类（08 §6）
 * @details 由 VisionAlgorithmPlatform 提供，本模块只 include；v1 以最小桩形式落地以便单 DLL 编译。
 */

#ifndef SBT_I_OPERATOR_H_
#define SBT_I_OPERATOR_H_

#include <string>

#include "DataBag.h"
#include "Export.h"
#include "ManagerState.h"
#include "OpConfig.h"

namespace sbt {
namespace proc {

/**
 * @brief 平台 Operator 基类
 */
class SBT_OPS_API IOperator {
public:
    virtual ~IOperator() = default;

    /** @brief 进程启动时调用一次，失败则平台不下发数据 */
    virtual bool init(const OpConfig& config) = 0;

    /** @brief 每帧调用 */
    virtual bool execute(const DataBag& input, DataBag& output) = 0;

    /** @brief 进程退出 / 卸载时调用，必须幂等 */
    virtual void release() = 0;

    /** @brief Operator 名称 */
    virtual std::string name() const = 0;

    /** @brief 运行状态 */
    virtual ManagerState state() const = 0;
};

}
}

#endif // SBT_I_OPERATOR_H_
