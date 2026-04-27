#version 460
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_UV;

out vec2 o_TexCoord;
out vec4 o_VertexColor;
out vec3 o_FragPos;
out vec3 o_Normal;

uniform mat4 u_Projection;
uniform mat4 u_ModelView;
uniform vec4 u_AmbientColour;
uniform vec4 u_ShadowColour;
uniform vec4 u_UVOffset;
uniform float u_IsWater;

void main() {
    vec4 vertexViewSpace = u_ModelView * vec4(a_Position, 1.0);
    gl_Position = u_Projection * vertexViewSpace;

    // Mix ambient and shadow colour using vertex colour as blend factor (matches DX8 shader)
    o_VertexColor = u_AmbientColour * vec4(a_Color, 1.0) + u_ShadowColour * (vec4(1.0) - vec4(a_Color, 1.0));

    o_TexCoord = a_UV + u_UVOffset.xy;
    o_FragPos  = vertexViewSpace.xyz;
    o_Normal   = mat3(transpose(inverse(u_ModelView))) * a_Normal;
}
