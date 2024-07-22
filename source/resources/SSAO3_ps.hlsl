float4 globalScreenSize             : register(c44);
float Exposure                      : register(c66);
row_major float4x4 motionBlurMatrix : register(c72);
float4 TexelSize                    : register(c76);
float4 dofProj                      : register(c77);
float4 dofDist                      : register(c78);
float4 dofBlur                      : register(c79);
float gDirectionalMotionBlurLength  : register(c80);
float4 ToneMapParams                : register(c81);
float4 deSatContrastGamma           : register(c82);
float4 ColorCorrect                 : register(c83);
float4 ColorShift                   : register(c84);
float PLAYER_MASK                   : register(c85);
float4 znearfar						: register(c128);
// float4 NoiseParams                  : register(c85);

int samples  : register(i4);

sampler2D HDRSampler            : register(ps, s2);
sampler2D Difuse                : register(ps, s5);
sampler2D Normal                : register(ps, s0);
sampler2D BloomSampler          : register(ps, s3);
sampler2D specular              : register(ps, s4);
sampler2D Depth                 : register(ps, s1);
sampler2D StencilCopySampler    : register(ps, s6);

float4 TexelSize2               : register(c210);

float logToLinear(float w){
	// float z = log(w/znearfar.x) / log(znearfar.y/znearfar.x);
	//float z = log2(w/znearfar.y) / log2(znearfar.x/znearfar.y);
	//float z = (znearfar.y*(w-znearfar.x))/(w*(znearfar.y-znearfar.x));
	float z = pow(znearfar.y/znearfar.x, w);
	return z;
}

float DepthDownsample(sampler2D tex, float2 uv){
	float z = tex2D(tex, uv).r;
	return z;
}
float4 TexDownsample(sampler2D tex, float2 uv){
	float4 z = tex2D(tex, uv);
	z.w = 1.0;
	return z;
}

float4 main(float2 texcoord : TEXCOORD) : COLOR
{
	float depth = logToLinear(tex2D(Depth, texcoord).r) ;
	return (float4(depth, depth, depth, 1))/znearfar.y;
};

float4 mainDownsample(float2 texcoord : TEXCOORD) : COLOR
{
	return TexDownsample(HDRSampler, texcoord) ;
};
