// Temporal anti-aliasing shaders: object motion vectors, camera motion vectors and the resolve.
//
// Motion vectors are stored as (previous - current) in texture coordinates, jitter excluded:
// the history of a pixel is fetched at uv + motion.

// ---------------------------------------------------------------------------------------------
// Object motion vectors
//
// Moving entities are drawn a second time after the G-buffer pass, with the vertex buffers and
// bones they were drawn with, into the motion vector target. The clip position is computed
// exactly as the game shaders do it, so the logarithmic depth written here matches the depth
// buffer and the depth test keeps only the visible surface of the object.

row_major float4x4 gWorld         : register(c0);
row_major float4x4 gPrevWorld     : register(c4);
row_major float4x4 gWorldViewProj : register(c8);
row_major float4x4 gViewProj      : register(c12); // current, without jitter
row_major float4x4 gPrevViewProj  : register(c16); // previous, without jitter
float4 gBoneParams                : register(c20); // x: bone row v, y: 1 / bone texture width, z: previous bones present
float4 gBoneMtx[144]              : register(c64);

sampler2D PrevBoneTex : register(s0); // vertex texture: 3 texels per bone, one row per draw

struct VelocityOut
{
    float4 Position : POSITION;
    float3 Current  : TEXCOORD0;
    float3 Previous : TEXCOORD1;
    float  ClipW    : TEXCOORD2;
};

float4 GameClipPosition(float3 p)
{
    // Same instruction order as the game shaders: mul c9, mad c8, mad c10, add c11
    float4 clip = p.y * gWorldViewProj[1];
    clip = p.x * gWorldViewProj[0] + clip;
    clip = p.z * gWorldViewProj[2] + clip;
    return clip + gWorldViewProj[3];
}

VelocityOut MakeVelocityOut(float3 current, float3 previous)
{
    VelocityOut o;
    o.Position = GameClipPosition(current);
    o.ClipW = o.Position.w;

    float4 currentWorld = mul(float4(current, 1.0), gWorld);
    float4 previousWorld = mul(float4(previous, 1.0), gPrevWorld);
    o.Current = mul(float4(currentWorld.xyz, 1.0), gViewProj).xyw;
    o.Previous = mul(float4(previousWorld.xyz, 1.0), gPrevViewProj).xyw;
    return o;
}

VelocityOut VS_VelocityRigid(float4 position : POSITION)
{
    return MakeVelocityOut(position.xyz, position.xyz);
}

float4 PrevBone(float index, float row)
{
    float u = (index + row + 0.5) * gBoneParams.y;
    return tex2Dlod(PrevBoneTex, float4(u, gBoneParams.x, 0.0, 0.0));
}

VelocityOut VS_VelocitySkinned(float4 position : POSITION, float4 weights : BLENDWEIGHT, float4 indices : BLENDINDICES)
{
    float4 idx = indices * 765.005859;

    // Blend order matches the game shaders: y, x, z, w
    float4 r1 = weights.y * gBoneMtx[idx.y];
    float4 r2 = weights.y * gBoneMtx[idx.y + 1];
    float4 r3 = weights.y * gBoneMtx[idx.y + 2];
    r1 = gBoneMtx[idx.x] * weights.x + r1;
    r2 = gBoneMtx[idx.x + 1] * weights.x + r2;
    r3 = gBoneMtx[idx.x + 2] * weights.x + r3;
    r1 = gBoneMtx[idx.z] * weights.z + r1;
    r2 = gBoneMtx[idx.z + 1] * weights.z + r2;
    r3 = gBoneMtx[idx.z + 2] * weights.z + r3;
    r1 = gBoneMtx[idx.w] * weights.w + r1;
    r2 = gBoneMtx[idx.w + 1] * weights.w + r2;
    r3 = gBoneMtx[idx.w + 2] * weights.w + r3;

    float4 p = float4(position.xyz, 1.0);
    float3 current = float3(dot(p, r1), dot(p, r2), dot(p, r3));
    float3 previous = current;

    if (gBoneParams.z > 0.0)
    {
        float4 q1 = weights.x * PrevBone(idx.x, 0.0) + weights.y * PrevBone(idx.y, 0.0) + weights.z * PrevBone(idx.z, 0.0) + weights.w * PrevBone(idx.w, 0.0);
        float4 q2 = weights.x * PrevBone(idx.x, 1.0) + weights.y * PrevBone(idx.y, 1.0) + weights.z * PrevBone(idx.z, 1.0) + weights.w * PrevBone(idx.w, 1.0);
        float4 q3 = weights.x * PrevBone(idx.x, 2.0) + weights.y * PrevBone(idx.y, 2.0) + weights.z * PrevBone(idx.z, 2.0) + weights.w * PrevBone(idx.w, 2.0);
        previous = float3(dot(p, q1), dot(p, q2), dot(p, q3));
    }

    return MakeVelocityOut(current, previous);
}

