#cmake .. -DBUILD_FREE=ON -DBUILD_DOCKING=ON
cmake_minimum_required (VERSION 3.18)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
#
# valid options - GL_2 GL_3
set (BUILD_GRAPHICS "GL_3" CACHE STRING "build graphics option")
set (BUILD_DOCKING "DOCKING" CACHE STRING "Build imgui docking option")
set (BUILD_VSYNC "VSYNC" CACHE STRING "Build imgui vsync option")
#
option (BUILD_DX12 "Build dx12" ON)
#
set (COMPILE_MSVC "/arch:AVX" CACHE STRING "msvc compile options")
set (COMPILE_LINUX "-Ofast" CACHE STRING "linux compile options")
#
set (CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "limited configs" FORCE)

# utils lib
project (utils)
  file (GLOB HEADER_FILES utils/*.h)
  file (GLOB SOURCE_FILES utils/*.cpp utils/*.c)
  add_library (${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})

  if (MSVC)
    target_compile_options (${PROJECT_NAME} PUBLIC /W4 /fp:fast ${COMPILE_MSVC})
  else()
    target_compile_definitions (${PROJECT_NAME} PUBLIC _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64)
    target_compile_options (${PROJECT_NAME} PUBLIC -Wall -Wextra
                                                   -Wno-missing-field-initializers
                                                   -Wno-format-security -Wno-format-overflow
                                                   -pedantic
                                                   -march=native -flax-vector-conversions -ftree-vectorize
                                                   ${COMPILE_LINUX})
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
project (imgui)
  file (GLOB HEADER_FILES imgui/*.h)
  file (GLOB SOURCE_FILES imgui/*.cpp)
  add_library (${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})

  target_include_directories (${PROJECT_NAME} PUBLIC . glfw/include)
  target_compile_definitions (${PROJECT_NAME} PUBLIC ${BUILD_GRAPHICS} ${BUILD_DOCKING} ${BUILD_VSYNC})
  target_include_directories (${PROJECT_NAME} PUBLIC imgui imgui/backends)
  target_link_libraries (${PROJECT_NAME} PUBLIC utils)

# implot lib
project (implot)
  file (GLOB HEADER_FILES implot/*.h)
  file (GLOB SOURCE_FILES implot/*.cpp)
  add_library (${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})
  target_include_directories (${PROJECT_NAME} PUBLIC implot imgui)

if (MSVC)
  # gles3 emulator minimal app
  project (gles3emulator)
    file (GLOB SOURCE_FILES glesEmulator/main.cpp)
    add_executable (${PROJECT_NAME} ${SOURCE_FILES})

    target_include_directories (${PROJECT_NAME} PRIVATE glesEmulator/include)
    target_link_directories (${PROJECT_NAME} PUBLIC glesEmulator)
    target_link_libraries (${PROJECT_NAME} PRIVATE libGLESv2 libEGL opengl32)

  # dx11 app
  project (dx11)
    file (GLOB HEADER_FILES backends/imgui_impl_win32.h imgui/backends/imgui_impl_dx11.h)
    file (GLOB SOURCE_FILES dx11.cpp imgui/backends/imgui_impl_win32.cpp imgui/backends/imgui_impl_dx11.cpp)
    add_executable (${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

    target_include_directories (${PROJECT_NAME} PUBLIC implot imgui)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui d3d11 d3dcompiler glad opengl32)

  # dx12 app
  if (BUILD_DX12)
    project (dx12)
      file (GLOB HEADER_FILES backends/imgui_impl_win32.h imgui/backends/imgui_impl_dx12.h)
      file (GLOB SOURCE_FILES dx12.cpp imgui/backends/imgui_impl_win32.cpp imgui/backends/imgui_impl_dx12.cpp)
      add_executable (${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

      target_include_directories (${PROJECT_NAME} PUBLIC implot imgui)
      target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui d3d12 d3d11 d3dcompiler dxgi glad opengl32)
  endif()
endif()

# gles3 minimal app
project (gles3)
  file (GLOB HEADER_FILES backends/imgui_impl_glfw.h imgui/backends/imgui_impl_opengl3.h)
  file (GLOB SOURCE_FILES gles3.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp)
  add_executable (${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

  target_include_directories (${PROJECT_NAME} PUBLIC implot imgui)
  target_compile_definitions (${PROJECT_NAME} PRIVATE GLFW_INCLUDE_ES3=1
                                                      IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<GLFW/glfw3.h>
                                                      IMGUI_IMPL_OPENGL_ES3=1)
  if (MSVC)
    # use arm GLES emulator
    target_include_directories (${PROJECT_NAME} PRIVATE glesEmulator/include)
    target_link_directories (${PROJECT_NAME} PUBLIC glesEmulator)
    target_link_libraries (${PROJECT_NAME} PRIVATE imgui implot glfw libGLESv2 libEGL glad opengl32)
  else()
    target_link_libraries (${PROJECT_NAME} imgui implot glfw GLESv2 glad)
  endif()



# gl2 app
project (gl2)
  file (GLOB HEADER_FILES backends/imgui_impl_glfw.h imgui/backends/imgui_impl_opengl2.h)
  file (GLOB SOURCE_FILES gl2.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl2.cpp)
  add_executable (${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

  target_include_directories (${PROJECT_NAME} PUBLIC implot imgui)
  if (MSVC)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui glfw d3d11 d3dcompiler glad opengl32)
  else()
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui glfw GL glad)
  endif()

# gl3 app
project (gl3)
  file (GLOB HEADER_FILES backends/imgui_impl_glfw.h backends/imgui_impl_opengl3.h)
  file (GLOB SOURCE_FILES gl3.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp)
  add_executable (${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

  if (MSVC)
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui glfw d3d11 d3dcompiler glad opengl32)
  else()
    target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui glfw GL glad)
  endif()
