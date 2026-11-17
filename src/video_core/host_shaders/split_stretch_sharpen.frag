// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Split scale's last step: the upscaler's smaller result stretched over the output with RCAS in
// the same pass (temporal_sharpen.frag's range-compressed RCAS on bilinear taps one output pixel
// apart). The input is linear HDR.

#version 460 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : require

layout(binding = 0) uniform sampler2D InputTexture;

layout(push_constant) uniform PushConstants {
    // The vertex stage's texture scale and offset come first.
    layout(offset = 16) uvec4 Const0; // FsrRcasCon
};

layout(location = 0) in vec2 texcoord;
layout(location = 0) out vec4 color;

#define A_GPU 1
#define A_GLSL 1
#include "ffx_a.h"

vec3 Compress(vec3 c) {
    return c / (1.0 + max(max(c.r, c.g), c.b));
}

vec3 Expand(vec3 c) {
    c = clamp(c, vec3(0.0), vec3(1.0 - 1.0 / 65504.0));
    return c / (1.0 - max(max(c.r, c.g), c.b));
}

// This pixel's source position and one output pixel's step in the source.
vec2 center_uv;
vec2 step_x;
vec2 step_y;
ivec2 center_pixel;

#define FSR_RCAS_F 1
AF4 FsrRcasLoadF(ASU2 p) {
    const vec2 d = vec2(ivec2(p) - center_pixel);
    const vec2 uv = center_uv + d.x * step_x + d.y * step_y;
    return AF4(Compress(textureLod(InputTexture, uv, 0.0).rgb), 1.0);
}
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}

#include "ffx_fsr1.h"

void main() {
    center_uv = texcoord;
    step_x = dFdx(texcoord);
    step_y = dFdy(texcoord);
    center_pixel = ivec2(gl_FragCoord.xy);
    AF3 c;
    FsrRcasF(c.r, c.g, c.b, AU2(gl_FragCoord.xy), Const0);
    color = vec4(Expand(c), 1.0);
}
