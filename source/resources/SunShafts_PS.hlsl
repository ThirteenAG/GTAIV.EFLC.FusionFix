float4 gDirectionalColour             : register(c18);
float4 globalScreenSize               : register(c44);
float Exposure                        : register(c66);
row_major float4x4 motionBlurMatrix   : register(c72);
float4 TexelSize                      : register(c76);
float4 dofProj                        : register(c77);
float4 dofDist                        : register(c78);
float4 dofBlur                        : register(c79);
float gDirectionalMotionBlurLength    : register(c80);
float4 ToneMapParams                  : register(c81);
float4 deSatContrastGamma             : register(c82);
float4 ColorCorrect                   : register(c83);
float4 ColorShift                     : register(c84);
float PLAYER_MASK                     : register(c85);

sampler2D GBufferTextureSampler2      : register(ps, s0); // spec
sampler2D GBufferTextureSampler3      : register(ps, s1); // depth
sampler2D HDRSampler                  : register(ps, s2);
sampler2D BloomSampler                : register(ps, s3);
sampler2D AdapLumSampler              : register(ps, s4);
sampler2D JitterSampler               : register(ps, s5);
sampler2D StencilCopySampler          : register(ps, s6);

sampler2D HDRHalfTex                  : register(ps, s8); // game downsampled
sampler3D bluenoisevolume             : register(ps, s9);
sampler2D pHDRDownsampleTex           : register(ps, s11); // my downsample
sampler2D DiffuseTex                  : register(ps, s13); // diffuse

float4 globalFogParams       : register( c41 );
float4 globalFogColor        : register( c42 );
float4 globalFogColorN       : register( c43 );

float4 NoiseSale : register(c144);
float4 blueTimerVec4 : register(c218);

float4	 SS_params   : register(c96);
float4	 SS_params2   : register(c99);
float4	 SunDirection   : register(c97);
float4	 SunColor   : register(c98);

row_major float4x4 gWorld         : register(c100);
row_major float4x4 gWorldView     : register(c104);
row_major float4x4 gWorldViewProj : register(c108);
row_major float4x4 gViewInverse   : register(c112);
row_major float4x4 gShadowMatrix  : register(c116);

struct PS_IN
{
	float2 texcoord : TEXCOORD;
	float4 texcoord1 : TEXCOORD1;
};

// #define NUM_SAMPLES 25
int NUM_SAMPLES : register(ps, i5);
int NUM_SAMPLES2 : register(ps, i6);
float4 NumSamples : register( c120);

float InterleavedGradientNoise(float2 position_screen){
  float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
  return frac(magic.z * frac(dot(position_screen, magic.xy)));
}

