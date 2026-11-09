// Source: https://blog.selfshadow.com/publications/s2025-shading-course/pdi/s2025_pbs_pdi_slides.pdf
// HLSL conversion

//
// Sample implementation of the GT7 Tone Mapping operator.
//
// Version history:
// 1.0    (2025-08-10)    Initial release.
//
// -----
// MIT License
//
// Copyright (c) 2025 Polyphony Digital Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// -----------------------------------------------------------------------------
// Mode options.
// -----------------------------------------------------------------------------
#define TONE_MAPPING_UCS        0 // 0: ICTCP | 1: JZAZBZ

// -----------------------------------------------------------------------------
// Defines the SDR reference white level used in our tone mapping (typically 250 nits).
// -----------------------------------------------------------------------------
#define GRAN_TURISMO_SDR_PAPER_WHITE 250.0f // cd/m^2

// -----------------------------------------------------------------------------
// Gran Turismo luminance-scale conversion helpers.
// In Gran Turismo, 1.0f in the linear frame-buffer space corresponds to
// REFERENCE_LUMINANCE cd/m^2 of physical luminance (typically 100 cd/m^2).
// -----------------------------------------------------------------------------
#define REFERENCE_LUMINANCE 100.0f // cd/m^2 <-> 1.0f

float
frameBufferValueToPhysicalValue(float fbValue)
{
    // Converts linear frame-buffer value to physical luminance (cd/m^2)
    // where 1.0 corresponds to REFERENCE_LUMINANCE (e.g., 100 cd/m^2).
    return fbValue * REFERENCE_LUMINANCE;
}

float
physicalValueToFrameBufferValue(float physical)
{
    // Converts physical luminance (cd/m^2) to a linear frame-buffer value,
    // where 1.0 corresponds to REFERENCE_LUMINANCE (e.g., 100 cd/m^2).
    return physical / REFERENCE_LUMINANCE;
}

// -----------------------------------------------------------------------------
// Utility functions.
// -----------------------------------------------------------------------------
float
smoothStep(float x, float edge0, float edge1)
{
    float t = (x - edge0) / (edge1 - edge0);

    if (x < edge0)
    {
        return 0.0f;
    }
    if (x > edge1)
    {
        return 1.0f;
    }

    return t * t * (3.0f - 2.0f * t);
}

float
chromaCurve(float x, float a, float b)
{
    return 1.0f - smoothStep(x, a, b);
}

// -----------------------------------------------------------------------------
// "GT Tone Mapping" curve with convergent shoulder.
// -----------------------------------------------------------------------------
struct GTToneMappingCurveV2
{
    float peakIntensity_;
    float alpha_;
    float midPoint_;
    float linearSection_;
    float toeStrength_;
    float kA_, kB_, kC_;

    void initializeCurve(float monitorIntensity,
                         float alpha,
                         float grayPoint,
                         float linearSection,
                         float toeStrength)
    {
        peakIntensity_ = monitorIntensity;
        alpha_         = alpha;
        midPoint_      = grayPoint;
        linearSection_ = linearSection;
        toeStrength_   = toeStrength;

        // Pre-compute constants for the shoulder region.
        float k = (linearSection_ - 1.0f) / (alpha_ - 1.0f);
        kA_     = peakIntensity_ * linearSection_ + peakIntensity_ * k;
        kB_     = -peakIntensity_ * k * exp(linearSection_ / k);
        kC_     = -1.0f / (k * peakIntensity_);
    }

    float evaluateCurve(float x)
    {
        if (x < 0.0f)
        {
            return 0.0f;
        }

        float weightLinear = smoothStep(x, 0.0f, midPoint_);
        float weightToe    = 1.0f - weightLinear;

        // Shoulder mapping for highlights.
        float shoulder = kA_ + kB_ * exp(x * kC_);

        if (x < linearSection_ * peakIntensity_)
        {
            float toeMapped = midPoint_ * pow(x / midPoint_, toeStrength_);
            return weightToe * toeMapped + weightLinear * x;
        }
        else
        {
            return shoulder;
        }
    }
};

