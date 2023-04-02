#version 450
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint quadResolution;
    float minHeight;
    float maxHeight;
} params;

layout(binding = 1, set = 0) uniform Noise_t
{
  NoiseData noise;
};

layout (location = 0 ) out VS_OUT
{
    vec3 pos;
} vOut;

void main(void)
{
    vec3 pos = vec3(gl_VertexIndex / 4, gl_VertexIndex / 2 % 2 == 1, (gl_VertexIndex % 2) != 0);
    vOut.pos = (pos - vec3(0.5, 0.5, 0.5)) * 2 * noise.semiAxes;
    gl_Position   = params.mProjView * vec4(vOut.pos, 1.0);
}