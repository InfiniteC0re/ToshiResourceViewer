#version 460
layout(location = 0) out vec4 gColor;

in vec2 o_TexCoord;
in vec4 o_VertexColor;
in vec3 o_FragPos;
in vec3 o_Normal;

uniform sampler2D tex0;
uniform float u_AlphaRef;

void main() {
    vec4 texColor = texture(tex0, o_TexCoord);
    if (texColor.a * o_VertexColor.a < u_AlphaRef / 255.0) discard;

    gColor = texColor * o_VertexColor;
}
