#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (location = 0) out vec4 out_fragColor;

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
} vsOut;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D shadowMap;
layout (binding = 2) uniform sampler2D positionMap;
layout (binding = 3) uniform sampler2D normalMap;
layout (binding = 4) uniform sampler2D albedoMap;
layout (binding = 5) uniform sampler2D ssaoMap;
layout (binding = 6) uniform sampler2D lightViewWposition;
layout (binding = 7) uniform sampler2D lightViewWnormal;
layout (binding = 8) uniform sampler2D flux;
layout (binding = 9) buffer rsmSamplesBuf
{
  vec2 rsmSamples[];
};

#define PI 3.1415926538
#define rsmRmax 0.18f
#define rsmSamplesNum 400
#define indirectIlluminationIntensity 20.f

vec4 indirectIllumination(vec3 wPos, vec3 wNorm, vec2 texCoord)
{
  vec4 illumination = vec4(0.0f);
  for (int i = 0; i < rsmSamplesNum; ++i)
  {
    const vec2 rnd = rsmSamples[i];
    const vec2 coords = texCoord + rsmRmax * rnd.x * vec2(sin(2 * PI * rnd.y), cos(2 * PI * rnd.y));
    const vec3 wPosSample = texture(lightViewWposition, coords).xyz;
    const vec3 wNormSample = texture(lightViewWnormal, coords).xyz;
    const vec4 fluxSample = texture(flux, coords);
    illumination += fluxSample * rnd.x  * rnd.x 
      * max(0, dot(wNormSample, wPos - wPosSample))
      * max(0, dot(wNorm, wPosSample - wPos))
      / pow(length(wPos - wPosSample), 4);
  }
  return clamp(illumination / rsmSamplesNum * indirectIlluminationIntensity, 0.0f, 1.0f);
}

vec3 T(float s) {
    return vec3(0.233, 0.455, 0.649) * exp(-s*s/0.0064) + \
           vec3(0.1, 0.336, 0.344) * exp(-s*s/0.0484) + \
           vec3(0.118, 0.198, 0.0) * exp(-s*s/0.187) + \
           vec3(0.113, 0.007, 0.007) * exp(-s*s/0.567) + \
           vec3(0.358, 0.004, 0.0) * exp(-s*s/1.99) + \
           vec3(0.078, 0.0, 0.0) * exp(-s*s/7.41);
}


void main()
{
  const vec3 wPos = (Params.viewInverse * vec4(texture(positionMap, vsOut.texCoord).xyz, 1.0)).xyz;
  const vec4 posLightClipSpace = Params.lightMatrix*vec4(wPos, 1.0f);
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz/posLightClipSpace.w;    // for orto matrix, we don't need perspective division, you can remove it if you want; this is general case;
  vec2 shadowTexCoord    = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);  // just shift coords from [-1,1] to [0,1]
  const float sampledShadowDepth = texture(shadowMap, shadowTexCoord).x;

  const bool  outOfView = (shadowTexCoord.x < 0.0001f || shadowTexCoord.x > 0.9999f || shadowTexCoord.y < 0.0091f || shadowTexCoord.y > 0.9999f);
  const float shadow    = ((posLightSpaceNDC.z < sampledShadowDepth + 0.001f) || outOfView) ? 1.0f : 0.0f;

  const vec3 normal = (Params.viewInverse * texture(normalMap, vsOut.texCoord)).xyz;
  vec4 ambient = vec4(0.1f);
  if (Params.indirectIlluminationEnabled)
    ambient += indirectIllumination(wPos, normal, shadowTexCoord);

  const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
  const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);
  const vec3 subsurfaceColor = vec3(0.4f, 0.2f, 0.1f);

  vec4 lightColor2 = vec4(1.0f, 1.0f, 1.0f, 1.0f) * Params.lightIntensity;

  vec4 albedo     = texture(albedoMap, vsOut.texCoord);
  if (albedo.xyz == vec3(0.f))
  {
    out_fragColor = vec4(0.f);
  }
  else
  {
    float occlusion = Params.ssaoEnabled ? texture(ssaoMap, vsOut.texCoord).r : 1.f;
    vec3 lightDir = normalize(Params.lightPos - wPos);
    vec4 lightColor = max(dot(normal, lightDir), 0.0f) * lightColor2;
    vec4 directLight = vec4(0.f);
    if (Params.directIlluminationEnabled)
      directLight += lightColor * shadow;
    out_fragColor = (directLight + ambient * occlusion) * albedo;
    if (Params.sssEnabled)
    {
      bool condition = true;
      if (Params.sssForTeapot)
        condition = albedo.xyz == vec3(1.f);
      if (condition)
      {
        vec4 unscaledViewPos = Params.projInverse * vec4(shadowTexCoord * 2 - 1, sampledShadowDepth, 1.0f);
        float linearShadowDepth = (unscaledViewPos.xyz/unscaledViewPos.w).z;
        float linearLightDepth = (Params.lightView * vec4(wPos, 1.0f)).z;
        float s = abs(linearShadowDepth - linearLightDepth) / 10.f;
        float E = max(0.3 + dot(-normal, lightDir), 0.0);
        vec3 transmittance = T(s) * subsurfaceColor * E;
        out_fragColor += vec4(transmittance, 1.0f);
      }
    }
  }
}
