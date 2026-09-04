sampler2D FrameBufferSampler : register(s0);

struct VS_INPUT
{
    float4 Position : POSITION0;
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 Position : POSITION0;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT VSMain(VS_INPUT In)
{
    VS_OUTPUT Out;

    Out.Position = In.Position;
    Out.TexCoord = In.TexCoord;

    return Out;
}

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

float4 PSMain(VS_OUTPUT In) : COLOR0
{
    float4 color = tex2D(FrameBufferSampler, In.TexCoord);

    color.rgb = Rec709Encode(SRGBDecode(color.rgb));

    return color;
}