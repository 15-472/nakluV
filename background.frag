#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inPosition;
layout(push_constant) uniform Push {
    float time;
};

void main() {
   outColor = vec4(0., 0., 0., 1.0);  
}