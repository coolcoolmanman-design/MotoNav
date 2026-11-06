cmake_minimum_required(VERSION 3.16)
project(FirmwareHost LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HOST_HAS_SDL "Enable SDL2 window present" ON)

add_compile_definitions(HOST_SIM=1)

# Prefer stubs over Arduino/ESP-IDF / real MotoNavIpcStackGuard when names collide.
set(FH_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(FH_STUBS ${FH_ROOT}/stubs)
set(FH_FW_MOTONAV ${FH_ROOT}/../../firmware/boards/175/MotoNav)
set(FH_IPC_LIB ${FH_ROOT}/../../firmware/libraries/MotoNavIpcStackGuard/src)

add_executable(FirmwareHost
  src/host_main.cpp
  src/host_present.cpp
  src/host_tcp.cpp
  src/host_touch.cpp
  src/firmware_bundle.cpp
)

target_include_directories(FirmwareHost PRIVATE
  ${FH_STUBS}
  ${FH_ROOT}/src
  ${FH_FW_MOTONAV}
  ${FH_IPC_LIB}
)

# stubs/ first so MotoNavIpcStackGuard.h / Arduino headers resolve to host shims.
target_include_directories(FirmwareHost BEFORE PRIVATE ${FH_STUBS})

find_package(Threads REQUIRED)
target_link_libraries(FirmwareHost PRIVATE Threads::Threads)

if(HOST_HAS_SDL)
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    message(STATUS "FirmwareHost: SDL2 found — window present enabled")
    target_compile_definitions(FirmwareHost PRIVATE HOST_HAS_SDL=1)
    if(TARGET SDL2::SDL2)
      target_link_libraries(FirmwareHost PRIVATE SDL2::SDL2)
    else()
      target_include_directories(FirmwareHost PRIVATE ${SDL2_INCLUDE_DIRS})
      target_link_libraries(FirmwareHost PRIVATE ${SDL2_LIBRARIES})
    endif()
  else()
    # pkg-config fallback (Homebrew)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(SDL2 QUIET sdl2)
    endif()
    if(SDL2_FOUND)
      message(STATUS "FirmwareHost: SDL2 via pkg-config")
      target_compile_definitions(FirmwareHost PRIVATE HOST_HAS_SDL=1)
      target_include_directories(FirmwareHost PRIVATE ${SDL2_INCLUDE_DIRS})
      target_link_libraries(FirmwareHost PRIVATE ${SDL2_LIBRARIES})
    else()
      message(STATUS "FirmwareHost: SDL2 not found — headless PPM present")
      target_compile_definitions(FirmwareHost PRIVATE HOST_HAS_SDL=0)
    endif()
  endif()
else()
  message(STATUS "FirmwareHost: HOST_HAS_SDL=OFF — headless PPM present")
  target_compile_definitions(FirmwareHost PRIVATE HOST_HAS_SDL=0)
endif()

if(APPLE)
  # aligned_alloc / posix_memalign already available; silence OpenGL deprecation if SDL pulls it.
  target_compile_definitions(FirmwareHost PRIVATE GL_SILENCE_DEPRECATION=1)
endif()

install(TARGETS FirmwareHost RUNTIME DESTINATION bin)