float4 gDepthParams : register(c0); // x: 1 / near, y: 1 / log2(far / near), z: depth bias

float4 PS_Velocity(VelocityOut i, out float depth : DEPTH) : COLOR0
{
    depth = log2(i.ClipW * gDepthParams.x) * gDepthParams.y - gDepthParams.z;
    float2 current = i.Current.xy / i.Current.z;
    float2 previous = i.Previous.xy / i.Previous.z;
    return float4((previous - current) * float2(0.5, -0.5), 0.0, 1.0);
}

// ---------------------------------------------------------------------------------------------
// Previous bone upload: one point per texel

struct BoneWriteVertex
{
    float4 Position : POSITION;
    float4 Value    : TEXCOORD0;
};

struct BoneWriteOut
{
    float4 Position : POSITION;
    float4 Value    : TEXCOORD0;
    float  Size     : PSIZE;
};

BoneWriteOut VS_BoneWrite(BoneWriteVertex i)
{
    BoneWriteOut o;
    o.Position = i.Position;
    o.Value = i.Value;
    o.Size = 1.0;
    return o;
}

float4 PS_BoneWrite(float4 value : TEXCOORD0) : COLOR0
{
    return value;
}

// ---------------------------------------------------------------------------------------------
// Camera motion vectors, reprojected from the depth buffer

sampler2D SceneDepth : register(s0);

float4 gProjection : register(c0); // P00, P11, P20 and P21 of the (jittered) projection
float4 gJitter     : register(c1); // xy: jitter in NDC, z: near, w: log2(far / near)
row_major float4x4 gReproject : register(c2); // current view space -> previous clip space, jitter excluded

float4 PS_CameraMotion(float2 uv : TEXCOORD0) : COLOR0
{
    float z = tex2Dlod(SceneDepth, float4(uv, 0.0, 0.0)).r;
    float distance = gJitter.z * exp2(z * gJitter.w);

    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float3 view = float3(distance * (ndc.x + gProjection.z) / gProjection.x, distance * (ndc.y + gProjection.w) / gProjection.y, -distance);

    float4 previous = mul(float4(view, 1.0), gReproject);
    float2 current = ndc + gJitter.xy;
    return float4((previous.xy / previous.w - current) * float2(0.5, -0.5), 0.0, 1.0);
}

// ---------------------------------------------------------------------------------------------
// Depth for the resolve, DLSS and FSR: the standard [0, 1] depth of a perspective projection, from the
// logarithmic one, taken like the motion vectors before transparent geometry is drawn

float4 gDepthConvert : register(c0); // x: near, y: log2(far / near), z: far

float4 PS_UpscalerDepth(float2 uv : TEXCOORD0) : COLOR0
{
    float z = tex2Dlod(SceneDepth, float4(uv, 0.0, 0.0)).r;
    float distance = gDepthConvert.x * exp2(z * gDepthConvert.y);
    float depth = gDepthConvert.z * (distance - gDepthConvert.x) / (distance * (gDepthConvert.z - gDepthConvert.x));
    return depth.xxxx;
}