// to use in downsampled texture
float4 SunShafts1(PS_IN i) : COLOR {
	float4 pos = mul( float3(SunDirection.x,-SunDirection.z,SunDirection.y), gWorldViewProj);
	float4 o = tex2D(HDRSampler, i.texcoord);
	// float p = saturate((0.1-distance(float2(pos.x,pos.y), (i.texcoord.xy)*2-1))*10 * saturate(pos.z))*10;

	pos.y = -pos.y;
	pos.xy/=pos.w;
	pos.xy=pos.xy*0.5+0.5;
	float2 uv=i.texcoord; 
	float2 ScreenLightPos = pos.xy;

	//float2 uv2 = (uv.xy) - ((ScreenLightPos.xy)*((1/SS_params2.x)*(0.25)) - (((1/SS_params2.x))*(0.125)));
	//float2 uv2 =   ((uv)/((SS_params2.x)*(0.25)) + (((SS_params2.x))*(0.125)))-(ScreenLightPos.xy);
	float2 uv2 =  1 + (uv - SS_params2.x * 0.125 - ScreenLightPos.xy) / (SS_params2.x * 0.25);
		
	float Weight = SS_params.x; // 0.3;
	float Density = SS_params.y; // 0.3;
	float exposure = (SS_params.z *0.15) / Exposure; // 0.015;
	float Decay = SS_params.w; // 0.998;
	// Calculate vector from pixel to light source in screen space.
	float2 deltaTexCoord = (uv2 - float2(0.5, 0.5));//+0.001*(tex3D(bluenoisevolume, float3(uv*blueTimerVec4.xy, blueTimerVec4.z)));
	// Divide by number of samples and scale by control factor.
	deltaTexCoord *= 1.0f / NumSamples.x * Density;
	// Store initial sample.
	float3 color = float3(0.0, 0.0, 0.0);
	color = tex2D(pHDRDownsampleTex, uv2).xyz*10;
	
	// Set up illumination decay factor.
	float illuminationDecay = 1.0f;
	float3 SunCol= float3(1.0, 0.3, 0.2);
	float2 ratio = float2(globalScreenSize.x / globalScreenSize.y, 1.0);
	// Evaluate summation from Equation 3 NUM_SAMPLES.x iterations.
	for (int i = 0; i < NUM_SAMPLES.x; i++) {
		// Step sample location along ray.
		uv2-=deltaTexCoord;//InterleavedGradientNoise
		//uv2 = lerp(uv2-deltaTexCoord, uv2, 0.0+0.08*tex3D(bluenoisevolume, float3(uv*blueTimerVec4.xy*20, blueTimerVec4.z+(i/float(4*NumSamples.x)))));
		//uv2 = lerp(uv2-deltaTexCoord, uv2, 0.04*InterleavedGradientNoise(uv*globalScreenSize+uv*globalScreenSize*(0.5*1/NumSamples.x*blueTimerVec4.z)));
		// Retrieve sample at new location.
		// HDRSampler, pHDRDownsampleTex, DiffuseTex, BloomSampler, HDRHalfTex
		float3 sample = tex2D(pHDRDownsampleTex, uv2);
		// No sky check
		//float3 sample = tex2D(DiffuseTex, uv) * float3(1.0, 0.3, 0.2);

		// Apply sample attenuation scale/decay factors.
		sample *= illuminationDecay * Weight;
		// Accumulate combined color.
		color += sample;
		// Update exponential decay factor.
		illuminationDecay *= Decay;
	  }
	//return (0.1-distance(pos.xy, i.texcoord.xy))*30;
	// Output final color with a further scale control factor.	
	float4 ss = clamp(float4( color * exposure, 1)
		* saturate(pos.z * 1.85 - 0.5)
		* saturate(
		  pow(SunDirection.z + 0.19, 2) * 40
		* dot(float2(0.5, 0.5), pos.xy)
	), 0, 20) * Exposure * 20;
	return ss ;

}

