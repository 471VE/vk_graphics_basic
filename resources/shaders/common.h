#ifndef VK_GRAPHICS_BASIC_COMMON_H
#define VK_GRAPHICS_BASIC_COMMON_H

// GLSL-C++ datatype compatibility layer

#ifdef __cplusplus

#include <LiteMath.h>

// NOTE: This is technically completely wrong,
// as GLSL words are guaranteed to be 32-bit,
// while C++ unsigned int can be 16-bit.
// Touching LiteMath is forbidden though, so yeah.
using shader_uint  = LiteMath::uint;
using shader_uvec2 = LiteMath::uint2;
using shader_uvec3 = LiteMath::uint3;

using shader_float = float;
using shader_vec2  = LiteMath::float2;
using shader_vec3  = LiteMath::float3;
using shader_vec4  = LiteMath::float4;
using shader_mat4  = LiteMath::float4x4;

// The funny thing is, on a GPU, you might as well consider
// a single byte to be 32 bits, because nothing can be smaller
// than 32 bits, so a bool has to be 32 bits as well.
using shader_bool  = LiteMath::uint;

#else

#define shader_uint  uint
#define shader_uvec2 uvec2

#define shader_float float
#define shader_vec2  vec2
#define shader_vec3  vec3
#define shader_vec4  vec4
#define shader_mat4  mat4

#define shader_bool  bool

vec3 getColor(uint colorNo)
{
  switch (colorNo)
  {
    case 0: // Room walls
      return vec3(1.f, 1.f, 0.2f);;            
    case 1: // Teapot
      return vec3(1.f);            
    case 2: // Box
      return vec3(0.3f, 1.f, 0.3f);            
    case 3: // Cylinder
      return vec3(1.f, 0.2f, 0.2f);            
    case 4: // L-shaped figure
      return vec3(0.1f, 0.1f, 1.f);            
    case 5: // Sphere
      return vec3(0.f, 1.f, 1.f);       
  }
}

#endif


struct UniformParams
{
  shader_mat4  lightMatrix;
  shader_mat4  view;
  shader_mat4  viewInverse;
  shader_mat4  proj;
  shader_mat4  projInverse;
  shader_mat4  lightView;
  shader_vec3  lightPos;
  shader_float time;
  shader_vec3  baseColor;
  shader_bool  ssaoEnabled;
  shader_float lightIntensity;
  shader_uint  tonemappingMode;
  shader_bool directIlluminationEnabled;
  shader_bool  indirectIlluminationEnabled;
  shader_bool sssEnabled;
  shader_bool sssForTeapot;
};

#endif // VK_GRAPHICS_BASIC_COMMON_H