// ---------------------------------------------------------------------------------------------
// Reactive mask: particles, glass and other transparent geometry have no motion vectors of their own.
// Their pixels are found by comparing the final scene with a copy taken right after the fog pass, before
// transparent geometry and visual effects are drawn, and follow the current frame more.

sampler2D SceneTex  : register(s0);
sampler2D OpaqueTex : register(s1); // luminance of the scene before transparent geometry

float4 gReactive : register(c0); // x: scale, y: maximum

float Luma(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float4 PS_OpaqueLuma(float2 uv : TEXCOORD0) : COLOR0
{
    return Luma(tex2Dlod(SceneTex, float4(uv, 0.0, 0.0)).rgb).xxxx;
}

float ReactiveValue(float scene, float opaque, float4 params)
{
    // Compared after a tonemap, a bright particle on a dark background counts as much as a dark one on a
    // bright background
    scene = max(scene, 0.0);
    opaque = max(opaque, 0.0);
    float difference = abs(scene / (1.0 + scene) - opaque / (1.0 + opaque));
    return saturate(difference * params.x) * params.y;
}

float4 PS_Reactive(float2 uv : TEXCOORD0) : COLOR0
{
    float scene = Luma(tex2Dlod(SceneTex, float4(uv, 0.0, 0.0)).rgb);
    float opaque = tex2Dlod(OpaqueTex, float4(uv, 0.0, 0.0)).r;
    return ReactiveValue(scene, opaque, gReactive).xxxx;
}

// ---------------------------------------------------------------------------------------------
// Resolve

sampler2D CurrentTex  : register(s0);
sampler2D HistoryTex  : register(s1);
sampler2D MotionTex   : register(s2);
sampler2D DepthTex    : register(s3); // standard [0, 1] depth, before transparent geometry
sampler2D OpaqueLumaTex : register(s4); // see the reactive mask

float4 gTexel   : register(c0); // 1 / width, 1 / height, width, height
float4 gTaa     : register(c1); // xy: jitter in pixels, z: history valid, w: variance clip gamma
float4 gBlend   : register(c2); // x: minimum current weight, y: maximum current weight, z: weight per pixel of motion, w: luma weight
float4 gResolveReactive : register(c3); // x: scale, y: maximum current weight, z: enabled

// Luminance weighted reversible tonemap: keeps bright samples from dominating the filters
float3 Compress(float3 c)
{
    return c / (1.0 + Luma(c) * gBlend.w);
}

float3 Expand(float3 c)
{
    return c / max(1.0 - Luma(c) * gBlend.w, 1.0 / 65504.0);
}

float3 ToYCoCg(float3 c)
{
    return float3(dot(c, float3(0.25, 0.5, 0.25)), dot(c, float3(0.5, 0.0, -0.5)), dot(c, float3(-0.25, 0.5, -0.25)));
}

float3 FromYCoCg(float3 c)
{
    return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z);
}

float3 FetchCurrent(float2 uv)
{
    float3 c = tex2Dlod(CurrentTex, float4(uv, 0.0, 0.0)).rgb;
    return ToYCoCg(Compress(clamp(c, 0.0, 65000.0)));
}

float3 FetchHistory(float2 uv)
{
    // Catmull-Rom through five bilinear taps
    float2 position = uv * gTexel.zw;
    float2 center = floor(position - 0.5) + 0.5;
    float2 f = position - center;

    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-0.5 + 0.5 * f);

    float2 w12 = w1 + w2;
    float2 tc12 = (center + w2 / w12) * gTexel.xy;
    float2 tc0 = (center - 1.0) * gTexel.xy;
    float2 tc3 = (center + 2.0) * gTexel.xy;

    float4 result = float4(tex2Dlod(HistoryTex, float4(tc12.x, tc0.y, 0.0, 0.0)).rgb, 1.0) * (w12.x * w0.y);
    result += float4(tex2Dlod(HistoryTex, float4(tc0.x, tc12.y, 0.0, 0.0)).rgb, 1.0) * (w0.x * w12.y);
    result += float4(tex2Dlod(HistoryTex, float4(tc12.x, tc12.y, 0.0, 0.0)).rgb, 1.0) * (w12.x * w12.y);
    result += float4(tex2Dlod(HistoryTex, float4(tc3.x, tc12.y, 0.0, 0.0)).rgb, 1.0) * (w3.x * w12.y);
    result += float4(tex2Dlod(HistoryTex, float4(tc12.x, tc3.y, 0.0, 0.0)).rgb, 1.0) * (w12.x * w3.y);

    return ToYCoCg(Compress(clamp(result.rgb / result.a, 0.0, 65000.0)));
}