float4 SunShafts2(PS_IN i) : COLOR {
	float4 pos = mul( float3(SunDirection.x,-SunDirection.z,SunDirection.y), gWorldViewProj);
	float4 o = tex2D(HDRSampler, i.texcoord);
	// float p = saturate((0.1-distance(float2(pos.x,pos.y), (i.texcoord.xy)*2-1))*10 * saturate(pos.z))*10;

	pos.y = -pos.y;
	pos.xy/=pos.w;
	pos.xy=pos.xy*0.5+0.5;
	float2 uv=i.texcoord;
	float2 ScreenLightPos=pos.xy;


	float Weight = SS_params.x; // 0.3;
	float Density = SS_params.y; // 0.3;
	float exposure = (SS_params.z *0.15) / Exposure; // 0.015;
	float Decay = SS_params.w; // 0.998;
	// Calculate vector from pixel to light source in screen space.
	float2 deltaTexCoord = (uv - ScreenLightPos.xy);
	// Divide by number of samples and scale by control factor.
	// float samples = 0;
	// for(int i = 0; i<NUM_SAMPLES.x; i++){
		// samples += 1.0;
	// }
	deltaTexCoord *= 1.0f / NumSamples.x * Density;
	// Store initial sample.
	float3 color = float3(0.0, 0.0, 0.0);;
	color = tex2D(pHDRDownsampleTex, uv).xyz*0;
	
	// Set up illumination decay factor.
	float illuminationDecay = 1.0f;
	float3 SunCol= float3(1.0, 0.3, 0.2);
	float2 ratio = float2(globalScreenSize.x / globalScreenSize.y, 1.0);
	// Evaluate summation from Equation 3 NUM_SAMPLES iterations.
	for (int i = 0; i < NUM_SAMPLES.x; i++) {
		// Step sample location along ray.
		uv -= deltaTexCoord;
		//uv = lerp(uv-deltaTexCoord, uv, 0.0+0.2*tex3D(bluenoisevolume, float3(uv*blueTimerVec4.xy*1, blueTimerVec4.z+(i/float(4*NumSamples.x)))));
		//uv = lerp(uv-deltaTexCoord, uv, 0.04*InterleavedGradientNoise(uv*globalScreenSize.xy+uv*globalScreenSize*(2*1/NumSamples.x+blueTimerVec4.z)));
		// Retrieve sample at new location.
		// HDRSampler, pHDRDownsampleTex, DiffuseTex, BloomSampler, HDRHalfTex
		float3 sample = tex2D(pHDRDownsampleTex, uv);
		// No sky check
		//float3 sample = tex2D(DiffuseTex, uv) * float3(1.0, 0.3, 0.2);

		// Apply sample attenuation scale/decay factors.
		sample *= illuminationDecay * Weight;
		// Accumulate combined color.
		color += sample;
		// Update exponential decay factor.
		illuminationDecay *= Decay;
	  }
	// Output final color with a further scale control factor.
	//return (0.1-distance(pos.xy, i.texcoord.xy))*30;
	float4 ss = float4(clamp(color * exposure
		//* saturate(pos.z * 1.5 - 0.5)
		//* saturate(
		 //pow(SunDirection.z + 0.15, 2) * 40
		//* dot(float2(0.5, 0.5), pos.xy)
	, 0, 20) * Exposure * 20,  1);
	return ss ;
}

// gpugems3-chapter-13-volumetric-light-scattering-post-process
float4 SunShafts3(PS_IN i) : COLOR {
	float4 pos = mul( float3(SunDirection.x,-SunDirection.z,SunDirection.y), gWorldViewProj);
	float4 o = tex2D(HDRSampler, i.texcoord);
	// float p = saturate((0.1-distance(float2(pos.x,pos.y), (i.texcoord.xy)*2-1))*10 * saturate(pos.z))*10;

	pos.y = -pos.y;
	pos.xy /= pos.w;
	pos.xy = pos.xy*0.5+0.5;
	float2 uv = i.texcoord;
	float2 ScreenLightPos=pos.xy;

	float Weight = SS_params.x; // 0.3;
	float Density = SS_params.y; // 0.3;
	float exposure = (SS_params.z *0.15) / Exposure; // 0.015;
	float Decay = SS_params.w; // 0.998;
	// Calculate vector from pixel to light source in screen space.
	float2 deltaTexCoord = (uv - ScreenLightPos.xy);
	// Divide by number of samples and scale by control factor.
	// float samples = 0;
	// for(int i = 0; i<NUM_SAMPLES2; i++){
		// samples += 1.0;
	// }
	deltaTexCoord *= 1.0f / NumSamples.x * Density;
	// Store initial sample.
	// float3 color = tex2D(HDRSampler, uv).xyz;
	float3 color = float3(0.0, 0.0, 0.0);;
	// Set up illumination decay factor.
	float illuminationDecay = 1.0f;
	float3 SunCol= float3(1.0, 0.3, 0.2);
	float2 ratio = float2(globalScreenSize.x / globalScreenSize.y, 1.0);
	// Evaluate summation from Equation 3 NUM_SAMPLES iterations.
	for (int i = 0; i < NUM_SAMPLES.x; i++) {
		// Step sample location along ray.
		uv -= deltaTexCoord;
		// Retrieve sample at new location.
		// HDRSampler, pHDRDownsampleTex, DiffuseTex, BloomSampler, HDRHalfTex
		float3 sample = tex2D(pHDRDownsampleTex, uv);
		// No sky check
		//float3 sample = tex2D(DiffuseTex, uv) * float3(1.0, 0.3, 0.2);

		// Apply sample attenuation scale/decay factors.
		sample *= illuminationDecay * Weight;
		// Accumulate combined color.
		color += sample;
		// Update exponential decay factor.
		illuminationDecay *= Decay;
	  }
	// Output final color with a further scale control factor.
	float4 ss = float4(clamp( color * exposure
		* saturate(pos.z * 1.85 - 0.5)
		* saturate(
		  pow(SunDirection.z + 0.19, 2) * 40
		* dot(float2(0.5, 0.5), pos.xy)
	), 0, 20) * Exposure * 20, 1);
	return ss + o;
}

