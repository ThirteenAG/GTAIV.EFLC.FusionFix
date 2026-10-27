sampler2D Scene : register(s0);
sampler2D Motion : register(s1);
sampler2D Depth : register(s2);
sampler2D History : register(s3);
float4 Pixel : register(c0);
float4 Temporal : register(c1);
float4 Camera : register(c2);
row_major float4x4 PreviousClip : register(c7);
float4 Jitter : register(c11);

// _STENCIL_BUFFER_ holds the stencil in r and the motion in gb; the sky has none and follows the camera
float2 ScreenMotion(float2 uv)
{
    float2 closest = uv;
    float depth = tex2D(Depth, uv).r;
    const float2 corners[4] = { float2(-1.0, -1.0), float2(1.0, -1.0), float2(-1.0, 1.0), float2(1.0, 1.0) };
    [unroll] for (int i = 0; i < 4; ++i)
    {
        float2 offset = corners[i] * Pixel.xy;
        float d = tex2D(Depth, uv + offset).r;
        if (d < depth)
        {
            depth = d;
            closest = uv + offset;
        }
    }
    if (depth < 1.0)
        return tex2D(Motion, closest).gb;
    float2 screenUV = uv - Pixel.xy * 0.5;
    float4 previous = mul(float4(screenUV * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0), PreviousClip);
    return screenUV - Jitter.xy - (previous.xy / previous.w * float2(0.5, -0.5) + 0.5);
}

float4 CopyDepth(float2 uv : TEXCOORD0) : COLOR0
{
    float distance = Camera.x * exp2(tex2D(Depth, uv).r * log2(Camera.y / Camera.x));
    return saturate(Camera.y / (Camera.y - Camera.x) * (1.0 - Camera.x / max(distance, Camera.x)));
}

float3 ToYCoCg(float3 color)
{
    return float3(dot(color, float3(0.25, 0.5, 0.25)), color.r - color.b, color.g - (color.r + color.b) * 0.5);
}

float3 FromYCoCg(float3 color)
{
    return float3(color.x + color.y * 0.5 - color.z * 0.5, color.x + color.z * 0.5, color.x - color.y * 0.5 - color.z * 0.5);
}

// Catmull-Rom with five bilinear taps: keeps the history sharp when it moves
float3 SampleHistory(float2 uv)
{
    float2 size = 1.0 / Pixel.xy;
    float2 position = uv * size;
    float2 center = floor(position - 0.5) + 0.5;
    float2 f = position - center;
    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-0.5 + 0.5 * f);
    float2 w12 = w1 + w2;
    float2 uv0 = (center - 1.0) * Pixel.xy;
    float2 uv3 = (center + 2.0) * Pixel.xy;
    float2 uv12 = (center + w2 / w12) * Pixel.xy;
    float3 color = tex2D(History, float2(uv12.x, uv0.y)).rgb * w12.x * w0.y;
    color += tex2D(History, float2(uv0.x, uv12.y)).rgb * w0.x * w12.y;
    color += tex2D(History, uv12).rgb * w12.x * w12.y;
    color += tex2D(History, float2(uv3.x, uv12.y)).rgb * w3.x * w12.y;
    color += tex2D(History, float2(uv12.x, uv3.y)).rgb * w12.x * w3.y;
    float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
    return max(color / weight, 0.0);
}

float4 Resolve(float2 uv : TEXCOORD0) : COLOR0
{
    float3 current = max(tex2D(Scene, uv).rgb, 0.0);
    float2 velocity = ScreenMotion(uv);
    float2 historyUV = uv - velocity;
    if (Temporal.w == 0.0 || any(historyUV < 0.0) || any(historyUV > 1.0))
        return float4(current, 1.0);
    float3 mean = 0.0;
    float3 square = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            float3 color = ToYCoCg(max(tex2D(Scene, uv + float2(x, y) * Pixel.xy).rgb, 0.0));
            mean += color;
            square += color * color;
        }
    }
    mean /= 9.0;
    float3 sigma = sqrt(max(square / 9.0 - mean * mean, 0.0)) * Temporal.z;
    float3 history = ToYCoCg(SampleHistory(historyUV));
    float3 offset = history - mean;
    float3 extent = abs(offset) / max(sigma, 0.0001);
    history = mean + offset / max(1.0, max(extent.x, max(extent.y, extent.z)));
    float weight = lerp(Temporal.x, Temporal.y, saturate(length(velocity / Pixel.xy) * Camera.z));
    return float4(max(FromYCoCg(lerp(history, ToYCoCg(current), weight)), 0.0), 1.0);
}

float4 Copy(float2 uv : TEXCOORD0) : COLOR0
{
    return float4(tex2D(Scene, uv).rgb, 1.0);
}

float4 BridgeMotion(float2 uv : TEXCOORD0) : COLOR0
{
    return float4(-ScreenMotion(uv), 0.0, 0.0);
}
