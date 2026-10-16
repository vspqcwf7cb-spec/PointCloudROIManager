/**
 * @file DataBag.h
 * @brief Pipeline 数据袋（08 §6 / 09 §4）
 * @details 平台级类型擦除容器（std::any），约定键见 09 §4：input_cloud / roi_result / roi_clouds。
 */

#ifndef SBT_DATA_BAG_H_
#define SBT_DATA_BAG_H_

#include <any>
#include <string>
#include <unordered_map>

namespace sbt {
namespace proc {

/** @brief 键值型数据袋，用于 Pipeline 各阶段之间传递数据 */
class DataBag {
public:
    /** @brief 写入一个键值（拷贝） */
    template <typename T>
    void set(const std::string& key, const T& value) {
        store_[key] = value;
    }

    /** @brief 读取并校验类型；键缺失或类型不符返回 false */
    template <typename T>
    bool get(const std::string& key, T& out) const {
        auto it = store_.find(key);
        if (it == store_.end()) {
            return false;
        }
        const T* p = std::any_cast<T>(&it->second);
        if (!p) {
            return false;
        }
        out = *p;
        return true;
    }

    /** @brief 键是否存在 */
    bool has(const std::string& key) const { return store_.count(key) != 0; }

    /** @brief 写入整帧错误信息 */
    void setError(const std::string& msg) { error_ = msg; }

    /** @brief 读取整帧错误信息 */
    const std::string& error() const { return error_; }

    /** @brief 清空 */
    void clear() {
        store_.clear();
        error_.clear();
    }

private:
    std::unordered_map<std::string, std::any> store_;
    std::string error_;
};

}
}

#endif // SBT_DATA_BAG_H_
