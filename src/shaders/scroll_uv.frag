#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);

    // Use luminance to determine alpha for blending
    // Dark areas become transparent
    // float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));

    // Option 1: Linear fade based on brightness
    // texColor.a = luminance;

    // Option 2: Smoother fade with a curve
    // texColor.a = smoothstep(0.0, 0.3, -luminance);

    // Option 3: If your texture already has alpha, just use it
    // texColor.a = texColor.a;

    outColor = texColor;
}
