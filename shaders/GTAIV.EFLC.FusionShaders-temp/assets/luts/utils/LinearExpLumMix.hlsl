// This looks pretty faithful to untonemapped vanilla

float3 CustomTMO(float3 color)
{
	float3 lumconstants = float3(0.2126f, 0.7152f, 0.0722f);
	float lum = dot(lumconstants, color);

    // can be precalculated
	float threshold = 0.2f;
	float C1 = -1.44269504089 / (1.0f - threshold); // log2(e)
	float C2 = -C1 * threshold + log2(1.0f - threshold);

	float4 shoulder = 1.0f - exp2(float4(color, lum) * C1 + C2);

	float4 tonemap = lerp(float4(color, lum), shoulder, step(threshold, float4(color, lum)));
	
	color = lerp(tonemap.w / lum * color, tonemap.xyz, tonemap.w);

	return color;
}
