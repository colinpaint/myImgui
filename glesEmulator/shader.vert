#version 100

attribute vec4 av4position;
attribute vec3 av3colour;

uniform mat4 mvp;

varying vec3 vv3colour;

void main() {
  vv3colour = av3colour;
  gl_Position = mvp * av4position;
  }
