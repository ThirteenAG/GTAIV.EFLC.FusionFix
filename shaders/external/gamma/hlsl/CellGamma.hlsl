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

float4 PSMain(VS_OUTPUT In) : COLOR0
{
    float4 color = tex2D(FrameBufferSampler, In.TexCoord);

    float3 clampedColor = max(color.rgb, 0.0f);
    color.rgb = pow(clampedColor, 1.2f);

    return color;
}