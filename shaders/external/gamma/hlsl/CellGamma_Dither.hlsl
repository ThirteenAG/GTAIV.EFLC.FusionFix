float4 globalScreenSize : register(c44);
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

// https://blog.demofox.org/2022/02/01/two-low-discrepancy-grids-plus-shaped-sampling-ldg-and-r2-ldg/
float R2LDG(float2 pos)
{
    return frac(dot(pos, float2(0.754877666247f, 0.569840290998f)));
}

float4 PSMain(VS_OUTPUT In) : COLOR0
{
    float4 color = tex2D(FrameBufferSampler, In.TexCoord);

    float3 clampedColor = max(color.rgb, 0.0f);
    color.rgb = pow(clampedColor, 1.2f);

    float noise = R2LDG(globalScreenSize.xy * In.TexCoord);
    noise -= 0.5f;

    color.rgb += noise * (1.0f / 255.0f);

    return color;
}