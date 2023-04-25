#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 color;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

vec3 reinhard(vec3 color)
{
  return color.xyz / (color.xyz + vec3(1.0));
}

float luminance(vec3 v)
{
  return dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
}

vec3 reinhard_jodie(vec3 v)
{
  float l = luminance(v);
  vec3 tv = v / (1.0f + v);
  return mix(v / (1.0f + l), tv, tv);
}

vec3 uncharted2_tonemap_partial(vec3 x)
{
  float A = 0.15f;
  float B = 0.50f;
  float C = 0.10f;
  float D = 0.20f;
  float E = 0.02f;
  float F = 0.30f;
  return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 uncharted2_filmic(vec3 v)
{
  float exposure_bias = 2.0f;
  vec3 curr = uncharted2_tonemap_partial(v * exposure_bias);

  vec3 W = vec3(11.2f);
  vec3 white_scale = vec3(1.0f) / uncharted2_tonemap_partial(W);
  return curr * white_scale;
}

const mat3 aces_input_matrix =
{
  vec3(0.59719f, 0.35458f, 0.04823f),
  vec3(0.07600f, 0.90834f, 0.01566f),
  vec3(0.02840f, 0.13383f, 0.83777f)
};

const mat3 aces_output_matrix =
{
  vec3( 1.60475f, -0.53108f, -0.07367f),
  vec3(-0.10208f,  1.10813f, -0.00605f),
  vec3(-0.00327f, -0.07276f,  1.07602f)
};

vec3 rtt_and_odt_fit(vec3 v)
{
  vec3 a = v * (v + 0.0245786f) - 0.000090537f;
  vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
  return a / b;
}

vec3 aces(vec3 v)
{
  v = aces_input_matrix * v;
  v = rtt_and_odt_fit(v);
  return aces_output_matrix * v;
}

vec3 aces_approx(vec3 v)
{
  v *= 0.6f;
  float a = 2.51f;
  float b = 0.03f;
  float c = 2.43f;
  float d = 0.59f;
  float e = 0.14f;
  return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

void main()
{
  vec3 colorRGB = textureLod(colorTex, surf.texCoord, 0).rgb;

  switch (Params.tonemappingMode) {
    case 0:
      colorRGB = clamp(colorRGB, 0.f, 1.f);
      break;
    case 1:
      colorRGB = reinhard(colorRGB);
      break;
    case 2:
      colorRGB = reinhard_jodie(colorRGB);
      break;
    case 3:
      colorRGB = uncharted2_filmic(colorRGB);
      break;
    case 4:
      colorRGB = aces(colorRGB);
      break;
    case 5:
      colorRGB = aces_approx(colorRGB);
      break;
    }

    color = vec4(colorRGB, 1.f);
}