float3 ClipTowardsCenter(float3 boxMin, float3 boxMax, float3 history)
{
    float3 center = 0.5 * (boxMax + boxMin);
    float3 extents = 0.5 * (boxMax - boxMin) + 0.00001;
    float3 offset = history - center;
    float3 units = abs(offset / extents);
    float maxUnit = max(units.x, max(units.y, units.z));
    return maxUnit > 1.0 ? center + offset / maxUnit : history;
}

struct ResolveOut
{
    float4 Color   : COLOR0;
    float4 History : COLOR1;
};

ResolveOut PS_TemporalResolve(float2 uv : TEXCOORD0)
{
    static const float2 offsets[9] =
    {
        float2(-1.0, -1.0), float2(0.0, -1.0), float2(1.0, -1.0),
        float2(-1.0,  0.0), float2(0.0,  0.0), float2(1.0,  0.0),
        float2(-1.0,  1.0), float2(0.0,  1.0), float2(1.0,  1.0)
    };

    float3 m1 = 0.0;
    float3 m2 = 0.0;
    float3 boxMin = 65504.0;
    float3 boxMax = -65504.0;
    float3 filtered = 0.0;
    float filterWeight = 0.0;

    float closestDepth = 1.0;
    float2 closestOffset = 0.0;

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 tap = uv + offsets[i] * gTexel.xy;
        float3 c = FetchCurrent(tap);

        m1 += c;
        m2 += c * c;
        boxMin = min(boxMin, c);
        boxMax = max(boxMax, c);

        // Each pixel holds the scene sampled at its center minus the jitter
        float2 d = offsets[i] - gTaa.xy;
        float w = exp(-2.29 * dot(d, d));
        filtered += c * w;
        filterWeight += w;


        float depth = tex2Dlod(DepthTex, float4(tap, 0.0, 0.0)).r;
        if (depth < closestDepth)
        {
            closestDepth = depth;
            closestOffset = offsets[i];
        }
    }

    filtered /= filterWeight;

    float2 motion = tex2Dlod(MotionTex, float4(uv + closestOffset * gTexel.xy, 0.0, 0.0)).xy;
    float2 historyUV = uv + motion;

    float3 mean = m1 / 9.0;
    float3 sigma = sqrt(max(m2 / 9.0 - mean * mean, 0.0));
    float3 clipMin = max(boxMin, mean - gTaa.w * sigma);
    float3 clipMax = min(boxMax, mean + gTaa.w * sigma);

    float3 result = filtered;
    bool onScreen = all(historyUV == saturate(historyUV));

    if (gTaa.z > 0.0 && onScreen)
    {
        float3 history = ClipTowardsCenter(clipMin, clipMax, FetchHistory(historyUV));

        float speed = length(motion * gTexel.zw);
        float currentWeight = lerp(gBlend.x, gBlend.y, saturate(speed * gBlend.z));

        if (gResolveReactive.z > 0.0)
        {
            float scene = Luma(tex2Dlod(CurrentTex, float4(uv, 0.0, 0.0)).rgb);
            float opaque = tex2Dlod(OpaqueLumaTex, float4(uv, 0.0, 0.0)).r;
            currentWeight = max(currentWeight, ReactiveValue(scene, opaque, gResolveReactive));
        }

        result = lerp(history, filtered, currentWeight);
    }

    ResolveOut o;
    o.Color = float4(Expand(FromYCoCg(result)), 1.0);
    o.History = o.Color;
    return o;
}