// -----------------------------------------------------------------------------
// EOTF / inverse-EOTF for ST-2084 (PQ).
// Note: Introduce exponentScaleFactor to allow scaling of the exponent in the EOTF for Jzazbz.
// -----------------------------------------------------------------------------
float
eotfSt2084(float n, float exponentScaleFactor = 1.0f)
{
    if (n < 0.0f)
    {
        n = 0.0f;
    }
    if (n > 1.0f)
    {
        n = 1.0f;
    }

    // Base functions from SMPTE ST 2084:2014
    // Converts from normalized PQ (0-1) to absolute luminance in cd/m^2 (linear light)
    // Assumes float input; does not handle integer encoding (Annex)
    // Assumes full-range signal (0-1)
    const float m1  = 0.1593017578125f;                // (2610 / 4096) / 4
    const float m2  = 78.84375f * exponentScaleFactor; // (2523 / 4096) * 128
    const float c1  = 0.8359375f;                      // 3424 / 4096
    const float c2  = 18.8515625f;                     // (2413 / 4096) * 32
    const float c3  = 18.6875f;                        // (2392 / 4096) * 32
    const float pqC = 10000.0f;                        // Maximum luminance supported by PQ (cd/m^2)

    // Does not handle signal range from 2084 - assumes full range (0-1)
    float np = pow(n, 1.0f / m2);
    float l  = np - c1;

    if (l < 0.0f)
    {
        l = 0.0f;
    }

    l = l / (c2 - c3 * np);
    l = pow(l, 1.0f / m1);

    // Convert absolute luminance (cd/m^2) into the frame-buffer linear scale.
    return physicalValueToFrameBufferValue(l * pqC);
}

float
inverseEotfSt2084(float v, float exponentScaleFactor = 1.0f)
{
    const float m1  = 0.1593017578125f;
    const float m2  = 78.84375f * exponentScaleFactor;
    const float c1  = 0.8359375f;
    const float c2  = 18.8515625f;
    const float c3  = 18.6875f;
    const float pqC = 10000.0f;

    // Convert the frame-buffer linear scale into absolute luminance (cd/m^2).
    float physical = frameBufferValueToPhysicalValue(v);
    float y        = physical / pqC; // Normalize for the ST-2084 curve

    float ym = pow(y, m1);
    return exp2(m2 * (log2(c1 + c2 * ym) - log2(1.0f + c3 * ym)));
}

// -----------------------------------------------------------------------------
// ICtCp conversion.
// Reference: ITU-T T.302 (https://www.itu.int/rec/T-REC-T.302/en)
// -----------------------------------------------------------------------------
float3
rgbToICtCp(float3 rgb) // Input: linear Rec.2020
{
    float l = (rgb.x * 1688.0f + rgb.y * 2146.0f + rgb.z * 262.0f) / 4096.0f;
    float m = (rgb.x * 683.0f + rgb.y * 2951.0f + rgb.z * 462.0f) / 4096.0f;
    float s = (rgb.x * 99.0f + rgb.y * 309.0f + rgb.z * 3688.0f) / 4096.0f;

    float lPQ = inverseEotfSt2084(l);
    float mPQ = inverseEotfSt2084(m);
    float sPQ = inverseEotfSt2084(s);

    float3 ictCp;
    ictCp.x = (2048.0f * lPQ + 2048.0f * mPQ) / 4096.0f;
    ictCp.y = (6610.0f * lPQ - 13613.0f * mPQ + 7003.0f * sPQ) / 4096.0f;
    ictCp.z = (17933.0f * lPQ - 17390.0f * mPQ - 543.0f * sPQ) / 4096.0f;
    
    return ictCp;
}

float3
iCtCpToRgb(float3 ictCp) // Output: linear Rec.2020
{
    float l = ictCp.x + 0.00860904f * ictCp.y + 0.11103f * ictCp.z;
    float m = ictCp.x - 0.00860904f * ictCp.y - 0.11103f * ictCp.z;
    float s = ictCp.x + 0.560031f * ictCp.y - 0.320627f * ictCp.z;

    float lLin = eotfSt2084(l);
    float mLin = eotfSt2084(m);
    float sLin = eotfSt2084(s);

    float3 rgb;
    rgb.x = max(3.43661f * lLin - 2.50645f * mLin + 0.0698454f * sLin, 0.0f);
    rgb.y = max(-0.79133f * lLin + 1.9836f * mLin - 0.192271f * sLin, 0.0f);
    rgb.z = max(-0.0259499f * lLin - 0.0989137f * mLin + 1.12486f * sLin, 0.0f);
    
    return rgb;
}

// -----------------------------------------------------------------------------
// Jzazbz conversion.
// Reference:
// Muhammad Safdar, Guihua Cui, Youn Jin Kim, and Ming Ronnier Luo,
// "Perceptually uniform color space for image signals including high dynamic
// range and wide gamut," Opt. Express 25, 15131-15151 (2017)
// Note: Coefficients adjusted for linear Rec.2020
// -----------------------------------------------------------------------------
#define JZAZBZ_EXPONENT_SCALE_FACTOR 1.7f // Scale factor for exponent

