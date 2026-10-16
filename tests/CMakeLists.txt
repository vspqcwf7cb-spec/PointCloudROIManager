# 轻量自测（不依赖 GTest）：加载 data/4.ply + data/roi_single.json，验证单 ROI 裁剪

add_executable(test_roi_manager test_roi_manager.cpp)
target_link_libraries(test_roi_manager
    PRIVATE
        sbt_cv_ops
        ${PCL_LIBRARIES}
)
target_include_directories(test_roi_manager PRIVATE ${PCL_INCLUDE_DIRS})

set_target_properties(test_roi_manager PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/install
)

# 注册为 ctest 用例；从 install/ 运行以解析相对路径 ../data/...
add_test(NAME test_roi_manager COMMAND test_roi_manager)
set_tests_properties(test_roi_manager PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/install
    LABELS "unit"
)
