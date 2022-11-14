#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 Albedo;
layout(location = 1) out vec4 NormalMap;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (location = 0) in VS_OUT
{
  vec3 wNorm;
  vec2 texCoord;
  vec3 color;
} surf;

void main()
{
    Albedo = vec4(surf.color, 1.0);
    NormalMap = vec4(surf.wNorm, 1.0);
}