float4 SunShafts4(PS_IN i) : COLOR {
	float4 pos = mul( float3(SunDirection.x,-SunDirection.z,SunDirection.y), gWorldViewProj);
	float4 o = tex2D(HDRSampler, i.texcoord);
	// float p = saturate((0.1-distance(float2(pos.x,pos.y), (i.texcoord.xy)*2-1))*10 * saturate(pos.z))*10;

	pos.y = -pos.y;
	pos.xy/=pos.w;
	pos.xy=pos.xy*0.5+0.5;
	float2 uv=i.texcoord;
	float2 ScreenLightPos=pos.xy;

	//float2 uv2 = (uv.xy) - ((ScreenLightPos)*((1/SS_params2.x)*(0.25)) - (((1/SS_params2.x))*(0.125)));
	//float2 uv2 =   ((uv)/((SS_params2.x)*(0.25)) + (((SS_params2.x))*(0.125)))-(ScreenLightPos);
	float2 uv2 =  uv;//1 + (uv - SS_params2.x * 0.125 - ScreenLightPos.xy) / (SS_params2.x * 0.25);
	
	
	float Weight = SS_params.x; // 0.3;
	float Density = SS_params.y; // 0.3;
	float exposure = (SS_params.z *0.15) / Exposure; // 0.015;
	float Decay = SS_params.w; // 0.998;
	// Calculate vector from pixel to light source in screen space.
	//float2 deltaTexCoord = (uv2 - float2(0.5, 0.5));//+0.001*(tex3D(bluenoisevolume, float3(uv*blueTimerVec4.xy, blueTimerVec4.z)));
	float2 deltaTexCoord = (uv - ScreenLightPos.xy);
	// Divide by number of samples and scale by control factor.
	deltaTexCoord *= 1.0f / NumSamples.x * Density;
	// Store initial sample.
	// float3 color = tex2D(HDRSampler, uv2).xyz;
	float3 color = float3(0.0, 0.0, 0.0);
	// Set up illumination decay factor.
	float illuminationDecay = 1.0f;
	float3 SunCol= float3(1.0, 0.3, 0.2);
	float2 ratio = float2(globalScreenSize.x / globalScreenSize.y, 1.0);
	// Evaluate summation from Equation 3 NUM_SAMPLES iterations.
	for (int i = 0; i < NUM_SAMPLES.x; i++) {
		// Step sample location along ray.
		//uv2-=deltaTexCoord;
		uv2 = lerp(uv2-deltaTexCoord, uv2, -(NumSamples.y*0.005)+NumSamples.y*0.01*tex3D(bluenoisevolume, float3(uv*blueTimerVec4.xy, blueTimerVec4.z+i/NumSamples.x)));
		//uv2 = lerp(uv2-deltaTexCoord, uv2, -(NumSamples.y*0.005)+NumSamples.y*0.01*InterleavedGradientNoise(uv*globalScreenSize.xy+blueTimerVec4.z*16+i));
		// Retrieve sample at new location.
		// HDRSampler, pHDRDownsampleTex, DiffuseTex, BloomSampler, HDRHalfTex
		float3 sample = tex2D(HDRSampler, uv2)*
		pow(saturate(SS_params2.x * 0.1 - distance(uv2 * ratio, ScreenLightPos * ratio)), SS_params2.y * 0.1)
		;
		// No sky check
		//float3 sample = tex2D(DiffuseTex, uv) * float3(1.0, 0.3, 0.2);

		// Apply sample attenuation scale/decay factors.
		sample *= illuminationDecay * Weight;
		// Accumulate combined color.
		color += sample;
		// Update exponential decay factor.
		illuminationDecay *= Decay;
	  }
	// Output final color with a further scale control factor.
	float4 ss = float4(clamp( color * exposure
		* saturate(pos.z * 1.85 - 0.5)
		* saturate(
		  pow(SunDirection.z + 0.19, 2) * 40
		* dot(float2(0.5, 0.5), pos.xy)
	), 0, 20) * Exposure * 20
	, 1);
	return ss + o;
}

