#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inPosition;
layout(push_constant) uniform Push {
    float time;
};

void main() {
    outColor = vec4(fract(sin(inPosition.x * time * inPosition.y * 5)), fract(sin((inPosition.x + time + inPosition.y) )), fract(sin(time/10)), 1.0);
    
}