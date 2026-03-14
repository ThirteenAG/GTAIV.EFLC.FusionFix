//  Modified Shader Model 3.0 port of SAO (Scalable Ambient Obscurance) (https://casual-effects.com/research/McGuire2012SAO/index.html)
//
//
//  Open Source under the "BSD" license: http://www.opensource.org/licenses/bsd-license.php
//
//  Copyright (c) 2011-2012, NVIDIA
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
//
//  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

texture AOTexture2D, AOCamDepthTexture2D, DepthTex2D;

sampler2D AOCamDepthTexture
{
    Texture = <AOCamDepthTexture2D>;
    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = Point;
    MinFilter = Point;
    MagFilter = Point;
};

sampler2D AOTexture
{
    Texture = <AOTexture2D>;
    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
};

sampler2D DepthTex
{
    Texture = <DepthTex2D>;
    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = Point;
    MinFilter = Point;
    MagFilter = Point;
};

uniform float2 vec2InvViewportSize;
uniform float fNearPlane;
uniform float fFarPlane;
uniform float fFarDivNear;
uniform float fRadius;
uniform float fBias;
uniform float fIntensity;
uniform float fProjScale;
uniform float4 vec4ProjInfo;
uniform float2 vec2BlurDirection;

#ifndef NUM_SAMPLES
#define NUM_SAMPLES 9
#endif
#ifndef LOG_MAX_OFFSET
#define LOG_MAX_OFFSET 3
#endif
#ifndef MAX_MIP_LEVEL
#define MAX_MIP_LEVEL 5
#endif
#ifndef FAR_CLIP
#define FAR_CLIP 150.0f
#endif
static const float PI = 3.14159265;

float LogDepthToViewZ(float logDepth)
{
    return pow(fFarDivNear, logDepth) * fNearPlane;
}

// Reconstruct view-space position from depth
float3 ReconstructViewPos(float2 S, float z)
{
    return float3(((S.xy + 0.5f) * vec4ProjInfo.xy + vec4ProjInfo.zw) * z, z);
}

float3 ReconstructNormal(float3 pos)
{
    return normalize(cross(ddy(pos), ddx(pos)));
}

float GetRandomRotationAngle(float2 vPos)
{
    return 2 * PI * frac(52.9829189 * frac(dot(vPos, float2(0.06711056, 0.00583715))));
}

float3 getOffsetPosition(float2 ssC, float2 unitOffset, float ssR)
{
    float2 ssP = ssC + ssR * unitOffset;

    int mipLevel = clamp((int) floor(log2(ssR)) - LOG_MAX_OFFSET, 0, MAX_MIP_LEVEL);

    // --- Sample depth ---
    float z = tex2Dlod(
        AOCamDepthTexture,
        float4(ssP * vec2InvViewportSize, 0.0, (float) mipLevel)
    ).r;

    // --- Reconstruct view-space position ---
    return ReconstructViewPos(ssP, z);
}

float2 tapLocation(int sampleNumber, float spinAngle, out float ssR)
{
	// Radius relative to ssR
    float alpha = float(sampleNumber + 0.5) * (1.0 / float(NUM_SAMPLES));
    float angle = alpha * (7 * 6.28) + spinAngle;

    ssR = alpha;
    return float2(cos(angle), sin(angle));
}

float sampleAO(in float2 ssC, in float3 C, in float3 n_C, in float ssDiskRadius, in int tapIndex, in float randomPatternRotationAngle)
{
	// Offset on the unit disk, spun for this pixel
    float ssR;
    float2 unitOffset = tapLocation(tapIndex, randomPatternRotationAngle, ssR);
    ssR *= ssDiskRadius;

	// The occluding point in camera space
    float3 Q = getOffsetPosition(ssC, unitOffset, ssR);

    float3 v = Q - C;

    float vv = dot(v, v);
    float vn = dot(v, n_C);

    const float epsilon = 0.01;
    float f = max((fRadius * fRadius) - vv, 0.0);
    return f * f * f * max((vn - fBias) / (epsilon + vv), 0.0);
}

float2 BilateralBlur(sampler2D Texture, sampler2D CamDepthTexture, in float2 uv)
{ // thanks to Parallellines0451
    static const float BilateralDepthTreshold = 0.03;
    
    static const float GaussianWeights[9] =
    {
        0.0276305506389,
	0.0662822452864,
	0.123831536806,
	0.180173822911,
	0.204163688715,
	0.180173822911,
	0.123831536806,
	0.0662822452864,
	0.0276305506389
    };
    
    static const float2 UVOffsets[9] =
    {
        -4.0f, -4.0f,
	-3.0f, -3.0f,
	-2.0f, -2.0f,
	-1.0f, -1.0f,
	 0.0f, 0.0f,
	 1.0f, 1.0f,
	 2.0f, 2.0f,
	 3.0f, 3.0f,
	 4.0f, 4.0f,
    };
    
    float2 refTap;
    refTap.x = tex2D(Texture, uv).x;
    refTap.y = tex2D(CamDepthTexture, uv).x;

    const float blurThreshold = BilateralDepthTreshold * refTap.y;

    float blurredValue = 0;
    
    float2 texel = vec2BlurDirection;

	[unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 offsetUV = uv + (UVOffsets[i].xy * texel);
        float2 tap;
        tap.x = tex2D(Texture, offsetUV).x;
        tap.y = tex2D(CamDepthTexture, offsetUV).x;
        float weight = GaussianWeights[i];

        blurredValue += weight * ((abs(refTap.y - tap.y) < blurThreshold) ? tap.x : refTap.x);
    }

    return float2(blurredValue, refTap.y);
}

