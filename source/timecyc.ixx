module;

#include <common.hxx>

export module timecyc;

import common;
import settings;
import comvars;
import natives;
import timecycext;
import seasonal;

float fVolFogFarClip = 4500.0f;

int scanfCount = 0;
int timecyc_scanf(const char* i, const char* fmt, int* mAmbient0ColorR, int* mAmbient0ColorG, int* mAmbient0ColorB, int* mAmbient1ColorR, int* mAmbient1ColorG, int* mAmbient1ColorB,
    int* mDirLightColorR, int* mDirLightColorG, int* mDirLightColorB, int* unusedParam, int* mFilmGrain, int* mSkyBottomColorFogDensityA,
    int* mSkyBottomColorFogDensityR, int* mSkyBottomColorFogDensityG, int* mSkyBottomColorFogDensityB, int* mSunCoreR, int* mSunCoreG, int* mSunCoreB,
    int* unusedParam1, int* unusedParam2, int* unusedParam3, float* unusedParam4, float* mCoronaBrightness, float* mFarClip, float* mFogStart,
    int* mLowCloudsColorR, int* mLowCloudsColorG, int* mLowCloudsColorB, int* mBottomCloudsColorR, int* mBottomCloudsColorG, int* mBottomCloudsColorB,
    int* mWaterR, int* mWaterG, int* mWaterB, int* mWaterA, float* mExposure, float* mBloomThreshold, float* mMidGrayValue, float* mBloomIntensity,
    int* mColorCorrectionR, int* mColorCorrectionG, int* mColorCorrectionB, int* mColorAddR, int* mColorAddG, int* mColorAddB, float* mDesaturation,
    float* mContrast, float* mGamma, float* mDesaturationFar, float* mContrastFar, float* mGammaFar, float* mDepthFxNear, float* mDepthFxFar,
    float* mLumMin, float* mLumMax, float* mLumDelay, int* mCloudAlpha, float* mDirLightMultiplier, float* mAmbient0Multiplier, float* mAmbient1Multiplier,
    float* mSkyLightMultiplier, float* mDirLightSpecMultiplier, int* mTemperature, float* mGlobalReflectionMultiplier, float* mSkyColorR, float* mSkyColorG,
    float* mSkyColorB, float* mSkyHorizonColorR, float* mSkyHorizonColorG, float* mSkyHorizonColorB, float* mSkyEastHorizonColorR, float* mSkyEastHorizonColorG,
    float* mSkyEastHorizonColorB, float* mCloud1ColorR, float* mCloud1ColorG, float* mCloud1ColorB, float* mUnknown3, float* mSkyHorizonHeight, float* mSkyHorizonBrightness,
    float* unusedParam5, float* unusedParam6, float* mCloud2ColorR, float* mCloud2ColorG, float* mCloud2ColorB, float* mCloud2ShadowStrength, float* mCloud2Threshold,
    float* mCloud2Bias1, float* mCloud2Scale, float* unusedParam7, float* mCloud2Bias2, float* mDetailNoiseScale, float* mDetailNoiseMultiplier, float* mCloud2Offset,
    float* unusedParam8, float* mCloudsFadeOut, float* mCloud1Bias, float* mCloud1Detail, float* mCloud1Threshold, float* mCloud1Height, float* mCloud3ColorR,
    float* mCloud3ColorG, float* mCloud3ColorB, float* mUnknown29, float* mSunColorR, float* mSunColorG, float* mSunColorB, float* mCloudsBrightness,
    float* mDetailNoiseOffset, float* mStarsBrightness, float* mVisibleStars, float* mMoonBrightness, float* mMoonColorR, float* mMoonColorG,
    float* mMoonColorB, float* mMoonGlow, float* mMoonParam3, float* unusedParam9, float* unusedParam10, float* mSunSize, float* mUnknown46, float* mDOFStart,
    float* unusedParam11, float* unusedParam12, float* mNearDOFBlur, float* mFarDOFBlur, float* mWaterReflectionMultiplier, float* mParticleBrightness,
    float* mCoronaSize, float* mSkyBrightness, float* mAOStrength, float* mRimLightingMultiplier, float* mDistantCoronaBrightness, float* mDistantCoronaSize,
    float* mPedAOStrength)
{
    if (!i)
        return 0;

    // TODO: Maybe to seasonal at some point?
    {
        static auto getDatFile = [](const std::string& fileName) {
            std::vector<std::string> dat;
            static const auto filePath = GetModulePath(GetModuleHandleW(NULL)).parent_path() / "pc" / "data";
            std::ifstream ifstr(filePath / fileName);
            std::string line;
            while (std::getline(ifstr, line))
            {
                if (line.find_first_not_of(" \t\r\n") != std::string::npos && !line.contains('/'))
                    dat.emplace_back(line);
            }
            return dat;
        };

        static const auto snowTC = getDatFile("snow.dat");
        static const auto hallTC = getDatFile("halloween.dat");

        switch (SeasonalManager::GetCurrent()) {
            case SeasonalType::Snow:
            {
                if (snowTC.size() == 99)
                    i = snowTC[scanfCount].c_str();
                break;
            }
            case SeasonalType::Halloween:
            {
                if (hallTC.size() == 99)
                    i = hallTC[scanfCount].c_str();
                break;
            }
        }
    }

    auto res = sscanf(i, fmt, mAmbient0ColorR, mAmbient0ColorG, mAmbient0ColorB, mAmbient1ColorR, mAmbient1ColorG, mAmbient1ColorB, mDirLightColorR,
        mDirLightColorG, mDirLightColorB, unusedParam, mFilmGrain, mSkyBottomColorFogDensityA, mSkyBottomColorFogDensityR, mSkyBottomColorFogDensityG,
        mSkyBottomColorFogDensityB, mSunCoreR, mSunCoreG, mSunCoreB, unusedParam1, unusedParam2, unusedParam3, unusedParam4, mCoronaBrightness, mFarClip,
        mFogStart, mLowCloudsColorR, mLowCloudsColorG, mLowCloudsColorB, mBottomCloudsColorR, mBottomCloudsColorG, mBottomCloudsColorB, mWaterR, mWaterG,
        mWaterB, mWaterA, mExposure, mBloomThreshold, mMidGrayValue, mBloomIntensity, mColorCorrectionR, mColorCorrectionG, mColorCorrectionB, mColorAddR,
        mColorAddG, mColorAddB, mDesaturation, mContrast, mGamma, mDesaturationFar, mContrastFar, mGammaFar, mDepthFxNear, mDepthFxFar, mLumMin, mLumMax,
        mLumDelay, mCloudAlpha, mDirLightMultiplier, mAmbient0Multiplier, mAmbient1Multiplier, mSkyLightMultiplier, mDirLightSpecMultiplier, mTemperature,
        mGlobalReflectionMultiplier, mSkyColorR, mSkyColorG, mSkyColorB, mSkyHorizonColorR, mSkyHorizonColorG, mSkyHorizonColorB, mSkyEastHorizonColorR,
        mSkyEastHorizonColorG, mSkyEastHorizonColorB, mCloud1ColorR, mCloud1ColorG, mCloud1ColorB, mUnknown3, mSkyHorizonHeight, mSkyHorizonBrightness,
        unusedParam5, unusedParam6, mCloud2ColorR, mCloud2ColorG, mCloud2ColorB, mCloud2ShadowStrength, mCloud2Threshold, mCloud2Bias1, mCloud2Scale,
        unusedParam7, mCloud2Bias2, mDetailNoiseScale, mDetailNoiseMultiplier, mCloud2Offset, unusedParam8, mCloudsFadeOut, mCloud1Bias, mCloud1Detail,
        mCloud1Threshold, mCloud1Height, mCloud3ColorR, mCloud3ColorG, mCloud3ColorB, mUnknown29, mSunColorR, mSunColorG, mSunColorB, mCloudsBrightness,
        mDetailNoiseOffset, mStarsBrightness, mVisibleStars, mMoonBrightness, mMoonColorR, mMoonColorG, mMoonColorB, mMoonGlow, mMoonParam3, unusedParam9,
        unusedParam10, mSunSize, mUnknown46, mDOFStart, unusedParam11, unusedParam12, mNearDOFBlur, mFarDOFBlur, mWaterReflectionMultiplier, mParticleBrightness,
        mCoronaSize, mSkyBrightness, mAOStrength, mRimLightingMultiplier, mDistantCoronaBrightness, mDistantCoronaSize, mPedAOStrength);

    if (!FusionFixSettings("PREF_BLOOM"))
        *mBloomIntensity = 0.0f;

    if (FusionFixSettings("PREF_VOLUMETRICFOG"))
        *mFarClip = fVolFogFarClip;

    switch (FusionFixSettings("PREF_TCYC_DOF"))
    {
    case FusionFixSettings.DofText.eOff:
    case FusionFixSettings.DofText.eCutscenesOnly:
        *mNearDOFBlur *= 0.0f;
        *mFarDOFBlur *= 0.0f;
        break;
    case FusionFixSettings.DofText.eLow:
        *mNearDOFBlur *= 0.7f;
        *mFarDOFBlur *= 0.7f;
        break;
    case FusionFixSettings.DofText.eMedium:
        *mNearDOFBlur *= 0.8f;
        *mFarDOFBlur *= 0.8f;
        break;
    case FusionFixSettings.DofText.eHigh:
        *mNearDOFBlur *= 0.9f;
        *mFarDOFBlur *= 0.9f;
        break;
    case FusionFixSettings.DofText.eVeryHigh:
        [[fallthrough]];
    default:
        break;
    }

    {
        static int IV2TLAD[3][100] =
        {
            { 96,101,92,130,158,161,156,143,140,138,107,128,128,101,122,143,125,133,122,122,133,128,119,119,110,115,138,133,138,148,133,133,119,155,155,78,98,135,130,130,138,98,98,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 146,142,136,111,133,117,129,104,104,108,133,128,128,130,110,104,109,106,110,110,96,128,140,140,120,101,127,122,125,132,96,96,140,181,181,100,108,127,124,118,119,108,108,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 156,148,138,83,92,74,73,54,56,69,138,128,128,109,94,69,77,68,94,94,66,128,144,144,102,64,90,82,80,75,66,66,144,186,186,105,110,88,95,86,85,110,110,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int IV2TBOGT[3][100] =
        {
            { 107,103,86,120,130,115,130,138,138,138,107,128,128,130,122,109,109,94,122,94,133,128,119,119,108,122,116,138,138,138,133,133,119,155,155,78,98,107,107,107,138,115,115,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 133,130,116,105,120,122,120,108,108,108,133,128,128,101,110,120,120,111,110,111,103,128,140,140,120,110,140,131,131,131,96,96,140,181,181,100,108,124,124,126,124,126,126,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 138,134,117,84,98,113,98,69,69,69,138,128,128,125,94,122,122,122,94,122,101,128,144,144,99,94,128,106,106,106,66,66,144,186,186,105,110,128,128,128,98,128,128,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int IV2OFF[3][100] =
        {
            { 125,123,107,103,116,117,116,105,105,105,125,128,128,113,109,117,117,108,109,109,98,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,105,105,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
            { 125,123,107,103,116,117,116,105,105,105,125,128,128,113,109,117,117,108,109,109,98,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,105,105,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
            { 125,123,107,103,116,117,116,105,105,105,125,128,128,113,109,117,117,108,109,109,98,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,105,105,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
        };

        static int TLAD2IV[3][100] =
        {
            { 107,103,86,120,130,115,130,138,138,138,107,128,128,101,122,109,125,122,122,122,133,128,119,119,108,122,116,138,138,138,133,133,119,155,155,78,98,107,107,138,98,98,98,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 133,130,116,105,120,122,120,108,108,108,133,128,128,130,110,120,110,110,110,110,96,128,140,140,120,110,140,131,131,131,96,96,140,181,181,100,108,124,126,124,108,108,108,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 138,134,117,84,98,113,98,69,69,69,138,128,128,109,94,122,90,94,94,94,66,128,144,144,99,94,128,106,106,106,66,66,144,186,186,105,110,128,128,98,110,110,110,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int TLAD2TBOGT[3][100] =
        {
            { 107,103,86,120,130,115,130,122,122,138,107,128,128,130,122,109,94,94,94,94,133,128,119,119,108,122,116,138,138,138,133,133,119,155,155,78,98,107,107,138,115,115,115,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 133,130,116,105,120,122,120,110,110,108,133,128,128,101,110,120,111,111,103,103,103,128,140,140,120,110,140,131,131,131,96,96,140,181,181,100,108,124,126,124,126,126,126,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 138,134,117,84,98,113,98,94,94,69,138,128,128,125,94,122,122,122,122,122,101,128,144,144,99,94,128,106,106,106,66,66,144,186,186,105,110,128,128,98,128,128,128,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int TLAD2OFF[3][100] =
        {
            { 133,130,122,108,128,117,119,100,100,105,129,128,128,113,109,105,104,102,109,109,98,128,134,134,111,93,118,112,114,118,98,98,134,174,174,94,105,117,116,111,114,105,105,174,110,110,79,117,131,139,139,108,93,111,111,116,116,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,118,117,101,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,130,109,109,128 },
            { 133,130,122,108,128,117,119,100,100,105,129,128,128,113,109,105,104,102,109,109,98,128,134,134,111,93,118,112,114,118,98,98,134,174,174,94,105,117,116,111,114,105,105,174,110,110,79,117,131,139,139,108,93,111,111,116,116,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,118,117,101,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,130,109,109,128 },
            { 133,130,122,108,128,117,119,100,100,105,129,128,128,113,109,105,104,102,109,109,98,128,134,134,111,93,118,112,114,118,98,98,134,174,174,94,105,117,116,111,114,105,105,174,110,110,79,117,131,139,139,108,93,111,111,116,116,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,118,117,101,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,130,109,109,128 },
        };

        static int TBOGT2IV[3][100] =
        {
            { 107,103,86,120,130,115,130,138,122,138,107,128,128,101,122,109,109,125,122,122,133,128,119,119,108,122,116,138,138,138,133,133,119,155,155,78,98,107,107,107,138,115,115,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 133,130,116,105,120,122,120,108,110,108,133,128,128,130,110,120,120,110,110,110,96,128,140,140,120,110,140,131,131,131,96,96,140,181,181,100,108,124,124,126,124,126,126,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 138,134,117,84,98,113,98,69,94,69,138,128,128,109,94,122,122,90,94,94,66,128,144,144,99,94,128,106,106,106,66,66,144,186,186,105,110,128,128,128,98,128,128,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int TBOGT2TLAD[3][100] =
        {
            { 96,101,92,130,158,161,156,143,140,138,90,128,128,101,122,143,125,133,122,122,133,128,119,119,110,115,138,133,138,148,133,133,119,155,155,78,98,135,130,130,138,98,98,155,99,99,89,104,136,133,133,104,102,91,91,91,91,104,104,121,121,121,121,117,117,117,96,96,96,109,117,117,109,117,99,107,107,104,104,130,92,99,104,104,124,104,104,104,99,119,122,128,120,128,115,118,122,122,128 },
            { 146,142,136,111,133,117,129,104,104,108,143,128,128,130,110,104,109,106,110,110,96,128,140,140,120,101,127,122,125,132,96,96,140,181,181,100,108,127,124,118,119,108,108,181,112,112,88,117,138,143,143,120,97,125,125,125,125,131,131,133,133,133,142,130,130,130,111,111,111,121,117,117,122,117,105,120,120,120,120,120,112,117,117,117,130,124,120,120,112,141,110,128,114,128,122,140,110,110,128 },
            { 156,148,138,83,92,74,73,54,56,69,153,128,128,109,94,69,77,68,94,94,66,128,144,144,102,64,90,82,80,75,66,66,144,186,186,105,110,88,95,86,85,110,110,186,120,120,60,130,118,142,142,99,80,116,116,116,116,140,140,143,143,143,143,125,125,125,120,120,120,122,117,117,122,117,97,103,103,99,99,104,120,130,130,130,104,130,99,99,120,145,94,128,90,128,108,134,94,94,128 },
        };

        static int TBOGT2OFF[3][100] =
        {
            { 126,122,106,103,116,117,116,105,109,105,126,128,128,119,109,117,117,109,109,106,112,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,123,123,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
            { 126,122,106,103,116,117,116,105,109,105,126,128,128,119,109,117,117,109,109,106,112,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,123,123,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
            { 126,122,106,103,116,117,116,105,109,105,126,128,128,119,109,117,117,109,109,106,112,128,134,134,109,109,128,125,125,125,98,98,134,174,174,94,105,120,120,120,120,123,123,174,110,110,79,117,131,139,139,108,93,111,111,111,111,125,125,132,132,132,135,124,124,124,109,109,109,117,117,117,117,117,100,110,110,108,108,118,108,115,117,117,119,119,108,108,110,135,109,128,108,128,115,131,109,109,128 },
        };

        switch (FusionFixSettings("PREF_TIMECYC"))
        {
        case FusionFixSettings.TimecycText.eOFF:
        {
            if (*_dwCurrentEpisode == 0)
            {
                *mColorCorrectionR = IV2OFF[0][scanfCount];
                *mColorCorrectionG = IV2OFF[1][scanfCount];
                *mColorCorrectionB = IV2OFF[2][scanfCount];
            }
            else if (*_dwCurrentEpisode == 1)
            {
                *mColorCorrectionR = TLAD2OFF[0][scanfCount];
                *mColorCorrectionG = TLAD2OFF[1][scanfCount];
                *mColorCorrectionB = TLAD2OFF[2][scanfCount];
            }
            else if (*_dwCurrentEpisode == 2)
            {
                *mColorCorrectionR = TBOGT2OFF[0][scanfCount];
                *mColorCorrectionG = TBOGT2OFF[1][scanfCount];
                *mColorCorrectionB = TBOGT2OFF[2][scanfCount];
            }
        }
        break;
        case FusionFixSettings.TimecycText.eIV:
        {
            if (*_dwCurrentEpisode == 1)
            {
                *mColorCorrectionR = TLAD2IV[0][scanfCount];
                *mColorCorrectionG = TLAD2IV[1][scanfCount];
                *mColorCorrectionB = TLAD2IV[2][scanfCount];
            }
            else if (*_dwCurrentEpisode == 2)
            {
                *mColorCorrectionR = TBOGT2IV[0][scanfCount];
                *mColorCorrectionG = TBOGT2IV[1][scanfCount];
                *mColorCorrectionB = TBOGT2IV[2][scanfCount];
            }
        }
        break;
        case FusionFixSettings.TimecycText.eTLAD:
        {
            if (*_dwCurrentEpisode == 0)
            {
                *mColorCorrectionR = IV2TLAD[0][scanfCount];
                *mColorCorrectionG = IV2TLAD[1][scanfCount];
                *mColorCorrectionB = IV2TLAD[2][scanfCount];
            }
            else if (*_dwCurrentEpisode == 2)
            {
                *mColorCorrectionR = TBOGT2TLAD[0][scanfCount];
                *mColorCorrectionG = TBOGT2TLAD[1][scanfCount];
                *mColorCorrectionB = TBOGT2TLAD[2][scanfCount];
            }
        }
        break;
        case FusionFixSettings.TimecycText.eTBOGT:
        {
            if (*_dwCurrentEpisode == 0)
            {
                *mColorCorrectionR = IV2TBOGT[0][scanfCount];
                *mColorCorrectionG = IV2TBOGT[1][scanfCount];
                *mColorCorrectionB = IV2TBOGT[2][scanfCount];
            }
            else if (*_dwCurrentEpisode == 1)
            {
                *mColorCorrectionR = TLAD2TBOGT[0][scanfCount];
                *mColorCorrectionG = TLAD2TBOGT[1][scanfCount];
                *mColorCorrectionB = TLAD2TBOGT[2][scanfCount];
            }
        }
        break;
        default:
            break;
        }
    }

    {
        int h = scanfCount % NUMHOURS;
        int w = scanfCount / NUMHOURS;

        CTimeCycleExt::tmp_fDirLightColorR[h][w] = (float)(*mDirLightColorR) / 255.0f;
        CTimeCycleExt::tmp_fDirLightColorG[h][w] = (float)(*mDirLightColorG) / 255.0f;
        CTimeCycleExt::tmp_fDirLightColorB[h][w] = (float)(*mDirLightColorB) / 255.0f;
        CTimeCycleExt::tmp_fDirLightMultiplier[h][w] = *mDirLightMultiplier;
    }

    scanfCount++;

    if (scanfCount >= 99)
        scanfCount = 0;

    return res;
}

int cutsc_scanf(const char* i, const char* fmt, int* keyframe, float* farclip, float* nearclip)
{
    auto res = sscanf(i, fmt, keyframe, farclip, nearclip);

    if (FusionFixSettings("PREF_VOLUMETRICFOG"))
    {
        *farclip = 0.0f;
        //*nearclip = -1.0f;
    }

    return res;
}

int timecyclemodifiers_scanf(const char* i, const char* fmt, char* Name, float* MinFarClip, float* MaxFarClip, float* MinFogStart,
    float* MaxFogStart, int* AmbientLight0R, int* AmbientLight0G, int* AmbientLight0B, int* AmbientLight0A, float* AmbientLight0M,
    int* AmbientLight1R, int* AmbientLight1G, int* AmbientLight1B, int* AmbientLight1A, float* AmbientLight1M, int* DirectionalLightR,
    int* DirectionalLightG, int* DirectionalLightB, int* DirectionalLightA, float* DirectionalLightM, float* AmbientDownMult, int* FogR,
    int* FogG, int* FogB, int* FogA, int* NearFogR, int* NearFogG, int* NearFogB, int* NearFogA, float* FogMult, int* NearFog, float* Axis,
    float* Temperature, float* PostFxStrength, float* Exposure, float* ExposureMultiplier, float* BrightPassThreshold, float* MidGreyValue,
    int* IntensityBloomR, int* IntensityBloomG, int* IntensityBloomB, int* ColourCorrectR, int* ColourCorrectG, int* ColourCorrectB, float* ColourAdd,
    float* Desaturation, float* Contrast, float* Gamma, float* DesaturationFar, float* ContrastFar, float* GammaFar, float* DepthFxNear, float* DepthFxFar,
    float* LumMin, float* LumMax, float* LumAdaptDelay, float* GlobEnvRefl, float* MinFarDOF, float* MaxFarDOF, float* MinNearDOF, float* MaxNearDOF,
    float* NearBlurDOF, float* MidBlurDOF, float* FarBlurDOF, float* WaterReflection, float* ParticleHDR, float* AmbientOcclusionMult, float* SkyMultiplier,
    float* PedAmbOccHack, int* NearFogAmount)
{
    auto res = sscanf(i, fmt, Name, MinFarClip, MaxFarClip, MinFogStart, MaxFogStart, AmbientLight0R, AmbientLight0G, AmbientLight0B, AmbientLight0A,
        AmbientLight0M, AmbientLight1R, AmbientLight1G, AmbientLight1B, AmbientLight1A, AmbientLight1M, DirectionalLightR, DirectionalLightG, DirectionalLightB,
        DirectionalLightA, DirectionalLightM, AmbientDownMult, FogR, FogG, FogB, FogA, NearFogR, NearFogG, NearFogB, NearFogA, FogMult, NearFog, Axis, Temperature,
        PostFxStrength, Exposure, ExposureMultiplier, BrightPassThreshold, MidGreyValue, IntensityBloomR, IntensityBloomG, IntensityBloomB, ColourCorrectR,
        ColourCorrectG, ColourCorrectB, ColourAdd, Desaturation, Contrast, Gamma, DesaturationFar, ContrastFar, GammaFar, DepthFxNear, DepthFxFar, LumMin, LumMax,
        LumAdaptDelay, GlobEnvRefl, MinFarDOF, MaxFarDOF, MinNearDOF, MaxNearDOF, NearBlurDOF, MidBlurDOF, FarBlurDOF, WaterReflection, ParticleHDR, AmbientOcclusionMult,
        SkyMultiplier, PedAmbOccHack, NearFogAmount);

    if (std::string_view(Name) == "Police" || std::string_view(Name) == "intro")
        return res;

    if (FusionFixSettings("PREF_VOLUMETRICFOG"))
    {
        *MinFarClip = -1.0f;
        *MaxFarClip = -1.0f;
    }
    //else // handled in shaders module now
    //{
    //    // Workaround for phone screen depth issues.
    //    //if (*MinFarClip != -1.0f && *MinFarClip < 205.0f)
    //    //    *MinFarClip = 205.0f;
    //    //
    //    //if (*MaxFarClip != -1.0f && *MaxFarClip < 205.0f)
    //    //    *MaxFarClip = 205.0f;
    //}

    return res;
}

class Timecyc
{
public:
    Timecyc()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            fVolFogFarClip = iniReader.ReadFloat("FOG", "VolFogFarClip", 4500.0f);

            FusionFixSettings.SetCallback("PREF_TIMECYC", [](int32_t value) {
                CTimeCycle::Initialise();
                bMenuNeedsUpdate = 200;
            });

            // Make timecyc changes visible in menu
            auto pattern = hook::pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 0F 85 ? ? ? ? E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 11 04 24");
            if (!pattern.empty())
            {
                static auto byte_1173590 = *pattern.get_first<uint8_t*>(2);
                struct MenuTimecycHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        *(uint8_t*)&regs.eax |= *byte_1173590;
                        if (bMenuNeedsUpdate > 0) {
                            *(uint8_t*)&regs.eax = 0;
                            bMenuNeedsUpdate--;
                        }
                    }
                }; injector::MakeInline<MenuTimecycHook>(pattern.get_first(0), pattern.get_first(6));
                pattern = hook::pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 74 20");
                injector::MakeInline<MenuTimecycHook>(pattern.get_first(0), pattern.get_first(6));
                pattern = hook::pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 74 12");
                injector::MakeInline<MenuTimecycHook>(pattern.get_first(0), pattern.get_first(6));
            }
            else
            {
                static injector::hook_back<int(*)()> hbsub_4B18F0;
                static auto MenuTimecycHook = []() -> int
                {
                    if (bMenuNeedsUpdate > 0) {
                        bMenuNeedsUpdate--;
                        return 0;
                    }
                    return hbsub_4B18F0.fun();
                };

                pattern = hook::pattern("E8 ? ? ? ? 84 C0 5F 0F 85");
                hbsub_4B18F0.fun = injector::MakeCALL(pattern.get_first(), static_cast<int(*)()>(MenuTimecycHook), true).get();

                pattern = hook::pattern("E8 ? ? ? ? 84 C0 74 ? A1 ? ? ? ? 69 C0");
                hbsub_4B18F0.fun = injector::MakeCALL(pattern.get_first(), static_cast<int(*)()>(MenuTimecycHook), true).get();

                static injector::hook_back<char(*)()> hbsub_8AC010;
                static auto MenuTimecycHook2 = []() -> char {
                    if (bMenuNeedsUpdate > 0)
                    {
                        bMenuNeedsUpdate--;
                        return 0;
                    }
                    return hbsub_8AC010.fun();
                };

                pattern = hook::pattern("E8 ? ? ? ? 84 C0 74 ? 38 1D ? ? ? ? 75 ? B3");
                hbsub_8AC010.fun = injector::MakeCALL(pattern.get_first(), static_cast<char(*)()>(MenuTimecycHook2), true).get();

                pattern = hook::pattern("80 3D ? ? ? ? ? 74 ? 84 DB 74 ? 8B 06");
                injector::MakeNOP(pattern.get_first(0), 9, true);
            }

            pattern = find_pattern("E8 ? ? ? ? 0F B6 8C 24 ? ? ? ? 0F B6 84 24", "E8 ? ? ? ? 0F B6 84 24 ? ? ? ? 0F B6 8C 24");
            injector::MakeCALL(pattern.get_first(0), timecyc_scanf, true);

            FusionFixSettings.SetCallback("PREF_TCYC_DOF", [](int32_t value) {
                CTimeCycle::Initialise();
                bMenuNeedsUpdate = 200;
            });

            FusionFixSettings.SetCallback("PREF_BLOOM", [](int32_t value) {
                CTimeCycle::Initialise();
                bMenuNeedsUpdate = 200;
            });

            FusionFixSettings.SetCallback("PREF_SHADOW_QUALITY", [](int32_t value) {
                bMenuNeedsUpdate = 200;
            });

            FusionFixSettings.SetCallback("PREF_VOLUMETRICFOG", [](int32_t value) {
                CTimeCycle::Initialise();
                CTimeCycle::InitialiseModifiers();
                bMenuNeedsUpdate = 200;
            });

            // z-fighting fix helpers
            {
                auto pattern = hook::pattern("E8 ? ? ? ? 6A 0C E8 ? ? ? ? 8B 0D");
                injector::MakeCALL(pattern.get_first(0), cutsc_scanf, true);

                pattern = hook::pattern("E8 ? ? ? ? 69 F6 ? ? ? ? 8D 84 24");
                injector::MakeCALL(pattern.get_first(0), timecyclemodifiers_scanf, true);
            }
        };
    }
} Timecyc;