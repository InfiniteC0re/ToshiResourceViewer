#version 460
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_UV;

out vec2 textureCoord;
out vec3 vertexColor;
out vec3 fragPos;
out vec3 normal;

uniform mat4 u_Projection;
uniform mat4 u_ViewWorld;
uniform mat4 u_ModelView;

void main() {
    vec4 vertexViewSpace = u_ModelView * vec4(a_Position, 1.0);

    mat4 modelMatrix = u_ViewWorld * u_ModelView;
    gl_Position = u_Projection * vertexViewSpace;
    
    textureCoord = a_UV;
    vertexColor = a_Color;
    fragPos = vertexViewSpace.xyz;
    normal = mat3(transpose(inverse(modelMatrix))) * a_Normal;
}