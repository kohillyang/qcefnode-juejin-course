cmake_minimum_required(VERSION 3.13.0)
project (napi)
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
target_compile_definitions(${PROJECT_NAME} PRIVATE -DHOST_BINARY="${HOST_BINARY}")
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/node.lib)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")