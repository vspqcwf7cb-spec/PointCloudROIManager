/**
 * @file Export.h
 * @brief 跨 DLL 导出宏 SBT_OPS_API
 * @details 统一算子动态库的 dllexport / dllimport 范式（参考 pointCloudCropProcess.h）。
 *          构建库时定义 SBT_CV_OPS_EXPORTS 打开 dllexport，消费方走 dllimport。
 */

#ifndef SBT_EXPORT_H_
#define SBT_EXPORT_H_

#if defined(_WIN32)
    #if defined(SBT_CV_OPS_EXPORTS)
        #define SBT_OPS_API __declspec(dllexport)
    #else
        #define SBT_OPS_API __declspec(dllimport)
    #endif
#else
    #define SBT_OPS_API __attribute__((visibility("default")))
#endif

#endif // SBT_EXPORT_H_
