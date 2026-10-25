// Object motion vectors of temporal anti-aliasing (the camera's motion, the resolve and the rest are the shared module's,
// FusionFixUtils aa/shaders).
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
