#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (triangles) in;
layout (triangle_strip, max_vertices = 20) out;

layout (push_constant) uniform params_t {
    mat4 mProjView;
    mat4 mModel;
} params;

layout (location = 0) in VS_IN {
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vertexIn[];

layout (location = 0) out VS_OUT {
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vertexOut;

layout (binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

void setOutVertex(vec3 pos, vec3 norm, vec3 tangent, vec2 texCoord) {
    vertexOut.wPos     = pos;
    vertexOut.wNorm    = norm;
    vertexOut.wTangent = tangent;
    vertexOut.texCoord = texCoord;
    gl_Position = params.mProjView * vec4(pos, 1.0f);
    EmitVertex();
}

void main(void) {
    vec3 middleVertexNorm     = normalize(vertexIn[0].wNorm + vertexIn[1].wNorm + vertexIn[2].wNorm);
    vec3 middleVertexPos      = (vertexIn[0].wPos + vertexIn[1].wPos + vertexIn[2].wPos) / 3.f;
    vec3 middleVertexTangent  = normalize(vertexIn[0].wTangent + vertexIn[1].wTangent + vertexIn[2].wTangent);
    vec2 middleVertexTexCoord = (vertexIn[0].texCoord + vertexIn[1].texCoord + vertexIn[2].texCoord) / 3.f;

    float scale = sin(Params.time) * sin(Params.time);
    vec3 w = cross(middleVertexNorm, middleVertexTangent);
    float x1 = cos(8.f * Params.time);
    float x2 = sin(8.f * Params.time);
    vec3 displacement = x1 * middleVertexTangent + x2 * w;

    middleVertexPos += (middleVertexNorm * 0.1f + 0.05f * displacement) * scale;

    setOutVertex(vertexIn[0].wPos, vertexIn[0].wNorm, vertexIn[0].wTangent, vertexIn[0].texCoord);
    setOutVertex(vertexIn[2].wPos, vertexIn[2].wNorm, vertexIn[2].wTangent, vertexIn[2].texCoord);
    setOutVertex(middleVertexPos, middleVertexNorm, middleVertexTangent, middleVertexTexCoord);
    setOutVertex(vertexIn[1].wPos, vertexIn[1].wNorm, vertexIn[1].wTangent, vertexIn[1].texCoord);
    setOutVertex(vertexIn[0].wPos, vertexIn[0].wNorm, vertexIn[0].wTangent, vertexIn[0].texCoord);

    EndPrimitive();
}