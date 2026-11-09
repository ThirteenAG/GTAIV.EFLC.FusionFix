// HDR output
//
// The back buffer is 16-bit float. Everything is composited into it exactly like in SDR: the game's
// post processing writes gamma encoded color, which is no longer clipped at 1.0, and the interface is
// drawn on top of it. This pass runs last and turns that into scRGB for the display: 1.0 in gamma space
// becomes paper white, highlights above it roll off towards the peak brightness of the display.

sampler2D Composite : register(s0);

float4 gOutput : register(c0); // x: paper white / 80 nits, y: peak / paper white, z: roll-off start (fraction of the peak), w: HDR enabled

float4 PS_HDROutput(float2 uv : TEXCOORD0) : COLOR0
{
    float3 color = max(tex2Dlod(Composite, float4(uv, 0.0, 0.0)).rgb, 0.0);

    if (gOutput.w <= 0.0)
        return float4(saturate(color), 1.0);

    // Relative to paper white
    float3 linearColor = pow(color, 2.2);

    float peak = gOutput.y;
    float start = min(peak * gOutput.z, 1.0);
    float brightest = max(linearColor.r, max(linearColor.g, linearColor.b));
    if (brightest > start)
    {
        float range = max(peak - start, 0.0001);
        float mapped = start + range * (1.0 - exp(-(brightest - start) / range));
        linearColor *= mapped / brightest;
    }

    return float4(linearColor * gOutput.x, 1.0);
}

// Calibration pattern of the HDR page of the menu, drawn over the frame after the output pass in absolute scRGB
// (1.0 = 80 nits), so nothing of it passes through the roll-off. A card with a field and the Fusion Fix logo (the
// alpha of the texture) on it: card, field and logo each have one brightness.

sampler2D Logo : register(s0);

float4 gCalibration : register(c0);     // x: card, y: field, z: logo, in scRGB
float4 gFieldRect : register(c1);       // the field in the card, left top right bottom (0..1)
float4 gLogoRect : register(c2);        // the logo in the card, left top right bottom (0..1)

float4 PS_HDRCalibration(float2 uv : TEXCOORD0) : COLOR0
{
    bool inField = all(uv >= gFieldRect.xy) && all(uv <= gFieldRect.zw);
    float value = inField ? gCalibration.y : gCalibration.x;

    // Sampled everywhere for the mip level, the address mode clamps
    float2 logoUV = (uv - gLogoRect.xy) / (gLogoRect.zw - gLogoRect.xy);
    float logo = tex2D(Logo, logoUV).a * (all(logoUV >= 0.0) && all(logoUV <= 1.0) ? 1.0 : 0.0);
    value = lerp(value, gCalibration.z, logo);

    return float4(value.xxx, 1.0);
}
