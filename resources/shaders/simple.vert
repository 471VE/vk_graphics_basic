#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"
#include "brownian_noise.glsl"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint quadResolution;
    float minHeight;
    float maxHeight;
} params;

layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;

} vOut;

float getHeight(vec2 position, float minH, float maxH) {
    return fbm(position * 2) * (maxH - minH) - 6.05f + minH;
}

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec2 meshResolution = vec2(params.quadResolution, params.quadResolution);

    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    vOut.wPos     = (params.mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(params.mModel))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    float height = getHeight(vOut.texCoord, params.minHeight, params.maxHeight);
    vOut.wPos.y = height;

    // Calculate normals
    vec2 delta  = vec2(1 / meshResolution.x, 1 / meshResolution.y);
    vec2 dx = vec2(delta.x , 0.0);
    vec2 dy = vec2(0.0 , delta.y);
    float right = getHeight(vOut.texCoord + dx, params.minHeight, params.maxHeight);
    float left = getHeight(vOut.texCoord - dx, params.minHeight, params.maxHeight);
    float top = getHeight(vOut.texCoord + dy, params.minHeight, params.maxHeight);
    float bottom = getHeight(vOut.texCoord - dy, params.minHeight, params.maxHeight);
    vec3 norm = -vec3((right - left) / (2.0f * delta.x),
                      -1.0f,
                      (top - bottom) / (2.0f * delta.y));
    norm = normalize(norm);
    vOut.wNorm = norm;

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
