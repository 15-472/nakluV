#version 450 //GLSL version 4.5

layout(location = 0) out vec2 outPosition;

void main() {
    vec2 POSITION = vec2(2 * (gl_VertexIndex & 2) - 1, 
        4 * (gl_VertexIndex & 1) - 1);
    gl_Position = vec4(POSITION, 0.0, 1.0);
    outPosition = POSITION * 0.5 + 0.5; //make the screen [0,1]x[0,1]
}