float4 SunShafts5(PS_IN i) : COLOR {
	float4 pos = mul( float3(SunDirection.x,-SunDirection.z,SunDirection.y), gWorldViewProj);
	float4 o = tex2D(HDRSampler, i.texcoord);
	// float p = saturate((0.1-distance(float2(pos.x,pos.y), (i.texcoord.xy)*2-1))*10 * saturate(pos.z))*10;

	pos.y = -pos.y;
	pos.xy/=pos.w;
	pos.xy=pos.xy*0.5+0.5;
	float2 uv=i.texcoord;
	float2 ScreenLightPos=pos.xy;

	//float2 uv2 = (uv.xy) - ((ScreenLightPos)*((1/SS_params2.x)*(0.25)) - (((1/SS_params2.x))*(0.125)));
	//float2 uv2 =   ((uv)/((SS_params2.x)*(0.25)) + (((SS_params2.x))*(0.125)))-(ScreenLightPos);
	float2 uv2 =  1 + (uv - SS_params2.x * 0.125 - ScreenLightPos.xy) / (SS_params2.x * 0.25);
	
	
	float Weight = SS_params.x; // 0.3;
	float Density = SS_params.y; // 0.3;
	float exposure = (SS_params.z *0.15) / Exposure; // 0.015;
	float Decay = SS_params.w; // 0.998;
	// Calculate vector from pixel to light source in screen space.
	float2 deltaTexCoord = (uv2 - float2(0.5, 0.5));//+0.001*(tex3D(bluenoisevolume, float3(uv*blueTimerVec4.xy, blueTimerVec4.z)));
	// Divide by number of samples and scale by control factor.
	deltaTexCoord *= 1.0f / NumSamples.x * Density;
	// Store initial sample.
	// float3 color = tex2D(HDRSampler, uv2).xyz;
	float3 color = float3(0.0, 0.0, 0.0);
	// Set up illumination decay factor.
	float illuminationDecay = 1.0f;
	float3 SunCol= float3(1.0, 0.3, 0.2);
	float2 ratio = float2(globalScreenSize.x / globalScreenSize.y, 1.0);
	// Evaluate summation from Equation 3 NUM_SAMPLES iterations.
	for (int i = 0; i < NUM_SAMPLES.x; i++) {
		// Step sample location along ray.
		//uv2-=deltaTexCoord;
		uv2 = lerp(uv2-deltaTexCoord, uv2, 0.0+0.06*tex3D(bluenoisevolume, float3(uv*blueTimerVec4.xy*20, blueTimerVec4.z+(i/float(4*NumSamples.x)))));
		// Retrieve sample at new location.
		// HDRSampler, pHDRDownsampleTex, DiffuseTex, BloomSampler, HDRHalfTex
		float3 sample = tex2D(pHDRDownsampleTex, uv2);
		// No sky check
		//float3 sample = tex2D(DiffuseTex, uv) * float3(1.0, 0.3, 0.2);

		// Apply sample attenuation scale/decay factors.
		sample *= illuminationDecay * Weight;
		// Accumulate combined color.
		color += sample;
		// Update exponential decay factor.
		illuminationDecay *= Decay;
	  }
	// Output final color with a further scale control factor.
	float4 ss = float4(clamp(color * exposure
		//* saturate(pos.z * 1.5 - 0.5)
		//* saturate(
		 //pow(SunDirection.z + 0.15, 2) * 40
		//* dot(float2(0.5, 0.5), pos.xy)
	, 0, 20) * Exposure * 20,  1);
	return ss ;
}


