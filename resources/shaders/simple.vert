#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"
#include "common.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint type;
} params;


layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;

} vOut;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

mat4 rotateTeapot(float time) {
    float s = sin(time);
    float c = cos(time);

    return mat4(
        c,   0.f, s,   0.f,
        0.f, 1.f, 0.f, 0.f, 
        -s,  0.f, c,   0.f,
        0.f, 0.f, 0.f, 1.f
    );
}

mat4 jumpingCylinder(float time) {
    return mat4(
        1.f, 0.f,                                      0.f, 0.f,
        0.f, 0.8f + 0.2f * sin(6.f * (time - 0.785f)), 0.f, 0.f, 
        0.f, 0.f,                                      1.f, 0.f,
        0.f, -0.3f + sqrt(abs(sin(3.f * time))),       0.f, 1.f
    );
}

mat4 jumpingL(float time) {
    return mat4(
        1.f, 0.f,                                     0.f, 0.f,
        0.f, 0.8f + 0.2f * cos(6.f * (time - 1.57f)), 0.f, 0.f, 
        0.f, 0.f,                                     1.f, 0.f,
        0.f, -0.3f + sqrt(abs(cos(3.f * time))),      0.f, 1.f
    );
}

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    vOut.wPos     = (params.mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(params.mModel))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    mat4 transformationMatrix;

    switch (params.type) {
        case 1: // Teapot
            transformationMatrix = rotateTeapot(Params.time);
            break;

        case 3: // Cylinder
            transformationMatrix = jumpingCylinder(Params.time);
            break;

        case 4: // L-shaped figure
            transformationMatrix = jumpingL(Params.time);
            break;

        default:
            transformationMatrix = mat4(1.f);
            break;
    }

    vOut.wPos     = (transformationMatrix * vec4(vOut.wPos, 1.0)).xyz;
    vOut.wNorm    = (transformationMatrix * vec4(vOut.wNorm, 1.0)).xyz;
    vOut.wTangent = (transformationMatrix * vec4(vOut.wTangent, 1.0)).xyz;

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
