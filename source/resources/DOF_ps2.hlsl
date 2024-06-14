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

int samples  = 2;

sampler2D HDRSampler            : register(ps, s0);
sampler2D Difuse                : register(ps, s1);
sampler2D Normal                : register(ps, s2);
sampler2D BloomSampler          : register(ps, s3);
sampler2D specular              : register(ps, s4);
sampler2D Depth                 : register(ps, s5);
sampler2D StencilCopySampler    : register(ps, s6);

float4 TexelSize2               : register(c210);
float FocusPoint: register(c212);
float FocusScale: register(c213);
	// float	maxRadius 	: register (c212);
	// float	blend 		: register (c213);

// uniform sampler2D HDRSampler; //Image to be processed
// uniform sampler2D depth; //Linear depth, where 1.0 == far plane
// uniform float2 TexelSize; //The size of a pixel: float2(1.0/width, 1.0/height)

float uFar = 1500; // Far plane
float GOLDEN_ANGLE = 2.39996323; 
float MAX_BLUR_SIZE = 20.0; 
float RAD_SCALE = 0.5; // Smaller = nicer blur, larger = faster

float getBlurSize(float depth, float focusPoint, float focusScale)
{
	float coc = clamp((1.0 / focusPoint - 1.0 / depth)*focusScale, -1.0, 1.0);
	return abs(coc) * MAX_BLUR_SIZE;
}

float logToLinear(float w){
	float z = log(w/znearfar.y) / log(znearfar.x/znearfar.y);
	return w;
}

float4 depthOfField(float2 texCoord, float focusPoint, float focusScale)
{
	float centerDepth = logToLinear(tex2D(Depth, texCoord).r) * uFar;
	float centerSize = abs(clamp((1.0 / focusPoint - 1.0 / centerDepth)*focusScale, -1.0, 1.0))* MAX_BLUR_SIZE;
	float3 color = tex2D(HDRSampler, texCoord).rgb;
	float tot = 1.0;
	float radius = RAD_SCALE;
	float ang = 0.0;
	for (int i = 0; i< 10 && radius<MAX_BLUR_SIZE; i++ )
	{
		float2 tc = texCoord + float2(cos(ang), sin(ang)) * TexelSize.xy * radius;
		float3 sampleColor = tex2D(HDRSampler, tc).rgb;
		float sampleDepth = logToLinear(tex2D(Depth, tc).r) * uFar;
		// float sampleSize = getBlurSize(sampleDepth, focusPoint, focusScale);
		float sampleSize = abs(clamp((1.0 / focusPoint - 1.0 / sampleDepth)*focusScale, -1.0, 1.0))* MAX_BLUR_SIZE;

		// if (sampleDepth > centerDepth)
			// sampleSize = clamp(sampleSize, 0.0, centerSize*2.0);
		float m = smoothstep(radius-0.5, radius+0.5, sampleSize);
		color += lerp(color/tot, sampleColor, m);
		tot += 1.0;   
		radius += RAD_SCALE/radius;
		ang += GOLDEN_ANGLE	
	}
	return float4(color /= tot, 1);
}


float4 main(float2 texcoord : TEXCOORD) : COLOR
{
	return depthOfField(texcoord, FocusPoint, FocusScale);
};

float4 main2(float2 texcoord : TEXCOORD) : COLOR
{
	float cnt = 0;
	float4 o = float4(0,0,0,0);
	for(float y=-samples; y<=samples; y++){
		for(float x=-samples; x<=samples; x++){
			o += tex2Dlod(HDRSampler, float4(x,y, 0, 0) * TexelSize + float4(texcoord, 0, 0));
			cnt++;
		};
	};
	return o / float4(cnt,cnt,cnt,cnt);
};






// SamplerState	samplerLinear	: register(s0);
// SamplerState	samplerPoint	: register(s1);
// Texture2D		TextureCoC		: register(t0, UPDATE_FREQ_PER_FRAME);
// Texture2D		TextureColor	: register(t1, UPDATE_FREQ_PER_FRAME);

// static const float GOLDEN_ANGLE = 2.39996323f; 
// static const float MAX_BLUR_SIZE = 20.0f; 
// static const float RAD_SCALE = 1.5; // Smaller = nicer blur, larger = faster

// float getBlurSize(float2 coc)
// {
	// if(coc.r > 0.0f)
		// return coc.r * maxRadius * MAX_BLUR_SIZE * 5;
	// return coc.g * maxRadius * MAX_BLUR_SIZE * 5;
// }

// float logToLinear(float w){
	// float z = log(w/znearfar.y) / log(znearfar.x/znearfar.y);
	// return z;
// }


// float4 main(float2 texcoord : TEXCOORD) : COLOR
// {
	// float2 pixelSize = TexelSize.xy;

	// float3 color = tex2D(HDRSampler, texcoord).rgb;
	
	// float2 coc = logToLinear(tex2D(depth, texcoord).x).rr;

	// float centerSize = getBlurSize(coc);

	// float total = 1.0;
	// float radius = RAD_SCALE;
	
	// [unroll(20)]
	// for (float ang = 0.0; radius < MAX_BLUR_SIZE; ang += GOLDEN_ANGLE)
	// {
		// float2 tc = texcoord + float2(cos(ang), sin(ang)) * pixelSize * radius;
		// float3 sampleColor = tex2D(HDRSampler, tc).rgb;

		// float3 sampleCoC = logToLinear(tex2D(depth, tc).r).rrr;
		// float sampleSize = getBlurSize(sampleCoC.xy);

		// // if (sampleSize > centerSize)
		// 	// sampleSize = clamp(sampleSize, 0.0, centerSize*2.0);

		// float m = smoothstep(radius - 0.5f, radius + 0.5f, sampleSize);
		// color += lerp(color/total, sampleColor, m);
		
		// total += 1.0;   
		// radius += RAD_SCALE / radius;
	// }

	// return float4(color / total, 1.0f);
// }


