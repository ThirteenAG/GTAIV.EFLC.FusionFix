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
