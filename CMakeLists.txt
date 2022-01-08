#cmake .. -DBUILD_FREE=ON -DBUILD_DOCKING=ON
cmake_minimum_required (VERSION 3.18)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
#
# valid options - GL_2 GL_3 VULKAN
set (BUILD_GRAPHICS "GL_3" CACHE STRING "build graphics option")
set (BUILD_VSYNC "VSYNC" CACHE STRING "Build imgui vsync option")
set (BUILD_DOCKING "DOCKING" CACHE STRING "Build imgui docking option")
#
if (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
  option (BUILD_DX11 "Build dx11" ON)
  option (BUILD_DX12 "Build dx12" OFF)
  set (COMPILE_MSVC "/arch:AVX" CACHE STRING "msvc compile options")
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
  option (BUILD_VALIDATION "Build vulkan validation layers" OFF)
  set (COMPILE_LINUX "-Ofast" CACHE STRING "linux compile options")
elseif()
  message (FATAL_ERROR, "unknown CMAKE_HOST_SYSTEM_NAME = ${CMAKE_HOST_SYSTEM_NAME}")
endif()
#
set (CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "limited configs" FORCE)
#set (CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)

# utils lib
project (utils C CXX)
  file (GLOB HEADER_FILES utils/*.h)
  file (GLOB SOURCE_FILES utils/*.cpp utils/*.c)
  add_library (${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})

  if (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
    message (STATUS "buiding ${CMAKE_HOST_SYSTEM_NAME}")
    target_compile_options (${PROJECT_NAME} PUBLIC /W4 /fp:fast ${COMPILE_MSVC})
  elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
    message (STATUS "buiding ${CMAKE_HOST_SYSTEM_NAME}")
    target_compile_definitions (${PROJECT_NAME} PUBLIC _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64)
    target_compile_options (${PROJECT_NAME} PUBLIC -Wall -Wextra
                                                   -Wno-missing-field-initializers
                                                   -Wno-format-security -Wno-format-overflow
                                                   -pedantic
                                                   -fno-exceptions
                                                   -flax-vector-conversions -ftree-vectorize
                                                   -march=native
                                                   ${COMPILE_LINUX})
  endif()

message (STATUS "using ${BUILD_GRAPHICS} graphics")
if (BUILD_VSYNC)
  message (STATUS "using vsync")
endif()
if (BUILD_DOCKING)
  message (STATUS "using docking")
endif()
if (BUILD_GRAPHICS STREQUAL "VULKAN" AND BUILD_VALIDATION)
  message (STATUS "using vulkan validation")
endif()

# glfw
set (GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set (GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set (GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set (ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set (GLFW_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
#set (GLFW_USE_WAYLAND ON CACHE BOOL "" FORCE)
add_subdirectory (glfw)

# glad
if (BUILD_GRAPHICS STREQUAL "GL_2")
  add_subdirectory (glad2.1+fbo+vao)
elseif (BUILD_GRAPHICS STREQUAL "GL_3")
  add_subdirectory (glad4.5core)
endif()

# imgui glfw lib
project (imgui C CXX)
  file (GLOB HEADER_FILES imgui/*.h)
  file (GLOB SOURCE_FILES imgui/*.cpp)
  add_library (${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})

  target_include_directories (${PROJECT_NAME} PUBLIC . glfw/include imgui imgui/backends)
  target_link_libraries (${PROJECT_NAME} PUBLIC utils)

# implot lib
project (implot C CXX)
  file (GLOB HEADER_FILES implot/*.h)
  file (GLOB SOURCE_FILES implot/*.cpp)
  add_library (${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})
  target_include_directories (${PROJECT_NAME} PUBLIC implot imgui)

if (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
  # gles3 emulator minimal app - windows only
  project (gles3emulator C CXX)
    add_executable (${PROJECT_NAME} glesEmulator/main.cpp)
    target_include_directories (${PROJECT_NAME} PRIVATE glesEmulator/include)
    target_link_directories (${PROJECT_NAME} PRIVATE glesEmulator/lib)
    target_link_libraries (${PROJECT_NAME} PRIVATE libGLESv2 libEGL opengl32)
  endif()

if (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows AND BUILD_DX11)
  # dx11 app - optioned to shutup virus checker
  project (dx11 C CXX)
    add_executable (${PROJECT_NAME} dx11.cpp
                                    imgui/backends/imgui_impl_win32.h imgui/backends/imgui_impl_win32.cpp
                                    imgui/backends/imgui_impl_dx11.h imgui/backends/imgui_impl_dx11.cpp)

    target_compile_definitions (${PROJECT_NAME} PRIVATE ${BUILD_VSYNC} ${BUILD_DOCKING})
    target_include_directories (${PROJECT_NAME} PRIVATE implot imgui)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui d3d11 d3dcompiler glad opengl32)
endif()

if (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows AND BUILD_DX12)
  # dx12 app
  project (dx12 C CXX)
    add_executable (${PROJECT_NAME} dx12.cpp
                                    imgui/backends/imgui_impl_win32.h imgui/backends/imgui_impl_win32.cpp
                                    imgui/backends/imgui_impl_dx12.h imgui/backends/imgui_impl_dx12.cpp)

    target_compile_definitions (${PROJECT_NAME} PRIVATE ${BUILD_VSYNC} ${BUILD_DOCKING})
    target_include_directories (${PROJECT_NAME} PRIVATE implot imgui)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui d3d12 d3d11 d3dcompiler dxgi glad opengl32)
endif()

if (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
  # vulkan app
  project (vulkan C CXX)
    add_executable (${PROJECT_NAME} vulkan.cpp
                                    imgui/backends/imgui_impl_glfw.h imgui/backends/imgui_impl_glfw.cpp
                                    imgui/backends/imgui_impl_vulkan.h imgui/backends/imgui_impl_vulkan.cpp)

    find_package (Vulkan REQUIRED)
    target_compile_definitions (${PROJECT_NAME} PRIVATE VK_PROTOTYPES ${BUILD_VSYNC} ${BUILD_DOCKING})
    if (BUILD_VALIDATION)
      target_compile_definitions (${PROJECT_NAME} PRIVATE VALIDATION)
    endif()
    #target_include_directories (${PROJECT_NAME} PRIVATE implot imgui glfw/deps)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui glfw Vulkan::Vulkan)
endif()

# glfwGLES3 app
project (glfwGLES3 C CXX)
  file (GLOB HEADER_FILES imgui/backends/imgui_impl_glfw.h imgui/backends/imgui_impl_opengl3.h)
  file (GLOB SOURCE_FILES glfwGLES3.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp)
  add_executable (${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

  target_include_directories (${PROJECT_NAME} PRIVATE implot imgui)
  target_compile_definitions (${PROJECT_NAME} PRIVATE ${BUILD_VSYNC} ${BUILD_DOCKING}
                                                      GLFW_INCLUDE_ES3=1
                                                      IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<GLFW/glfw3.h>
                                                      IMGUI_IMPL_OPENGL_ES3=1)
  if (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
    # use arm GLES emulator
    target_include_directories (${PROJECT_NAME} PRIVATE glesEmulator/include)
    target_link_directories (${PROJECT_NAME} PRIVATE glesEmulator/lib)
    target_link_libraries (${PROJECT_NAME} PRIVATE imgui implot glfw libGLESv2 libEGL opengl32)
  elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
    target_link_libraries (${PROJECT_NAME} imgui implot glfw GLESv2 glad)
  endif()

# glfwGL2 app
project (glfwGL2 C CXX)
  file (GLOB HEADER_FILES imgui/backends/imgui_impl_glfw.h imgui/backends/imgui_impl_opengl2.h)
  file (GLOB SOURCE_FILES glfwGL2.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl2.cpp)
  add_executable (${PROJECT_NAME} glfwGL2.cpp
                                  imgui/backends/imgui_impl_glfw.h imgui/backends/imgui_impl_glfw.cpp
                                  imgui/backends/imgui_impl_opengl2.h imgui/backends/imgui_impl_opengl2.cpp)

  target_compile_definitions (${PROJECT_NAME} PRIVATE ${BUILD_VSYNC} ${BUILD_DOCKING})
  target_include_directories (${PROJECT_NAME} PRIVATE implot imgui)
  if (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui glfw d3d11 d3dcompiler glad opengl32)
  elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui glfw GL glad)
  endif()

# sdlGL3 app
project (sdlGL3 C CXX)
  add_executable (${PROJECT_NAME} sdlGL3.cpp
                                  imgui/backends/imgui_impl_sdl.h imgui/backends/imgui_impl_sdl.cpp
                                  imgui/backends/imgui_impl_opengl3.h imgui/backends/imgui_impl_opengl3.cpp)

  target_compile_definitions (${PROJECT_NAME} PRIVATE ${BUILD_VSYNC} ${BUILD_DOCKING})
  target_include_directories (${PROJECT_NAME} PRIVATE SDL/include imgui imgui/backends)

  if (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
    # windows uses prebuilt SDL2 includes/.lib/.dll in local SDL directory
    target_link_directories (${PROJECT_NAME} PRIVATE SDL/lib/x64)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui utils SDL2 opengl32)
  elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
    # linux uses installed SDL2
    find_package (PkgConfig REQUIRED)
    pkg_check_modules (SDL2 REQUIRED IMPORTED_TARGET sdl2)
    target_include_directories (${PROJECT_NAME} PRIVATE PkgConfig::SDL2)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui utils PkgConfig::SDL2 GL)
  endif()

# glfwGl3 app
project (glfwGL3 C CXX)
  add_executable (${PROJECT_NAME} glfwGL3.cpp
                                  imgui/backends/imgui_impl_glfw.h imgui/backends/imgui_impl_glfw.cpp
                                  imgui/backends/imgui_impl_opengl3.h imgui/backends/imgui_impl_opengl3.cpp)

  target_compile_definitions (${PROJECT_NAME} PRIVATE ${BUILD_VSYNC} ${BUILD_DOCKING})

  if (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui glfw glad opengl32)
  elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui glfw GL glad)
  endif()
