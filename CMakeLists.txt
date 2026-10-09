cmake_minimum_required(VERSION 3.20)
project(N3DLiteNative LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.5 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)
# Dear ImGui docking and the official raylib integration layer.
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG docking GIT_SHALLOW TRUE)
FetchContent_Declare(rlimgui GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git GIT_TAG main GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui rlimgui)
add_library(coremodel_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${rlimgui_SOURCE_DIR}/rlImGui.cpp)
target_include_directories(coremodel_imgui PUBLIC ${imgui_SOURCE_DIR} ${rlimgui_SOURCE_DIR})
target_link_libraries(coremodel_imgui PUBLIC raylib)
add_executable(N3DLite main.cpp)
target_link_libraries(N3DLite PRIVATE raylib coremodel_imgui)

# Download the open-source Roboto font during configuration; fallback to raylib font offline.
set(ROBOTO_FONT "${CMAKE_CURRENT_BINARY_DIR}/Roboto.ttf")
if(NOT EXISTS "${ROBOTO_FONT}")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/google/fonts/main/ofl/roboto/Roboto%5Bwdth%2Cwght%5D.ttf"
    "${ROBOTO_FONT}"
    STATUS ROBOTO_STATUS TIMEOUT 25 TLS_VERIFY ON)
  list(GET ROBOTO_STATUS 0 ROBOTO_RESULT)
  if(NOT ROBOTO_RESULT EQUAL 0)
    file(REMOVE "${ROBOTO_FONT}")
    message(WARNING "Roboto download unavailable; built-in font will be used.")
  endif()
endif()
if(EXISTS "${ROBOTO_FONT}")
  add_custom_command(TARGET N3DLite POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ROBOTO_FONT}" "$<TARGET_FILE_DIR:N3DLite>/Roboto.ttf")
endif()
