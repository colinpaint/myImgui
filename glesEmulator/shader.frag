#version 100

precision lowp float;

varying vec3 vv3colour;

void main() {
  gl_FragColor = vec4(vv3colour, 1.0);
  }
