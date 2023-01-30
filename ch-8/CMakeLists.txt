cmake_minimum_required(VERSION 3.13.0)
project (napi)
set(OpenCV_DIR ${CMAKE_CURRENT_LIST_DIR}/../opencv4.5.4/build)
find_package(OpenCV REQUIRED COMPONENTS world)
get_target_property(__dll_dbg opencv_world IMPORTED_LOCATION_DEBUG)
get_target_property(__dll_release opencv_world  IMPORTED_LOCATION_RELEASE)
add_definitions(-DNAPI_VERSION=4)
add_library(${PROJECT_NAME} SHARED 
    ${CMAKE_CURRENT_LIST_DIR}/js_native_api_types.h
    ${CMAKE_CURRENT_LIST_DIR}/js_native_api.h
    ${CMAKE_CURRENT_LIST_DIR}/node_api.h
    ${CMAKE_CURRENT_LIST_DIR}/node_api_types.h
    ${CMAKE_CURRENT_LIST_DIR}/main.cc
    ${CMAKE_CURRENT_LIST_DIR}/win_delay_load_hook.cc
)
set(HOST_BINARY node.exe)
target_link_options(${PROJECT_NAME} PRIVATE "/DELAYLOAD:${HOST_BINARY}")
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBRARIES})
message(INFO ${OpenCV_LIBRARIES})
target_compile_definitions(${PROJECT_NAME} PRIVATE -DHOST_BINARY="${HOST_BINARY}")
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/node.lib)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
add_custom_target(POST_COPY ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<$<CONFIG:Debug>:${__dll_dbg}>$<$<CONFIG:Release>:${__dll_release}>$<$<CONFIG:RelWithDebInfo>:${__dll_release}>$<$<CONFIG:MinSizeRel>:${__dll_release}>" $<TARGET_FILE_DIR:${PROJECT_NAME}>
  DEPENDS ${PROJECT_NAME}
)