float4 ComputeAO_PS(float2 uv : TEXCOORD0, float2 vPos : VPOS) : COLOR0
{
    float depth = tex2D(AOCamDepthTexture, uv).r;

    clip(FAR_CLIP - depth);
    
	// World space point being shaded
        
    float2 ssC = vPos;
    float3 C = ReconstructViewPos(ssC, depth);

    float randomPatternRotationAngle = GetRandomRotationAngle(ssC);

	// Reconstruct normals from positions. These will lead to 1-pixel black lines
	// at depth discontinuities, however the blur will wipe those out so they are not visible
	// in the final image.
    float3 n_C = ReconstructNormal(C);

	// Choose the screen-space sample radius
	// proportional to the projected area of the sphere
    float ssDiskRadius = -fProjScale * fRadius / C.z;
    
    float sum = 0.0;
    [unroll]
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
        sum += sampleAO(ssC, C, n_C, ssDiskRadius, i, randomPatternRotationAngle);
    }
    
    float temp = fRadius * fRadius * fRadius;
    sum /= temp * temp;
    float A = max(0.0, 1.0 - sum * fIntensity * (5.0 / NUM_SAMPLES));

	// Bilateral box-filter over a quad for free, respecting depth edges
	// (the difference that this makes is subtle)
    
    int2 pixel = ssC;
    
    if (abs(ddx(C.z)) < 0.02)
    {
        A -= ddx(A) * (float(pixel.x % 2) - 0.5);
    }
    if (abs(ddy(C.z)) < 0.02)
    {
        A -= ddy(A) * (float(pixel.y % 2) - 0.5);
    }
    
    static const float nearFade = FAR_CLIP - FAR_CLIP * 0.95f;  
    static const float farFade = FAR_CLIP;
    
    float t = saturate((C.z - nearFade) / (farFade - nearFade));
    A = lerp(A, 1.0f, t);
    
    return float4(A, A, A, 1.0);
}

float4 BlurAOToBuffer_PS(float2 uv : TEXCOORD0) : COLOR0
{
    float2 blur = BilateralBlur(AOTexture, AOCamDepthTexture, uv);
    return float4(blur.x, blur.x, blur.x, 1);
}

float4 OutputAO_PS(float2 uv : TEXCOORD0) : COLOR0
{
    float ao = tex2D(AOTexture, uv).r;
    return float4(0, 0, 0, 1.0 - ao);
}

float4 ComputeCamDepth_PS(float2 uv : TEXCOORD0) : COLOR0
{
    float viewZ = LogDepthToViewZ(tex2D(DepthTex, uv).r);
    return float4(viewZ, 0.0, 0.0, 0.0);
}

float2 vec2PrevMipSize;
float2 vec2PrevMipTexel;
int iPreviousMip;

float4 CamDepthMinify_PS(float2 fragCoord : VPOS) : COLOR0
{
    int2 ssP = int2(fragCoord.xy + 0.5);

    int2 offset = int2(ssP.y % 2, ssP.x % 2);
    int2 coord = ssP * 2 + offset;
    coord = clamp(coord, int2(0, 0), int2(vec2PrevMipSize) - 1);

    float2 uv = coord * vec2PrevMipTexel;

    return tex2Dlod(AOCamDepthTexture, float4(uv, 0, iPreviousMip));
}

void FullscreenQuadVS(in float4 iPos : POSITION, in float2 iUV : TEXCOORD0,
                      out float4 oPos : POSITION, out float2 oUV : TEXCOORD0)
{
    oPos = iPos;
    oUV = iUV;
}


technique AmbientOcclusion
{
    pass ComputeCamDepth
    {
        PixelShader = compile ps_3_0 ComputeCamDepth_PS();
        VertexShader = compile vs_3_0 FullscreenQuadVS();
        AlphaBlendEnable = FALSE;
        AlphaTestEnable = FALSE;
        ZEnable = 0;
        ZWriteEnable = FALSE;
        StencilEnable = FALSE;
    }
    pass CamDepthMinify
    {
        PixelShader = compile ps_3_0 CamDepthMinify_PS();
        VertexShader = compile vs_3_0 FullscreenQuadVS();
        AlphaBlendEnable = FALSE;
        AlphaTestEnable = FALSE;
        ZEnable = 0;
        ZWriteEnable = FALSE;
        StencilEnable = FALSE;
    }
    pass ComputeAO
    {
        PixelShader = compile ps_3_0 ComputeAO_PS();
        VertexShader = compile vs_3_0 FullscreenQuadVS();
        AlphaBlendEnable = FALSE;
        AlphaTestEnable = FALSE;
        ZEnable = 0;
        ZWriteEnable = FALSE;
        StencilEnable = FALSE;
        CullMode = NONE;
        FogEnable = FALSE;
        Clipping = FALSE;
    }
    pass BlurAOToBuffer
    {
        PixelShader = compile ps_3_0 BlurAOToBuffer_PS();
        VertexShader = compile vs_3_0 FullscreenQuadVS();
        AlphaBlendEnable = FALSE;
        AlphaTestEnable = FALSE;
        ZEnable = 0;
        ZWriteEnable = FALSE;
        StencilEnable = FALSE;
        CullMode = NONE;
        FogEnable = FALSE;
        Clipping = FALSE;
    }
    pass OutputAO
    {
        PixelShader = compile ps_3_0 OutputAO_PS();
        VertexShader = compile vs_3_0 FullscreenQuadVS();
        AlphaBlendEnable = TRUE;
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
        AlphaTestEnable = FALSE;
        ZEnable = 0;
        ZWriteEnable = FALSE;
        StencilEnable = FALSE;
        CullMode = NONE;
        FogEnable = FALSE;
        Clipping = FALSE;   
        ColorWriteEnable = BLUE; // output ao
    }
}