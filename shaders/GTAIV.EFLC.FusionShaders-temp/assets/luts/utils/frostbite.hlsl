// Source: https://www.ea.com/frostbite/news/high-dynamic-range-color-grading-and-display-in-frostbite

// Note: this function is expensive.
// This is why we bake it down into a LUT (ideally the same LUT we use for grading, to make it free).
// This is the hue-preserving version used for screenshots in this article, it’s very ad-hoc but hopefully interesting to play with.

// Aplies exponential ("Photographic") luma compression
float rangeCompress(float x)
{
    return 1.0 - exp(-x);
}

float rangeCompress(float val, float threshold)
{
    float v1 = val;
    float v2 = threshold + (1 - threshold) * rangeCompress((val - threshold) / (1 - threshold));
    return val < threshold ? v1 : v2;
}

float3 rangeCompress(float3 val, float threshold)
{
    return float3(
        rangeCompress(val.x, threshold),
        rangeCompress(val.y, threshold),
        rangeCompress(val.z, threshold));
}


static const float PQ_constant_N = (2610.0 / 4096.0 / 4.0);
static const float PQ_constant_M = (2523.0 / 4096.0 * 128.0);
static const float PQ_constant_C1 = (3424.0 / 4096.0);
static const float PQ_constant_C2 = (2413.0 / 4096.0 * 32.0);
static const float PQ_constant_C3 = (2392.0 / 4096.0 * 32.0);

// PQ (Perceptual Quantiser; ST.2084) encode/decode used for HDR TV and grading
float3 linearToPQ(float3 linearCol, const float maxPqValue)
{
    linearCol /= maxPqValue;

    float3 colToPow = pow(linearCol, PQ_constant_N);
    float3 numerator = PQ_constant_C1 + PQ_constant_C2*colToPow;
    float3 denominator = 1.0 + PQ_constant_C3*colToPow;
    float3 pq = pow(numerator / denominator, PQ_constant_M);

    return pq;
}

float3 PQtoLinear(float3 linearCol, const float maxPqValue)
{
    float3 colToPow = pow(linearCol, 1.0 / PQ_constant_M);
    float3 numerator = max(colToPow - PQ_constant_C1, 0.0);
    float3 denominator = PQ_constant_C2 - (PQ_constant_C3 * colToPow);
    float3 linearColor = pow(numerator / denominator, 1.0 / PQ_constant_N);

    linearColor *= maxPqValue;

    return linearColor;
}


// RGB with sRGB/Rec.709 primaries to CIE XYZ
float3 RGBToXYZ(float3 c)
{
    float3x3 mat = float3x3(
        0.4124564,  0.3575761,  0.1804375,
        0.2126729,  0.7151522,  0.0721750,
        0.0193339,  0.1191920,  0.9503041
    );
    return mul(mat, c);
}
float3 XYZToRGB(float3 c)
{
    float3x3 mat = float3x3(
         3.24045483602140870, -1.53713885010257510, -0.49853154686848090,
        -0.96926638987565370,  1.87601092884249100,  0.04155608234667354,
         0.05564341960421366, -0.20402585426769815,  1.05722516245792870
    );
    return mul(mat, c);
}
// Converts XYZ tristimulus values into cone responses for the three types of cones in the human visual system, matching long, medium, and short wavelengths.
// Note that there are many LMS color spaces; this one follows the ICtCp color space specification.
float3 XYZToLMS(float3 c)
{
    float3x3 mat = float3x3(
        0.3592,  0.6976, -0.0358,
        -0.1922, 1.1004,  0.0755,
        0.0070,  0.0749,  0.8434
    ); 
    return mul(mat, c);
}
float3 LMSToXYZ(float3 c)
{
    float3x3 mat = float3x3(
         2.07018005669561320, -1.32645687610302100,  0.206616006847855170,
         0.36498825003265756,  0.68046736285223520, -0.045421753075853236,
        -0.04959554223893212, -0.04942116118675749,  1.187995941732803400
    );
    return mul(mat, c);
}


// RGB with sRGB/Rec.709 primaries to ICtCp
float3 RGBToICtCp(float3 col)
{
    col = RGBToXYZ(col);
    col = XYZToLMS(col);
    // 1.0f = 100 nits, 100.0f = 10k nits
    col = linearToPQ(max(0.0.xxx, col), 100.0);

    // Convert PQ-LMS into ICtCp. Note that the "S" channel is not used,
    // but overlap between the cone responses for long, medium, and short wavelengths
    // ensures that the corresponding part of the spectrum contributes to luminance.

    float3x3 mat = float3x3(
        0.5000,  0.5000,  0.0000,
        1.6137, -3.3234,  1.7097,
        4.3780, -4.2455, -0.1325
    );

    return mul(mat, col);
}

float3 ICtCpToRGB(float3 col)
{
    float3x3 mat = float3x3(
        1.0,  0.00860514569398152,  0.11103560447547328,
        1.0, -0.00860514569398152, -0.11103560447547328,
        1.0,  0.56004885956263900, -0.32063747023212210
    );
    col = mul(mat, col);

    // 1.0f = 100 nits, 100.0f = 10k nits
    col = PQtoLinear(col, 100.0);
    col = LMSToXYZ(col);
    return XYZToRGB(col);
}


float3 applyHuePreservingShoulder(float3 col)
{
    float3 ictcp = RGBToICtCp(col);

    // Hue-preserving range compression requires desaturation in order to achieve a natural look. We adaptively desaturate the input based on its luminance.
    float saturationAmount = pow(smoothstep(1.0, 0.3, ictcp.x), 1.3);
    col = ICtCpToRGB(ictcp * float3(1, saturationAmount.xx));

    // Only compress luminance starting at a certain point. Dimmer inputs are passed through without modification.
    float linearSegmentEnd = 0.25;

    // Hue-preserving mapping
    float maxCol = max(col.x, max(col.y, col.z));
    float mappedMax = rangeCompress(maxCol, linearSegmentEnd);
    float3 compressedHuePreserving = col * mappedMax / maxCol;

    // Non-hue preserving mapping
    float3 perChannelCompressed = rangeCompress(col, linearSegmentEnd);

    // Combine hue-preserving and non-hue-preserving colors. Absolute hue preservation looks unnatural, as bright colors *appear* to have been hue shifted.
    // Actually doing some amount of hue shifting looks more pleasing
    col = lerp(perChannelCompressed, compressedHuePreserving, 0.6);

    float3 ictcpMapped = RGBToICtCp(col);

    // Smoothly ramp off saturation as brightness increases, but keep some even for very bright input
    float postCompressionSaturationBoost = 0.3 * smoothstep(1.0, 0.5, ictcp.x);

    // Re-introduce some hue from the pre-compression color. Something similar could be accomplished by delaying the luma-dependent desaturation before range compression.
    // Doing it here however does a better job of preserving perceptual luminance of highly saturated colors. Because in the hue-preserving path we only range-compress the max channel,
    // saturated colors lose luminance. By desaturating them more aggressively first, compressing, and then re-adding some saturation, we can preserve their brightness to a greater extent.
    ictcpMapped.yz = lerp(ictcpMapped.yz, ictcp.yz * ictcpMapped.x / max(1e-3, ictcp.x), postCompressionSaturationBoost);

    col = ICtCpToRGB(ictcpMapped);

    return col;
}
