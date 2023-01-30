cmake_minimum_required(VERSION 3.12.0)
project(task01)
set(USE_SANDBOX OFF CACHE BOOL "")
# CEF_ROOT修改为CEF的cmake文件夹所在路径
set(CEF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../cef/" CACHE PATH "") 
set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "CEF_RUNTIME_LIBRARY_FLAG")
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_RELEASE}" "${CEF_LIB_RELEASE}")
PRINT_CEF_CONFIG()
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOMOC ON)
add_executable(${PROJECT_NAME} WIN32
    ${CMAKE_CURRENT_LIST_DIR}/main.cc
)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Core)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Gui)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Widgets)
target_link_libraries(${PROJECT_NAME} PUBLIC libcef_lib libcef_dll_wrapper)
target_include_directories(${PROJECT_NAME} PUBLIC ${CEF_ROOT})
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<C_COMPILER_ID:MSVC>:/utf-8>")
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_custom_target(POST_COPY ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Core> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Gui> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Widgets> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:Qt5::Core>/../plugins/platforms $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms
  DEPENDS ${PROJECT_NAME}
)
COPY_FILES("${PROJECT_NAME}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR_RELEASE}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
COPY_FILES("${PROJECT_NAME}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")