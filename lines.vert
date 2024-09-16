#version 450

layout(set=0, binding=0, std140) uniform Camera{
    mat4 CLIP_FROM_WORLD;
};

layout(location=0) in vec3 Position;
layout(location=1) in vec4 Color;

layout(location=0) out vec4 color;

void main() {
    gl_Position = CLIP_FROM_WORLD * vec4(Position, 1.0);
    color = Color;
}