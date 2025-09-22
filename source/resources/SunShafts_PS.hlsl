// Settings
#define NUM_SAMPLES 24
#define USE_CLOUD_MASK 1
#define USE_HORIZON_FADE 1

// Aliases
#define Intensity        FusionPS208.x // Overall intensity of the effect.
#define Density          FusionPS208.y // How long the rays are.
#define Decay            FusionPS208.z // How fast the rays fade out from the center.
#define Exposure         1.0f          // Effectively the same as Intensity, but the value is squared with two passes.
#define Weight           1.0f          // Same as Exposure, but only affects offset samples and not the center.

#define VolumetricFog    FusionPS210.y
#define SunDirection     float4(FusionPS216.xyz, 0.0f)
#define TexelSize        globalScreenSize.zw
#define AspectRatio      globalScreenSize.x * globalScreenSize.wz
#define CameraPos        gViewInverse[3].xyz
#define LogDepthParams   FusionPS209

// Vanilla resources
uniform row_major float4x4 gWorldViewProj : register(c8);
uniform row_major float4x4 gViewInverse   : register(c12);
uniform float4 globalScreenSize           : register(c44);     // This is at full resolution.
sampler2D GBufferTextureSampler3          : register(s1);
sampler2D HDRSampler                      : register(s2);

// Custom resources
uniform float4 FusionPS208                : register(c208);
uniform float4 FusionPS209                : register(c209);
uniform float4 FusionPS210                : register(c210);
uniform float4 FusionPS216                : register(c216);
sampler2D pHDRDownsampleTex               : register(s11);     // Custom downsample.
sampler2D DiffuseTex                      : register(s13);     // Diffuse; cloud mask.

// From rage_postfxVS0.
struct PS_IN
{
    float2 texcoord : TEXCOORD;
    float3 viewray : TEXCOORD1;
};

float DecodeLogDepth(float depth)
{
    return pow(abs(LogDepthParams.z), depth) * LogDepthParams.w;
}

float2 getScreenLightPos()
{
    float4 pos = mul(SunDirection, gWorldViewProj);
    pos.xy = float2(pos.x, -pos.y) / pos.w;
    return pos.xy * 0.5f + 0.5f;
}

float2 FixHalfPixelOffset(float2 uv)
{
    // The game applies the half pixel offset on the CPU, so it still uses the full resolution value in our custom half resolution passes.
    // Undo full resolution half pixel offset.
    uv -= 0.5f * TexelSize.xy;
    // Add half resolution half pixel offset.
    uv += 0.5f * TexelSize.xy * 2.0f;
    return uv;
}

float SunMask(float3 viewray, float depth)
{
    float3 viewdir = normalize(DecodeLogDepth(depth) * -viewray);
    float mask = dot(viewdir, SunDirection.xyz);
    // Sun baseline = ~saturate(mask * 249.995773315f - 248.995773315f).
    return mask >= 0.996f ? 1.0f : 0.0f;
}

float SkyMask(float depth)
{
    if (VolumetricFog == 1.0f)
        // Hack, includes some of the depth range if volumetric fog is enabled.
        return saturate(depth * 10.0f - 9.0f);
    else
        return depth == 1.0f ? 1.0f : 0.0f;
}

float EdgeFade(float2 uv)
{
    float2 rectangle = min(uv, 1.0f - uv) * AspectRatio;
    return saturate(32.0f * min(rectangle.x, rectangle.y));
}

float HorizonFade()
{
    // Fade as the sun rises or sets.
    // ~0.209101 = 07:00 / 19:00
    float azimuth = smoothstep(0.0f, 0.209101f, SunDirection.z);
    // Restore visibility at high altitudes.
    float altitude = 1.0f - exp2(-0.01f * max(CameraPos.z, 0.0f));
    return max(azimuth, altitude);
}

float4 SSPrepass(PS_IN i) : COLOR
{
    float2 uv = FixHalfPixelOffset(i.texcoord.xy);
    
    float3 color = tex2D(HDRSampler, uv).xyz;
    float depth = tex2D(GBufferTextureSampler3, uv).x;
    
    // Restrict to sky.
    color *= SkyMask(depth);
    // Restrict to sun.
    color *= SunMask(i.viewray.xyz, depth);
    // Fade out at the edges of the screen.
    color *= EdgeFade(uv);
    // Multiply by TimecycExt intensity.
    color *= Intensity;
    
    // Apply clouds occlusion.
    #if USE_CLOUD_MASK
        float clouds = tex2D(DiffuseTex, uv).x;
        color *= clouds;
    #endif
    // Fade out near the horizon if volumetric fog is enabled.
    #if USE_HORIZON_FADE
        color *= VolumetricFog == 1.0f ? HorizonFade() : 1.0f;
    #endif
    
    return float4(color, 1.0f);
}

float4 SSDraw(PS_IN i) : COLOR
{
    // https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-13-volumetric-light-scattering-post-process
    
    float2 uv = FixHalfPixelOffset(i.texcoord.xy);

    // Calculate vector from pixel to light source in screen space.
    float2 deltaTexCoord = uv - getScreenLightPos();
    // Divide by number of samples and scale by control factor.
    deltaTexCoord *= (1.0f / NUM_SAMPLES) * Density;
    // Store initial sample.
    float3 color = tex2D(pHDRDownsampleTex, uv).xyz;
    // Set up illumination decay factor.
    float illuminationDecay = 1.0f;
    // Evaluate summation from Equation 3 NUM_SAMPLES iterations.
    [unroll]
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        // Step sample location along ray.
        uv -= deltaTexCoord;
        // Retrieve sample at new location.
        float3 sample = tex2D(pHDRDownsampleTex, uv).xyz;
        // Apply sample attenuation scale/decay factors.
        sample *= illuminationDecay * Weight;
        // Accumulate combined color.
        color += sample;
        // Update exponential decay factor.
        illuminationDecay *= Decay;
    }
    // Output final color with a further scale control factor.
    return float4(color * Exposure, 1.0f);
}

float4 SSAdd(PS_IN i) : COLOR
{
    return tex2D(HDRSampler, i.texcoord) + tex2D(pHDRDownsampleTex, i.texcoord);
}
