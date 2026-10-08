cmake_minimum_required(VERSION 3.20)
project(CoreModelNative LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.5 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)
add_executable(CoreModel main.cpp)
target_link_libraries(CoreModel PRIVATE raylib)

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
  add_custom_command(TARGET CoreModel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ROBOTO_FONT}" "$<TARGET_FILE_DIR:CoreModel>/Roboto.ttf")
endif()
