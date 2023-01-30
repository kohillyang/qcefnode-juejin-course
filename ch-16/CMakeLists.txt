cmake_minimum_required(VERSION 3.19.0)
cmake_policy(SET CMP0112 NEW)
project(task01)
set(USE_SANDBOX OFF CACHE BOOL "")
set(CEF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../cef/" CACHE PATH "") 
set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "CEF_RUNTIME_LIBRARY_FLAG")
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_RELEASE}" "${CEF_LIB_RELEASE}")
PRINT_CEF_CONFIG()
set(Python_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../Python3.7.9)
find_package (Python REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(USE_SANDBOX OFF CACHE BOOL "")
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
add_executable(${PROJECT_NAME} WIN32
    ${CMAKE_CURRENT_LIST_DIR}/main.cc
    ${CMAKE_CURRENT_LIST_DIR}/web/build/web.qrc
)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Core)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Gui)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Widgets)
target_link_libraries(${PROJECT_NAME} PUBLIC libcef_lib libcef_dll_wrapper)
target_include_directories(${PROJECT_NAME} PUBLIC ${CEF_ROOT})
add_custom_target(POST_COPY ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Core> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Gui> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Widgets> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:Qt5::Core>/../plugins/platforms $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms
  DEPENDS ${PROJECT_NAME}
)
COPY_FILES("${PROJECT_NAME}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR_RELEASE}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
COPY_FILES("${PROJECT_NAME}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/source-charset:utf-8>")  
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/execution-charset:utf-8>") 