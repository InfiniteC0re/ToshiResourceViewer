#version 400
out vec4 FragColor;

in vec2 o_TexCoord;
in vec3 o_Normal;
in vec3 o_Position;

uniform sampler2D tex0;
// uniform float u_Shininess;

void main() {
    vec4 texColor = texture(tex0, o_TexCoord);

    // FragColor.rgb = texColor.rgb * max(0.2f, dot(o_Normal, vec3(1.0f, 0.0f, 0.0f))) + texColor.rgb * );
    float ambient = 0.5f;
    vec3 lightDir = normalize(vec3(1.0f, 0.2f, 0.6f));
    
    FragColor.rgb = texColor.rgb * (ambient + min( 0.3f, max(0.0f, dot(o_Normal, lightDir))) + min( 0.2f, max(0.0f, dot(o_Normal, vec3(0.0f, -1.0f, 0.0f)))));
    FragColor.a = 1.0f;

    // // TODO: Make alpha reference an uniform value to control it from code
    // if (texColor.a < 0.00392156862745098f) discard;

    // gPosition = vec4(o_Position, 1.0f);
    // gNormal = vec4(o_Normal, 1.0f);
    // gColor = texColor;
    // gInfo.r = 4.0f;
    // gInfo.g = u_Shininess;
}
