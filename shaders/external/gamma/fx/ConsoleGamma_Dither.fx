float4 globalScreenSize : register(c44);
texture2D InputTex2D;

sampler2D FrameBufferSampler
{
    Texture = <InputTex2D>;

    AddressU = Clamp;
    AddressV = Clamp;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
};

void VSMain(in float4 iPosition : POSITION, in float2 iTexcoord : TEXCOORD0,
            out float4 oPosition : POSITION, out float2 oTexcoord : TEXCOORD0)
{
    oPosition = iPosition;
    oTexcoord = iTexcoord;
}

// Many XBOX 360 games, such as Grand Theft Auto IV and Saints Row 2, use these mismatched functions to apply a contrast boost to the image.
// Even some modern games still make use of them, like Ghost of Tsushima.
// https://www.desmos.com/calculator/z7scbxb6vn
float3 SRGBDecode(float3 color)
{
    float3 linearSection = color / 12.92f;
    float3 clampedColor = max(color, 0.0f);
    float3 powerSection = pow((clampedColor + 0.055f) / 1.055f, 2.4f);

    return (color >= 0.04045f) ? powerSection : linearSection;
}

float3 Rec709Encode(float3 color)
{
    float3 linearSection = color * 4.5f;
    float3 powerSection = 1.099f * pow(max(color, 0.0f), 0.45f) - 0.099f;

    return (color >= 0.018f) ? powerSection : linearSection;
}

// Low-discrepancy grid dither
// https://blog.demofox.org/2022/02/01/two-low-discrepancy-grids-plus-shaped-sampling-ldg-and-r2-ldg/
float R2LDG(float2 pos)
{
    return frac(dot(pos, float2(0.754877666247f, 0.569840290998f)));
}

float4 PSMain_BlitXenonGamma(float2 uv : TEXCOORD0) : COLOR0
{
    float4 color = tex2D(FrameBufferSampler, uv);

    color.rgb = Rec709Encode(SRGBDecode(color.rgb));

    float noise = R2LDG(globalScreenSize.xy * uv);
    noise -= 0.5f;

    color.rgb += noise * (1.0f / 255.0f);

    return color;
}

float4 PSMain_BlitCellGamma(float2 uv : TEXCOORD0) : COLOR0
{
    float4 color = tex2D(FrameBufferSampler, uv);

    float3 clampedColor = max(color.rgb, 0.0f);
    color.rgb = pow(clampedColor, 1.2f);

    float noise = R2LDG(globalScreenSize.xy * uv);
    noise -= 0.5f;

    color.rgb += noise * (1.0f / 255.0f);

    return color;
}

technique BlitXenonGamma
{
    pass P0
    {
        VertexShader = compile vs_3_0 VSMain();
        PixelShader = compile ps_3_0 PSMain_BlitXenonGamma();

        ZEnable = 0;
        ZWriteEnable = false;
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
        StencilEnable = false;
        CullMode = None;
        ScissorTestEnable = False;
    }
}

technique BlitCellGamma
{
    pass P0
    {
        VertexShader = compile vs_3_0 VSMain();
        PixelShader = compile ps_3_0 PSMain_BlitCellGamma();

        ZEnable = 0;
        ZWriteEnable = false;
        AlphaBlendEnable = false;
        AlphaTestEnable = false;
        StencilEnable = false;
        CullMode = None;
        ScissorTestEnable = False;
    }
}