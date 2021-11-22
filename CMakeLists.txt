cmake_minimum_required (VERSION 3.16)
#
option (BUILD_AVX "Build avx" ON)
option (BUILD_OPTIMISED "Build optimised" ON)
#
set (OPENGL_VERSION "OPENGL_45")
set (CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "limited configs" FORCE)

# utils lib
project (utils)
  file (GLOB HEADER_FILES utils/*.h)
  file (GLOB SOURCE_FILES utils/*.cpp utils/*.c)
  add_library (${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})

  if (MSVC)
    target_compile_options (${PROJECT_NAME} PUBLIC /W4 /fp:fast /std:c++17)
    if (BUILD_AVX)
      target_compile_options (${PROJECT_NAME} PUBLIC /arch:AVX)
    endif()
  else()
    target_compile_definitions (${PROJECT_NAME} PUBLIC _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64)
    target_compile_options (${PROJECT_NAME} PUBLIC -Wall -Wextra
                                                   -Wno-missing-field-initializers
                                                   -Wno-format-security -Wno-format-overflow
                                                   -pedantic
                                                   -march=native -flax-vector-conversions -ftree-vectorize
                                                   -std=c++17 )
    if (BUILD_OPTIMISED)
      target_compile_options (${PROJECT_NAME} PUBLIC -Ofast)
    else()
      target_compile_options (${PROJECT_NAME} PUBLIC -g -O0)
    endif()
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
if (OPENGL_VERSION STREQUAL "OPENGL_21")
  add_subdirectory (glad2.1+fbo+vao)
elseif (OPENGL_VERSION STREQUAL "OPENGL_45")
  add_subdirectory (glad4.5core)
endif()

# imgui glfw lib
project (imgui)
  file (GLOB HEADER_FILES imgui/*.h)
  file (GLOB SOURCE_FILES imgui/*.cpp)
  add_library (${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})

  target_include_directories (${PROJECT_NAME} PUBLIC . glfw/include)
  target_link_libraries (${PROJECT_NAME} PUBLIC utils)
  if (MSVC)
    target_link_libraries (${PROJECT_NAME} PRIVATE opengl32 glfw glad d3d11 d3dcompiler)
  else()
    target_link_libraries (${PROJECT_NAME} PRIVATE GL glfw glad)
  endif()

# implot lib
project (implot)
  file (GLOB HEADER_FILES implot/*.h)
  file (GLOB SOURCE_FILES implot/*.cpp)
  add_library (${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})
  target_include_directories (${PROJECT_NAME} PUBLIC implot imgui)

# opengl2 app
project (opengl2)
  file (GLOB HEADER_FILES backends/imgui_impl_glfw.h imgui/backends/imgui_impl_opengl2.h)
  file (GLOB SOURCE_FILES opengl2.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl2.cpp)
  add_executable (${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

  target_include_directories (${PROJECT_NAME} PRIVATE imgui imgui/backends)
  target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui)

# opengl3 app
project (opengl3)
  file (GLOB HEADER_FILES backends/imgui_impl_glfw.h backends/imgui_impl_opengl3.h)
  file (GLOB SOURCE_FILES opengl3.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp)
  add_executable (${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

  target_include_directories (${PROJECT_NAME} PRIVATE imgui imgui/backends)
  target_link_libraries (${PROJECT_NAME} PRIVATE implot imgui)
