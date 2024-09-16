#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inPosition;
layout(push_constant) uniform Push {
    float time;
};

void main() {
   outColor = vec4(fract(sin((inPosition.x + inPosition.y - 1.0) * 50) * 0.5 + 1.0), fract(cos((1.0 - inPosition.x + inPosition.y) * 50)* 0.5 + 1.0 ), fract(sin(time * 3.14159/30 ) * 0.5  + 1.0), 1.0);  
}