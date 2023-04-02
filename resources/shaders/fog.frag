#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"
#include "brownian_noise.glsl"

layout(location = 0) out vec4 color;

layout (location = 0 ) in VS_OUT
{
    vec3 pos;
} surf;

layout(push_constant) uniform params_t
{
    mat4 mPrayOriginjView;
    mat4 mModel;
    uint quadResolution;
    float minHeight;
    float maxHeight;
} params;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout(binding = 1, set = 0) uniform Noise_t
{
  NoiseData noise;
};

vec2 ellipsoidIntersection(vec3 ro, vec3 rd, vec3 sz)
{
    sz *= vec3(1.f + 0.2f*cos(Params.time), 1.f + 0.2f*sin(Params.time), 1.f + 0.2f*cos(Params.time));
    float a = (rd.x*rd.x)/(sz.x*sz.x) + (rd.y*rd.y)/(sz.y*sz.y) + (rd.z*rd.z)/(sz.z*sz.z);
    float b = 2 * (ro.x*rd.x/(sz.x*sz.x) + ro.y*rd.y/(sz.y*sz.y) + ro.z*rd.z/(sz.z*sz.z));
    float c = (ro.x*ro.x)/(sz.x*sz.x) + (ro.y*ro.y)/(sz.y*sz.y) + (ro.z*ro.z)/(sz.z*sz.z) - 1;

    float D = b*b - 4*a*c;
    if (D <= 0)
        return vec2(-1.0, -1.0);
    float t_min = (-b - sqrt(D)) / (2*a);
    float t_max = (-b + sqrt(D)) / (2*a);
    return vec2(t_min, t_max);
}

float getDensity(vec3 pos)
{
  return fbm(vec3(noise.scale.x * pos.x + 5. * sin(0.1 * Params.time),
                  noise.scale.y * pos.y + 0.3 * sin(0.5 * Params.time),
                  noise.scale.z * pos.z + 5. * cos(0.1 * Params.time)));
}

void main()
{
    vec3 rayDirection = normalize(Params.eyePos - surf.pos);
    vec3 rayOrigin = surf.pos - rayDirection;

    vec2 intersection = ellipsoidIntersection(rayOrigin, rayDirection, noise.semiAxes);
    vec3 entry = rayOrigin + intersection.x * rayDirection;
    vec3 exit  = rayOrigin + intersection.y * rayDirection;

    const int MAX_STEPS = 100 * int((noise.semiAxes.x + noise.semiAxes.y + noise.semiAxes.z) / 3.f);
    float stepSize = length(exit - entry) / MAX_STEPS;

    float transmittance = 1.0;
    vec3 point = entry;
    for (int steps = 0; steps < MAX_STEPS; ++steps) {
        point -= rayDirection * stepSize;
        float density = getDensity(point);
        if (density > 0) {
            transmittance *= exp(-density * stepSize * noise.extinction);
            if (transmittance < 0.01)
              break;
        }
    }

    float grayValue = mix(0.6, 0.4, transmittance);
    color = vec4(grayValue, grayValue, grayValue, 1 - transmittance);
}