float4 SSDownsampler(PS_IN i) : COLOR {

	float4 pos = mul( float3(SunDirection.x,-SunDirection.z,SunDirection.y), gWorldViewProj);
	pos.y = -pos.y;
	pos.xy/=pos.w;
	pos.xy=pos.xy*0.5+0.5;

	float2 uv = i.texcoord; 
	uv = (pos.xy) - ((1-uv)*(SS_params2.x*0.25) - ((SS_params2.x)*0.125));
	float2 ScreenLightPos = pos.xy;
	float2 ratio = float2(globalScreenSize.x/globalScreenSize.y, 1.0);
	float3 sample = tex2D(HDRSampler, uv) * SunColor * SS_params2.w *
        saturate(pow(tex2D(GBufferTextureSampler3, uv).x, SS_params2.z * 50)) * // depth
        pow(saturate(SS_params2.x * 0.1 - distance(uv * ratio, ScreenLightPos * ratio)), SS_params2.y * 0.1) // sun screen
        * saturate(pow((tex2D(DiffuseTex, uv).z) * 1, 1)) 
        * (1 - globalFogParams.w) // fog density
        ;
    return float4(sample, 1);
}

float4 SSDownsampler2(PS_IN i) : COLOR {
	float4 pos = mul( float3(SunDirection.x,-SunDirection.z,SunDirection.y), gWorldViewProj);
	pos.y = -pos.y;
	pos.xy/=pos.w;
	pos.xy=pos.xy*0.5+0.5;
	float2 uv = i.texcoord; 
	
	float2 ScreenLightPos = pos.xy;
	float2 ratio = float2(globalScreenSize.x/globalScreenSize.y, 1.0);
	float3 sample = tex2D(HDRSampler, uv) * SunColor * SS_params2.w * 
        saturate(pow(tex2D(GBufferTextureSampler3, uv).x, SS_params2.z * 50)) * // depth
        pow(saturate(SS_params2.x * 0.1 - distance(uv * ratio, ScreenLightPos * ratio)), SS_params2.y * 0.1)
        * saturate(pow((tex2D(DiffuseTex, uv).z) * 1, 1)) 
		* (1 - globalFogParams.w);
    return float4(sample, 1);
}


float4 SSAdd(PS_IN i) : COLOR {
	return tex2D(HDRSampler, i.texcoord) + tex2D(pHDRDownsampleTex, i.texcoord + TexelSize.xy*0.5);
}


/*
float4 main(PS_IN i) : COLOR {
	float4 pos = mul( float3(SunDirection.x,-SunDirection.z,SunDirection.y), gWorldViewProj);
	float4 o = tex2D(HDRSampler, i.texcoord);
	// float p = saturate((0.1-distance(float2(pos.x,pos.y), (i.texcoord.xy)*2-1))*10 * saturate(pos.z))*10;

	pos.y = -pos.y;
	pos.xy/=pos.w;
	pos.xy=pos.xy*0.5+0.5;
	//return (0.1-distance(pos.xy, i.texcoord.xy))*30;
	float4 ss = clamp(SunShafts(i.texcoord, pos.xy)
		* saturate(pos.z * 1.85 - 0.5)
		* saturate(
		  pow(SunDirection.z + 0.19, 2) * 40
		* dot(float2(0.5, 0.5), pos.xy)
	), 0, 20) * Exposure * 20;
	return ss ;
}


float4 main2(PS_IN i) : COLOR {
	float4 pos = mul( float3(SunDirection.x,-SunDirection.z,SunDirection.y), gWorldViewProj);
	float4 o = tex2D(HDRSampler, i.texcoord);
	// float p = saturate((0.1-distance(float2(pos.x,pos.y), (i.texcoord.xy)*2-1))*10 * saturate(pos.z))*10;

	pos.y = -pos.y;
	pos.xy/=pos.w;
	pos.xy=pos.xy*0.5+0.5;
	//return (0.1-distance(pos.xy, i.texcoord.xy))*30;
	float4 ss = clamp(SunShafts2(i.texcoord, pos.xy)
		//* saturate(pos.z * 1.5 - 0.5)
		//* saturate(
		 //pow(SunDirection.z + 0.15, 2) * 40
		//* dot(float2(0.5, 0.5), pos.xy)
	, 0, 20) * Exposure * 20;
	return ss ;
}
*/