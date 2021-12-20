// gles3min.cpp - minimal testbed for gles3 raspberry pi imgui + user defined uniforms, shader
// - https://github.com/blitz-research/opengldev
// uniforms tutorial
// - https://www.lighthouse3d.com/tutorials/glsl-tutorial/uniform-blocks/
//{{{  includes
#include <cstdio>
#include <cstdlib>

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
//}}}

namespace {
  const float pi2 = 6.28318530718f;
  //{{{
  // Needs to be *exactly* the same as version in shader
  struct sUniforms {
    int mNumInstances = 60;
    float mRotation = 0.0f;
    float mRadius = 0.9f;
    float mSize = 0.025f;
    };
  //}}}
  //{{{
  const char* vertShaderSource = R"(
    #version 300 es

    precision highp float;
    layout (std140) uniform sUniforms {
      int mNumInstances;
      float mRotation;
      float mRadius;
      float mSize;
      };

    const vec2[3] vertices = vec2[] (
      vec2 (-1.0, -1.0),
      vec2 ( 0.0,  2.0),
      vec2 ( 1.0, -1.0));

    const float pi2 = 6.28318530718;

    out float color;

    void main() {
      float r = float(gl_InstanceID) * pi2 / float(mNumInstances) + mRotation;
      float c = cos (r), s = sin(r);
      mat2 m = mat2 (c, s, -s, c);
      vec2 v = vec2 (c, s) * mRadius + m * vertices[gl_VertexID] * mSize;
      gl_Position = vec4 (v.x, v.y, 0.0, 1.0);
      color = sin ((r - mRotation) / 2.0);
      }
    )";
  //}}}
  //{{{
  const char* fragShaderSource = R"(

    #version 300 es

    precision highp float;

    in float color;
    out vec4 fragColor;

    void main() {
      fragColor = vec4 (1.0, color, 1.0 - color,1.0);
      }
    )";
  //}}}
  //{{{
  GLuint compileShader (GLenum type, const char* source) {

    GLuint shader = glCreateShader (type);
    glShaderSource (shader, 1, &source, nullptr);
    glCompileShader (shader);
    GLint status = 0;
    glGetShaderiv (shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE)
      return shader;

    // failed
    char log[256];
    glGetShaderInfoLog (shader, sizeof(log), nullptr, log);
    printf ("Compile shader failed:%s\n", log);
    exit (1);
    }
  //}}}
  }

int main() {

  // initialize GLFW and create window/GL context
  glfwInit();
  glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint (GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
  GLFWwindow* window = glfwCreateWindow (1280, 720, "raspberry pi 4 - imgui + shader demo", NULL, NULL);
  glfwMakeContextCurrent (window);

  #ifdef BUILD_FREE
    glfwSwapInterval (0); // disable vsync
  #else
    glfwSwapInterval (1); // Enable vsync
  #endif

  // initialize ImGui
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL (window, true);
  ImGui_ImplOpenGL3_Init();

  // create shaders
  GLuint vertShader = compileShader (GL_VERTEX_SHADER, vertShaderSource);
  GLuint fragShader = compileShader (GL_FRAGMENT_SHADER, fragShaderSource);
  GLuint program = glCreateProgram();
  glAttachShader (program, vertShader);
  glAttachShader (program, fragShader);
  glLinkProgram (program);
  glUseProgram (program);

  // create uniforms buffer
  GLuint uniformsBuf = 0;
  glGenBuffers (1, &uniformsBuf);
  glBindBufferBase (GL_UNIFORM_BUFFER, 0, uniformsBuf);
  GLuint uniformsBlock = glGetUniformBlockIndex (program, "Uniforms");
  glUniformBlockBinding (program, uniformsBlock, 0);

  // set initial state
  glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
  sUniforms uniforms;
  bool demo = true;

  // main UI loop
  while (!glfwWindowShouldClose (window)) {
    glfwPollEvents();

    // start new frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (demo)
      ImGui::ShowDemoWindow (&demo);

    // update uniforms
    ImGui::SliderInt ("Instances", &uniforms.mNumInstances, 6, 360);
    ImGui::SliderFloat ("Rotation", &uniforms.mRotation, 0.0f, pi2);
    ImGui::SliderFloat ("Radius", &uniforms.mRadius, 0.0f, 1.0f);
    ImGui::SliderFloat ("Size", &uniforms.mSize, 0.0f, 1.0f);

    ImGui::Render();

    // clear draw buffer
    glClear (GL_COLOR_BUFFER_BIT);

    // upload uniform data to shader
    glBufferData (GL_UNIFORM_BUFFER, sizeof(uniforms), &uniforms, GL_STREAM_DRAW);

    // render our stuff
    glDrawArraysInstanced (GL_TRIANGLES, 0, 3, uniforms.mNumInstances);

    // render imgui stuff!
    ImGui_ImplOpenGL3_RenderDrawData (ImGui::GetDrawData());

    // flip
    glfwSwapBuffers (window);
    }
  }