float3
rgbToJzazbz(float3 rgb) // Input: linear Rec.2020
{
    float l = rgb.x * 0.530004f + rgb.y * 0.355704f + rgb.z * 0.086090f;
    float m = rgb.x * 0.289388f + rgb.y * 0.525395f + rgb.z * 0.157481f;
    float s = rgb.x * 0.091098f + rgb.y * 0.147588f + rgb.z * 0.734234f;

    float lPQ = inverseEotfSt2084(l, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float mPQ = inverseEotfSt2084(m, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float sPQ = inverseEotfSt2084(s, JZAZBZ_EXPONENT_SCALE_FACTOR);

    float iz = 0.5f * lPQ + 0.5f * mPQ;

    float3 jab;
    jab.x = (0.44f * iz) / (1.0f - 0.56f * iz) - 1.6295499532821566e-11f;
    jab.y = 3.524000f * lPQ - 4.066708f * mPQ + 0.542708f * sPQ;
    jab.z = 0.199076f * lPQ + 1.096799f * mPQ - 1.295875f * sPQ;
    
    return jab;
}

float3
jzazbzToRgb(float3 jab) // Output: linear Rec.2020
{
    float jz = jab.x + 1.6295499532821566e-11f;
    float iz = jz / (0.44f + 0.56f * jz);
    float a  = jab.y;
    float b  = jab.z;

    float l = iz + a * 1.386050432715393e-1f + b * 5.804731615611869e-2f;
    float m = iz + a * -1.386050432715393e-1f + b * -5.804731615611869e-2f;
    float s = iz + a * -9.601924202631895e-2f + b * -8.118918960560390e-1f;

    float lLin = eotfSt2084(l, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float mLin = eotfSt2084(m, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float sLin = eotfSt2084(s, JZAZBZ_EXPONENT_SCALE_FACTOR);

    float3 rgb;
    rgb.x = lLin * 2.990669f + mLin * -2.049742f + sLin * 0.088977f;
    rgb.y = lLin * -1.634525f + mLin * 3.145627f + sLin * -0.483037f;
    rgb.z = lLin * -0.042505f + mLin * -0.377983f + sLin * 1.448019f;
    
    return rgb;
}

// -----------------------------------------------------------------------------
// Unified color space (UCS): ICtCp or Jzazbz.
// -----------------------------------------------------------------------------
#if TONE_MAPPING_UCS == 0
float3
rgbToUcs(float3 rgb)
{
    return rgbToICtCp(rgb);
}
float3
ucsToRgb(float3 ucs)
{
    return iCtCpToRgb(ucs);
}
#elif TONE_MAPPING_UCS == 1
float3
rgbToUcs(float3 rgb)
{
    return rgbToJzazbz(rgb);
}
float3
ucsToRgb(float3 ucs)
{
    return jzazbzToRgb(ucs);
}
#endif

// -----------------------------------------------------------------------------
// GT7 Tone Mapping class.
// -----------------------------------------------------------------------------
struct GT7ToneMapping
{
    float sdrCorrectionFactor_;

    float framebufferLuminanceTarget_;
    float framebufferLuminanceTargetUcs_; // Target luminance in UCS space
    GTToneMappingCurveV2 curve_;

    float blendRatio_;
    float fadeStart_;
    float fadeEnd_;

    // Initializes the tone mapping curve and related parameters based on the target display luminance.
    // This method should not be called directly. Use initializeAsHDR() or initializeAsSDR() instead.
    void initializeParameters(float physicalTargetLuminance)
    {
        framebufferLuminanceTarget_ = physicalValueToFrameBufferValue(physicalTargetLuminance);

        // Initialize the curve (slightly different parameters from GT Sport).
        curve_.initializeCurve(framebufferLuminanceTarget_, 0.25f, 0.538f, 0.444f, 1.280f);

        // Default parameters.
        blendRatio_ = 0.6f;
        fadeStart_  = 0.98f;
        fadeEnd_    = 1.16f;

        float3 rgb = float3(framebufferLuminanceTarget_,
                            framebufferLuminanceTarget_,
                            framebufferLuminanceTarget_);
        float3 ucs = rgbToUcs(rgb);
        framebufferLuminanceTargetUcs_ =
            ucs.x; // Use the first UCS component (I or Jz) as luminance
    }

    // Initialize for HDR (High Dynamic Range) display.
    // Input: target display peak luminance in nits (range: 250 to 10,000)
    // Note: The lower limit is 250 because the parameters for GTToneMappingCurveV2
    //       were determined based on an SDR paper white assumption of 250 nits (GRAN_TURISMO_SDR_PAPER_WHITE).
    void initializeAsHDR(float physicalTargetLuminance)
    {
        sdrCorrectionFactor_ = 1.0f;
        initializeParameters(physicalTargetLuminance);
    }

    // Initialize for SDR (Standard Dynamic Range) display.
    void initializeAsSDR()
    {
        // Regarding SDR output:
        // First, in GT (Gran Turismo), it is assumed that a maximum value of 1.0 in SDR output
        // corresponds to GRAN_TURISMO_SDR_PAPER_WHITE (typically 250 nits).
        // Therefore, tone mapping for SDR output is performed based on GRAN_TURISMO_SDR_PAPER_WHITE.
        // However, in the sRGB standard, 1.0f corresponds to 100 nits,
        // so we need to "undo" the tone-mapped values accordingly.
        // To match the sRGB range, the tone-mapped values are scaled using sdrCorrectionFactor_.
        //
        // * These adjustments ensure that the visual appearance (in terms of brightness)
        //   stays generally consistent across both HDR and SDR outputs for the same rendered content.
        sdrCorrectionFactor_ = 1.0f / physicalValueToFrameBufferValue(GRAN_TURISMO_SDR_PAPER_WHITE);
        initializeParameters(GRAN_TURISMO_SDR_PAPER_WHITE);
    }

    // Input:  linear Rec.2020 RGB (frame buffer values)
    // Output: tone-mapped RGB (frame buffer values);
    //         - in SDR mode: mapped to [0, 1], ready for sRGB OETF
    //         - in HDR mode: mapped to [0, framebufferLuminanceTarget_], ready for PQ inverse-EOTF
    // Note: framebufferLuminanceTarget_ represents the display's target peak luminance converted to a frame buffer value.
    //       The returned values are suitable for applying the appropriate OETF to generate final output signal.
    float3 applyToneMapping(float3 rgb)
    {
        // Convert to UCS to separate luminance and chroma.
        float3 ucs = rgbToUcs(rgb);

        // Per-channel tone mapping ("skewed" color).
        float3 skewedRgb = float3(curve_.evaluateCurve(rgb.x),
                                  curve_.evaluateCurve(rgb.y),
                                  curve_.evaluateCurve(rgb.z));

        float3 skewedUcs = rgbToUcs(skewedRgb);

        float chromaScale = chromaCurve(ucs.x / framebufferLuminanceTargetUcs_, fadeStart_, fadeEnd_);

        float3 scaledUcs = float3(skewedUcs.x,         // Luminance from skewed color
                                  ucs.y * chromaScale, // Scaled chroma components
                                  ucs.z * chromaScale);

        // Convert back to RGB.
        float3 scaledRgb = ucsToRgb(scaledUcs);

        // Final blend between per-channel and UCS-scaled results.
        {
            float3 blended = (1.0f - blendRatio_) * skewedRgb + blendRatio_ * scaledRgb;
            // When using SDR, apply the correction factor.
            // When using HDR, sdrCorrectionFactor_ is 1.0f, so it has no effect.
            return sdrCorrectionFactor_ * min(blended, framebufferLuminanceTarget_);
        }
    }
};

static const float3x3 Rec709ToXYZ = float3x3(
    0.4123908,  0.3575843,  0.1804808,
    0.2126390,  0.7151687,  0.0721923,
    0.0193308,  0.1191948,  0.9505322
);

static const float3x3 XYZToRec709 = float3x3(
    3.2409699, -1.5373832, -0.4986108,
    -0.9692436,  1.8759675,  0.0415551,
    0.0556301, -0.2039770,  1.0569715
);

static const float3x3 Rec2020ToXYZ = float3x3(
    0.6369580,  0.1446169,  0.1688810,
    0.2627002,  0.6779981,  0.0593017,
    0.0000000,  0.0280727,  1.0609851
);

static const float3x3 XYZToRec2020 = float3x3(
    1.7166512, -0.3556708, -0.2533663,
    -0.6666844,  1.6164812,  0.0157685,
    0.0176399, -0.0427706,  0.9421031
);

static const float3x3 Rec709ToRec2020 = mul(XYZToRec2020, Rec709ToXYZ);
static const float3x3 Rec2020ToRec709 = mul(XYZToRec709, Rec2020ToXYZ);

float3 GT7TonemapWrapper(float3 color)
{
    GT7ToneMapping toneMapper;
    toneMapper.initializeAsSDR();
    
    color = mul(Rec709ToRec2020, color);
    color = toneMapper.applyToneMapping(color);
    color = mul(Rec2020ToRec709, color);
    
    return color;
}
