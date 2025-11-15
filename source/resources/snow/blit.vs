uniform float4 PixelOffset : register(c5);

struct VS_IN
{
    float4 position : POSITION;
    float4 texcoord : TEXCOORD;
};

struct VS_OUT
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
};

VS_OUT main(VS_IN i)
{
    VS_OUT o;

    o.position.xy = PixelOffset.xy + i.position.xy;
    o.position.zw = i.position.zz * float2(1, 0) + float2(0, 1);
    o.texcoord = i.texcoord.xy;

    return o;
}