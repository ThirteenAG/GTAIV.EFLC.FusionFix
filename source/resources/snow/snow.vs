uniform float4 DeferredProjParams : register(c0);
uniform row_major float4x4 ViewInverse : register(c1);
uniform float4 PixelOffset : register(c5);

struct VSIn
{
	float3 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct VSOut
{
	float4 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
	float3 TexCoord1 : TEXCOORD1;
};

VSOut main(VSIn vsIn)
{
	VSOut vsOut;
	vsOut.Position = float4(vsIn.Position.xyz, 1.0);
	vsOut.Position.xy += PixelOffset.xy;
	vsOut.TexCoord = vsIn.TexCoord;

	float2 r0 = vsIn.TexCoord.xy * 2 - 1;
	r0.y = -r0.y;
	r0 *= DeferredProjParams.xy;
	
	vsOut.TexCoord1 = mul(float4(-r0, 1.0, 0.0), ViewInverse).xyz;
	
	return vsOut;
}