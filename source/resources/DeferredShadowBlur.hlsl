sampler GBufferTextureSampler0 : register(ps, s0);
sampler GBufferTextureSampler1 : register(ps, s1);
sampler GBufferTextureSampler2 : register(ps, s2);
sampler GBufferTextureSampler3 : register(ps, s4);
sampler GBufferStencilTextureSampler : register(ps, s5);
sampler ParabSampler : register(ps, s6);
// sampler ParabSampler : register(ps, s5);
sampler ShadowSampler : register(ps, s14);
sampler gShadowZSamplerDir : register(ps, s15);
float4 dReflectionParams : register(c72);
float4 gDeferredProjParams : register(c66);
float4 gDirectionalColour : register(c18);
float4 gDirectionalLight : register(c17);
float4 gFacetCentre : register(c54);
float4 gLightAmbient0 : register(c37);
float4 gLightAmbient1 : register(c38);
float4x4 gShadowMatrix : register(c60);
float4 gShadowParam0123 : register(c57);
float4 gShadowParam14151617 : register(c56);
float4 gShadowParam18192021 : register(c53);
float4 gShadowParam4567 : register(c58);
float4 gShadowParam891113 : register(c59);
float4x4 gViewInverse : register(c12);
float4 globalScalars : register(c39);

float4 BilateralDepthTreshold : register(c140);

struct PS_IN {
	float2 texCoords : TEXCOORD;
	float4 texCoords2 : TEXCOORD1;
};

#define NUM_SAMPLE_1 2
#define NUM_SAMPLE_2 2
#define NUM_SAMPLE_3 2

static float4 UVOffsets_horizontal [] = {
	float4(-3.0/1280,0.0,0.0,0), 
	float4(-2.0/1280,0.0,0.0,0), 
	float4(-1.0/1280,0.0,0.0,0), 
	float4(0.0/1280,0.0,0.0,0), 
	float4(1.0/1280,0.0,0.0,0), 
	float4(2.0/1280,0.0,0.0,0), 
	float4(3.0/1280,0.0,0.0,0)
};


float4 MainPS( in PS_IN input){
	float2 refTap = tex2D( ShadowSampler, input.texCoords.xy ).xy;

    const float blurThreshold = BilateralDepthTreshold * refTap.y;

	float blurredValue = 0;

	for( int i = 0; i < 7; ++i )	{
		float2 tap = tex2D( ShadowSampler, float4(input.texCoords.xy + UVOffsets_horizontal[ i ].xy, 0.5, 1) ).xy;
		float weight = 1.0/7;

		blurredValue += weight * ( ( abs( refTap.y - tap.y ) < blurThreshold ) ? tap.x : refTap.x );
	}

	return float4( blurredValue, refTap.y, 1, 1);  // Output to G16R16F
}


float4 BlurHorizontal(PS_IN input) : COLOR{
	float2 refTap = tex2D( ShadowSampler, input.texCoords.xy ).xy;

    const float blurThreshold = BilateralDepthTreshold;

	float blurredValue = 0;

	for( int i = 0; i < 7; ++i )	{
		float2 tap = tex2D( ShadowSampler, float4(input.texCoords.xy + UVOffsets_horizontal[ i ].xy, 0.5, 1) ).xy;
		float weight = 1.0/7;

		blurredValue += weight * ( ( abs( refTap.y - tap.y ) < blurThreshold ) ? tap.x : refTap.x );
	}

	return float4( blurredValue, refTap.y, 1, 1);  // Output to G16R16F
}

float4 BlurVertical(PS_IN input) : COLOR{
	float2 refTap = tex2D( ShadowSampler, input.texCoords.xy ).xy;

    const float blurThreshold = BilateralDepthTreshold;

	float blurredValue = 0;

	for( int i = 0; i < 7; ++i )	{
		float2 tap = tex2D( ShadowSampler, float4(input.texCoords.xy + UVOffsets_horizontal[ i ].yx, 0.5, 1) ).xy;
		float weight = 1.0/7;

		blurredValue += weight * ( ( abs( refTap.y - tap.y ) < blurThreshold ) ? tap.x : refTap.x );
	}

	return float4( blurredValue, refTap.y, 1, 1);  // Output to G16R16F
}

float4 BlurOmini(PS_IN input) : COLOR{
	return MainPS(input  );
}
