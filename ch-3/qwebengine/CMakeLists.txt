cmake_minimum_required(VERSION 3.12.0)
project(task01)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets WebEngineCore Quick PrintSupport Network Qml QuickWidgets Positioning WebChannel)
set(CMAKE_AUTOMOC ON)
add_executable(${PROJECT_NAME} WIN32
    ${CMAKE_CURRENT_LIST_DIR}/main.cc
)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Core)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Gui)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Widgets)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::WebEngineCore)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::WebEngineWidgets)
add_custom_target(POST_COPY ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Core> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Gui> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Widgets> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::WebEngineWidgets> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::WebEngineCore> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Quick> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::PrintSupport> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Network> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Qml> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::QuickWidgets> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Positioning> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::WebChannel> $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE_DIR:Qt5::Core>/libGLESv2d.dll $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:Qt5::Core>/../plugins/platforms $<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms
  COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:Qt5::Core>/../resources $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE_DIR:Qt5::Core>/QtWebEngineProcess$<$<CONFIG:Debug>:d>.exe" $<TARGET_FILE_DIR:${PROJECT_NAME}>
  DEPENDS ${PROJECT_NAME}
)
