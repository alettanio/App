#version 450
layout(location = 0) in vec2 inPos;   // współrzędne w pikselach
layout(location = 1) in vec4 inColor;
layout(push_constant) uniform Push { vec2 screen; } pc;
layout(location = 0) out vec4 outColor;
void main() {
    vec2 ndc = vec2((inPos.x / pc.screen.x) * 2.0 - 1.0, (inPos.y / pc.screen.y) * 2.0 - 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    outColor = inColor;
}
