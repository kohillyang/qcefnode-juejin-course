cmake_minimum_required(VERSION 3.13.0)
project (napi)
set(OpenCV_DIR ${CMAKE_CURRENT_LIST_DIR}/../opencv4.5.4/build)
set(Python_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../Python3.7.9)
find_package (Python REQUIRED)
function(webpack2qrc inFile outFile prefix)
  add_custom_command(
    OUTPUT 
    ${outFile}
    DEPENDS 
    ${inFile}
    ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/webpackmanifest2qrc.py
    COMMAND 
    ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/webpackmanifest2qrc.py --prefix=${prefix} --input=${inFile} --output=${outFile}
  )
endfunction()
webpack2qrc(
  ${CMAKE_SOURCE_DIR}/web/build/asset-manifest.json 
  ${CMAKE_SOURCE_DIR}/web/build/web.qrc
  "/web"
)
set(OpenCV_DIR ${CMAKE_CURRENT_LIST_DIR}/../opencv4.5.4/build)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(OpenCV REQUIRED COMPONENTS world)
get_target_property(__dll_dbg opencv_world IMPORTED_LOCATION_DEBUG)
get_target_property(__dll_release opencv_world  IMPORTED_LOCATION_RELEASE)
set(USE_SANDBOX OFF CACHE BOOL "")
# CEF_ROOT修改为CEF的cmake文件夹所在路径
set(CEF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../cef/" CACHE PATH "") 
set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "CEF_RUNTIME_LIBRARY_FLAG")
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_RELEASE}" "${CEF_LIB_RELEASE}")
PRINT_CEF_CONFIG()
add_definitions(-DNAPI_VERSION=9)
set (CV_NAPI_EXT_NAME "cv")
add_library(${CV_NAPI_EXT_NAME} SHARED 
    ${CMAKE_CURRENT_LIST_DIR}/js_native_api_types.h
    ${CMAKE_CURRENT_LIST_DIR}/js_native_api.h
    ${CMAKE_CURRENT_LIST_DIR}/node_api.h
    ${CMAKE_CURRENT_LIST_DIR}/node_api_types.h
    ${CMAKE_CURRENT_LIST_DIR}/cv.cc
)
target_link_libraries(${CV_NAPI_EXT_NAME} PRIVATE ${OpenCV_LIBRARIES})
target_link_libraries(${CV_NAPI_EXT_NAME} PRIVATE Qt5::Core)
target_link_libraries(${CV_NAPI_EXT_NAME} PRIVATE libcef_lib)
target_compile_definitions(${CV_NAPI_EXT_NAME} PRIVATE -DHOST_BINARY="${HOST_BINARY}")
target_include_directories(${CV_NAPI_EXT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_options(${CV_NAPI_EXT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/source-charset:utf-8>")  
target_compile_options(${CV_NAPI_EXT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/execution-charset:utf-8>") 
set_target_properties(${CV_NAPI_EXT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
add_executable(${PROJECT_NAME} WIN32
    ${CMAKE_CURRENT_LIST_DIR}/main.cc
    ${CMAKE_SOURCE_DIR}/web/build/web.qrc
)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Core)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Gui)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Widgets)
target_link_libraries(${PROJECT_NAME} PUBLIC libcef_lib libcef_dll_wrapper)
target_include_directories(${PROJECT_NAME} PUBLIC ${CEF_ROOT})
COPY_FILES("${PROJECT_NAME}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR_RELEASE}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
COPY_FILES("${PROJECT_NAME}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
add_custom_target(POST_COPY ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Core> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Gui> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Widgets> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:Qt5::Core>/../plugins/platforms $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_LIST_DIR}/xitu.png $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<$<CONFIG:Debug>:${__dll_dbg}>$<$<CONFIG:Release>:${__dll_release}>$<$<CONFIG:RelWithDebInfo>:${__dll_release}>$<$<CONFIG:MinSizeRel>:${__dll_release}>" $<TARGET_FILE_DIR:${PROJECT_NAME}>
  DEPENDS ${PROJECT_NAME}
)
add_dependencies(${PROJECT_NAME} ${CV_NAPI_EXT_NAME})
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/source-charset:utf-8>")  
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/execution-charset:utf-8>") 
