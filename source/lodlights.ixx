module;

#include <common.hxx>
#include <ranges>

export module lodlights;

import common;
import comvars;
import natives;
import settings;

#define IDR_LODLIGHTS 777
#define IDR_LODLIGHTSVC 778

float fCoronaRadiusMultiplier = 1.0f;
float fCoronaAlphaMultiplier = 1.0f;
bool bSlightlyIncreaseRadiusWithDistance = true;
bool bDisableDefaultLodLights = true;

enum BlinkTypes
{
    DEFAULT,
    RANDOM_FLASHING,
    T_1S_ON_1S_OFF,
    T_2S_ON_2S_OFF,
    T_3S_ON_3S_OFF,
    T_4S_ON_4S_OFF,
    T_5S_ON_5S_OFF,
    T_6S_ON_4S_OFF
};

class CRGBA
{
public:
    BYTE r, g, b, a;

    inline CRGBA() {}

    inline CRGBA(const CRGBA& in)
        : r(in.r), g(in.g), b(in.b), a(in.a)
    {
    }

    inline CRGBA(const CRGBA& in, BYTE alpha)
        : r(in.r), g(in.g), b(in.b), a(alpha)
    {
    }

    inline CRGBA(BYTE red, BYTE green, BYTE blue, BYTE alpha = 255)
        : r(red), g(green), b(blue), a(alpha)
    {
    }

    template <typename T>
    friend CRGBA Blend(const CRGBA& One, T OneStrength, const CRGBA& Two, T TwoStrength)
    {
        T	TotalStrength = OneStrength + TwoStrength;
        return CRGBA(((One.r * OneStrength) + (Two.r * TwoStrength)) / TotalStrength,
            ((One.g * OneStrength) + (Two.g * TwoStrength)) / TotalStrength,
            ((One.b * OneStrength) + (Two.b * TwoStrength)) / TotalStrength,
            ((One.a * OneStrength) + (Two.a * TwoStrength)) / TotalStrength);
    }

    template <typename T>
    friend CRGBA Blend(const CRGBA& One, T OneStrength, const CRGBA& Two, T TwoStrength, const CRGBA& Three, T ThreeStrength)
    {
        T	TotalStrength = OneStrength + TwoStrength + ThreeStrength;
        return CRGBA(((One.r * OneStrength) + (Two.r * TwoStrength) + (Three.r * ThreeStrength)) / TotalStrength,
            ((One.g * OneStrength) + (Two.g * TwoStrength) + (Three.g * ThreeStrength)) / TotalStrength,
            ((One.b * OneStrength) + (Two.b * TwoStrength) + (Three.b * ThreeStrength)) / TotalStrength,
            ((One.a * OneStrength) + (Two.a * TwoStrength) + (Three.a * ThreeStrength)) / TotalStrength);
    }
};

class CLamppostInfo
{
public:
    CVector			vecPos;
    CRGBA			colour;
    float			fCustomSizeMult;
    int 			nNoDistance;
    int             nDrawSearchlight;
    float			fHeading;
    int				nCoronaShowMode;
    float           fObjectDrawDistance;

    CLamppostInfo(const CVector& pos, const CRGBA& col, float fCustomMult, int CoronaShowMode, int nNoDistance, int nDrawSearchlight, float heading, float ObjectDrawDistance = 0.0f)
        : vecPos(pos), colour(col), fCustomSizeMult(fCustomMult), nCoronaShowMode(CoronaShowMode), nNoDistance(nNoDistance), nDrawSearchlight(nDrawSearchlight), fHeading(heading), fObjectDrawDistance(ObjectDrawDistance)
    {
    }
};

struct WplInstance
{
    float PositionX;
    float PositionY;
    float PositionZ;
    float RotationX;
    float RotationY;
    float RotationZ;
    float RotationW;
    unsigned int ModelNameHash;
    unsigned int Unknown1;
    signed int LODIndex;
    unsigned int Unknown2;
    float Unknown3;
};

std::map<unsigned int, CLamppostInfo>		FileContent;
std::multimap<unsigned int, CLamppostInfo>  FileContentMMap;
std::vector<CLamppostInfo>                  m_Lampposts;
std::map<unsigned int, CLamppostInfo>* pFileContent = &FileContent;
std::multimap<unsigned int, CLamppostInfo>* pFileContentMMap = &FileContentMMap;
bool							            m_bCatchLamppostsNow;

static inline float GetDistance(RwV3D* v1, RwV3D* v2)
{
    RwV3D v3;
    v3.x = v2->x - v1->x;
    v3.y = v2->y - v1->y;
    v3.z = v2->z - v1->z;
    return sqrt(v3.x * v3.x + v3.y * v3.y + v3.z * v3.z);
}

static inline unsigned int PackKey(unsigned short nModel, unsigned short nIndex)
{
    return nModel << 16 | nIndex;
}

void LoadDatFile()
{
    HMODULE hm = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&LoadDatFile, &hm);
    auto hResource = FindResource(hm, CText::hasViceCityStrings() ? MAKEINTRESOURCE(IDR_LODLIGHTSVC) : MAKEINTRESOURCE(IDR_LODLIGHTS), RT_RCDATA);

    if (hResource)
    {
        auto hLoadedResource = LoadResource(hm, hResource);
        if (hLoadedResource)
        {
            auto pLockedResource = LockResource(hLoadedResource);
            if (pLockedResource)
            {
                auto dwResourceSize = SizeofResource(hm, hResource);
                if (dwResourceSize)
                {
                    unsigned short	nCurIndexForModel = 0;
                    unsigned int	nModelIV = 0xFFFFFFFF;

                    std::string line;
                    std::istringstream str((char*)pLockedResource, dwResourceSize);
                    while (std::getline(str, line))
                    {
                        if (!line.empty() && line.back() == '\r')
                        {
                            line.pop_back();
                        }

                        if (line.empty())
                            continue;

                        if (line[0] == '#')
                            continue;

                        if (line[0] == '%')
                        {
                            nCurIndexForModel = 0;
                            if (line.compare("%additional_coronas") != 0)
                            {
                                nModelIV = hashStringLowercaseFromSeed(const_cast<char*>(line.c_str() + 1), 0);
                            }
                            else
                            {
                                nModelIV = 0xFFFFFFFF;
                            }
                        }
                        else
                        {
                            float			fOffsetX, fOffsetY, fOffsetZ;
                            unsigned int	nRed, nGreen, nBlue, nAlpha;
                            float			fCustomSize = 1.0f;
                            float			fDrawDistance = 0.0f;
                            int				nNoDistance = 0;
                            int				nDrawSearchlight = 0;
                            int				nCoronaShowMode = 0;
                            if (sscanf(line.c_str(), "%3d %3d %3d %3d %f %f %f %f %f %2d %1d %1d", &nRed, &nGreen, &nBlue, &nAlpha, &fOffsetX, &fOffsetY, &fOffsetZ, &fCustomSize, &fDrawDistance, &nCoronaShowMode, &nNoDistance, &nDrawSearchlight) != 12)
                                sscanf(line.c_str(), "%3d %3d %3d %3d %f %f %f %f %2d %1d %1d", &nRed, &nGreen, &nBlue, &nAlpha, &fOffsetX, &fOffsetY, &fOffsetZ, &fCustomSize, &nCoronaShowMode, &nNoDistance, &nDrawSearchlight);
                            pFileContentMMap->insert(std::make_pair(nModelIV, CLamppostInfo(CVector(fOffsetX, fOffsetY, fOffsetZ), CRGBA(static_cast<unsigned char>(nRed), static_cast<unsigned char>(nGreen), static_cast<unsigned char>(nBlue), static_cast<unsigned char>(nAlpha)), fCustomSize, nCoronaShowMode, nNoDistance, nDrawSearchlight, 0.0f, fDrawDistance)));
                        }
                    }

                    m_bCatchLamppostsNow = true;
                }
            }
        }
    }
}

bool IsBlinkingNeeded(int BlinkType)
{
    signed int nOnDuration = 0;
    signed int nOffDuration = 0;

    switch (BlinkType)
    {
    case BlinkTypes::DEFAULT:
        return false;
    case BlinkTypes::RANDOM_FLASHING:
        nOnDuration = 500;
        nOffDuration = 500;
        break;
    case BlinkTypes::T_1S_ON_1S_OFF:
        nOnDuration = 1000;
        nOffDuration = 1000;
        break;
    case BlinkTypes::T_2S_ON_2S_OFF:
        nOnDuration = 2000;
        nOffDuration = 2000;
        break;
    case BlinkTypes::T_3S_ON_3S_OFF:
        nOnDuration = 3000;
        nOffDuration = 3000;
        break;
    case BlinkTypes::T_4S_ON_4S_OFF:
        nOnDuration = 4000;
        nOffDuration = 4000;
        break;
    case BlinkTypes::T_5S_ON_5S_OFF:
        nOnDuration = 5000;
        nOffDuration = 5000;
        break;
    case BlinkTypes::T_6S_ON_4S_OFF:
        nOnDuration = 6000;
        nOffDuration = 4000;
        break;
    default:
        return false;
    }

    return *CTimer::m_snTimeInMilliseconds % (nOnDuration + nOffDuration) < nOnDuration;
}

constexpr uint32_t NUM_HOURS = 11;
constexpr uint32_t NUM_WEATHERS = 9;

struct TimeCycleParams
{
public:
    uint32_t mAmbient0Color;
    uint32_t mAmbient1Color;
    uint32_t mDirLightColor;
    float mDirLightMultiplier;
    float mAmbient0Multiplier;
    float mAmbient1Multiplier;
    float mAOStrength;
    float mPedAOStrength;
    float mRimLightingMultiplier;
    float mSkyLightMultiplier;
    float mDirLightSpecMultiplier;
    uint32_t mSkyBottomColorFogDensity;
    uint32_t mSunCore;
    float mCoronaBrightness;
    float mCoronaSize;
    float mDistantCoronaBrightness;
    float mDistantCoronaSize;
    float mFarClip;
    float mFogStart;
    float mDOFStart;
    float mNearDOFBlur;
    float mFarDOFBlur;
    uint32_t mLowCloudsColor;
    uint32_t mBottomCloudsColor;
    uint32_t mWater;
    float mUnused64[7];
    float mWaterReflectionMultiplier;
    float mParticleBrightness;
    float mExposure;
    float mBloomThreshold;
    float mMidGrayValue;
    float mBloomIntensity;
    uint32_t mColorCorrection;
    uint32_t mColorAdd;
    float mDesaturation;
    float mContrast;
    float mGamma;
    float mDesaturationFar;
    float mContrastFar;
    float mGammaFar;
    float mDepthFxNear;
    float mDepthFxFar;
    float mLumMin;
    float mLumMax;
    float mLumDelay;
    int32_t mCloudAlpha;
    float mUnusedD0;
    float mTemperature;
    float mGlobalReflectionMultiplier;
    float mUnusedDC;
    float mSkyColor[3];
    float mUnusedEC;
    float mSkyHorizonColor[3];
    float mUnusedFC;
    float mSkyEastHorizonColor[3];
    float mUnused10C;
    float mCloud1Color[3];
    float mUnknown11C;
    float mSkyHorizonHeight;
    float mSkyHorizonBrightness;
    float mSunAxisX;
    float mSunAxisY;
    float mCloud2Color[3];
    float mUnused13C;
    float mCloud2ShadowStrength;
    float mCloud2Threshold;
    float mCloud2Bias1;
    float mCloud2Scale;
    float mCloudInScatteringRange;
    float mCloud2Bias2;
    float mDetailNoiseScale;
    float mDetailNoiseMultiplier;
    float mCloud2Offset;
    float mCloudWarp;
    float mCloudsFadeOut;
    float mCloud1Bias;
    float mCloud1Detail;
    float mCloud1Threshold;
    float mCloud1Height;
    float mUnused17C;
    float mCloud3Color[3];
    float mUnused18C;
    float mUnknown190;
    float mUnused198[3];
    float mSunColor[3];
    float mUnused1AC;
    float mCloudsBrightness;
    float mDetailNoiseOffset;
    float mStarsBrightness;
    float mVisibleStars;
    float mMoonBrightness;
    float mUnused1C4[3];
    float mMoonColor[3];
    float mUnused1DC;
    float mMoonGlow;
    float mMoonParam3;
    float SunCenterStart;
    float SunCenterEnd;
    float mSunSize;
    float mUnused1F8[3];
    float mUnknown200;
    float mSkyBrightness;
    float mUnused208;
    int32_t mFilmGrain;
};

struct Timecycle
{
    TimeCycleParams mParams[NUM_HOURS][NUM_WEATHERS];

    static int32_t GameTimeToTimecycTimeIndex(const int32_t gameTime)
    {
        const int32_t gameTimeToTimecycTimeIndex[24] = { 0, 0, 0, 0, 0, 1, 2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 7, 8, 9, 10, 10 };
        return gameTimeToTimecycTimeIndex[gameTime];
    }
};

Timecycle* mTimeCycle = nullptr;
int(__cdecl *DrawCorona2)(int id, char r, char g, char b, float a5, CVector* pos, float radius, float a8, float a9, int a10, float a11, char a12, char a13, int a14);

void RegisterCustomCoronas()
{
    constexpr unsigned int nModelID = 0xFFFFFFFF;

    auto foundElements = *pFileContentMMap | std::views::filter([&nModelID](auto& v) {
        return v.first == nModelID;
    });

    for (auto& it : foundElements)
    {
        m_Lampposts.push_back(CLamppostInfo(it.second.vecPos, it.second.colour, it.second.fCustomSizeMult, it.second.nCoronaShowMode, it.second.nNoDistance, it.second.nDrawSearchlight, 0.0f));
    }
}

void RegisterLamppost(WplInstance* pObj, std::pair<std::multimap<unsigned int, CLamppostInfo>::iterator, std::multimap<unsigned int, CLamppostInfo>::iterator> range)
{
    CMatrix             dummyMatrix;

    float qw = pObj->RotationW;
    float qx = pObj->RotationX;
    float qy = pObj->RotationY;
    float qz = pObj->RotationZ;

    float n = 1.0f / sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    qx *= n;
    qy *= n;
    qz *= n;
    qw *= n;

    dummyMatrix.matrix.right.x = 1.0f - 2.0f * qy * qy - 2.0f * qz * qz;
    dummyMatrix.matrix.right.y = 2.0f * qx * qy - 2.0f * qz * qw;
    dummyMatrix.matrix.right.z = 2.0f * qx * qz + 2.0f * qy * qw;

    dummyMatrix.matrix.up.x = 2.0f * qx * qy + 2.0f * qz * qw;
    dummyMatrix.matrix.up.y = 1.0f - 2.0f * qx * qx - 2.0f * qz * qz;
    dummyMatrix.matrix.up.z = 2.0f * qy * qz - 2.0f * qx * qw;

    dummyMatrix.matrix.at.x = 2.0f * qx * qz - 2.0f * qy * qw;
    dummyMatrix.matrix.at.y = 2.0f * qy * qz + 2.0f * qx * qw;
    dummyMatrix.matrix.at.z = 1.0f - 2.0f * qx * qx - 2.0f * qy * qy;

    dummyMatrix.matrix.pos.x = pObj->PositionX;
    dummyMatrix.matrix.pos.y = pObj->PositionY;
    dummyMatrix.matrix.pos.z = pObj->PositionZ;

    {
        float dx = pObj->PositionX - -278.37f;
        float dy = pObj->PositionY - -1377.48f;
        float dz = pObj->PositionZ - 90.98f;
        if ((dx * dx + dy * dy + dz * dz) <= (300.0f * 300.0f))
            return;
    }

    float heading = atan2(dummyMatrix.GetUp()->y, -dummyMatrix.GetUp()->x);
    for (auto it = range.first; it != range.second; ++it)
    {
        m_Lampposts.push_back(CLamppostInfo(dummyMatrix * it->second.vecPos, it->second.colour, it->second.fCustomSizeMult, it->second.nCoronaShowMode, it->second.nNoDistance, it->second.nDrawSearchlight, heading, it->second.fObjectDrawDistance));
    }
}

WplInstance* PossiblyAddThisEntity(WplInstance* pInstance)
{
    if (m_bCatchLamppostsNow)
    {
        auto range = pFileContentMMap->equal_range(pInstance->ModelNameHash);
        if (range.first != range.second)
            RegisterLamppost(pInstance, range);
    }

    return pInstance;
}

void RegisterLODLights()
{
    static auto SolveEqSys = [](float a, float b, float c, float d, float value) -> float
    {
        float determinant = a - c;
        float x = (b - d) / determinant;
        float y = (a * d - b * c) / determinant;
        return std::min(x * value + y, d);
    };

    if (m_bCatchLamppostsNow)
        m_bCatchLamppostsNow = false;

    static auto dl = FusionFixSettings.GetRef("PREF_DISTANTLIGHTS");

    if (!dl->get())
        return;

    int currentHour = CClock::GetHours();
    if (currentHour < 19 && currentHour > 7)
        return;

    // Calculate alpha based on time
    unsigned int nTime = currentHour * 60 + CClock::GetMinutes();
    unsigned int curMin = CClock::GetMinutes();
    unsigned char bAlpha = 0;

    if (nTime >= 19 * 60)
        bAlpha = static_cast<unsigned char>(SolveEqSys(19 * 60.0f, 30.0f, 24 * 60.0f, 255.0f, static_cast<float>(nTime)));
    else if (nTime < 3 * 60)
        bAlpha = 255;
    else
        bAlpha = static_cast<unsigned char>(SolveEqSys(8 * 60.0f, 30.0f, 3 * 60.0f, 255.0f, static_cast<float>(nTime)));

    // Get time cycle parameters
    auto timeIndex = Timecycle::GameTimeToTimecycTimeIndex(currentHour);
    auto& timeCycleParams = mTimeCycle->mParams[timeIndex][*CWeather::OldWeatherType];
    float fDistantCoronaBrightness = timeCycleParams.mDistantCoronaBrightness / 10.0f;
    float fDistantCoronaSize = timeCycleParams.mDistantCoronaSize;

    // Get camera info once
    int currentCamera;
    Natives::GetRootCam(&currentCamera);
    CVector camPos;
    Natives::GetCamPos(currentCamera, &camPos.x, &camPos.y, &camPos.z);
    float fCoronaFarClip;
    Natives::GetCamFarClip(currentCamera, &fCoronaFarClip);

    for (const auto& lamppost : m_Lampposts)
    {
        // Check height bounds
        if (lamppost.vecPos.z < -15.0f || lamppost.vecPos.z > 1030.0f)
            continue;

        // Calculate distance
        float dx = camPos.x - lamppost.vecPos.x;
        float dy = camPos.y - lamppost.vecPos.y;
        float dz = camPos.z - lamppost.vecPos.z;
        float fDistSqr = dx * dx + dy * dy + dz * dz;

        float fCoronaDist = lamppost.fObjectDrawDistance - 30.0f;

        // Check if within corona range
        if (!lamppost.nNoDistance &&
            (fDistSqr <= fCoronaDist * fCoronaDist || fDistSqr >= fCoronaFarClip * fCoronaFarClip))
            continue;

        float distance = std::sqrt(fDistSqr);

        // Calculate radius
        float fRadius = lamppost.nNoDistance ? 3.5f :
            SolveEqSys(fCoronaDist, 0.0f, lamppost.fObjectDrawDistance, 3.5f, distance);

        if (bSlightlyIncreaseRadiusWithDistance)
            fRadius *= std::min(SolveEqSys(fCoronaDist, 1.0f, 9000.0, 4.0f, distance), 3.0f);

        float fAlphaDistMult = 110.0f - SolveEqSys(fCoronaDist / 4.0f, 10.0f, lamppost.fObjectDrawDistance * 4.0f, 100.0f, distance);

        // Calculate base alpha
        float baseAlpha = fCoronaAlphaMultiplier * ((bAlpha * (lamppost.colour.a / 255.0f)) / fAlphaDistMult);

        if (!lamppost.nNoDistance)
        {
            // Fade in alpha as camera moves away, similar to how radius is handled
            float alphaFade = SolveEqSys(fCoronaDist, 0.0f, lamppost.fObjectDrawDistance, 1.0f, distance);
            baseAlpha *= alphaFade;
        }

        if (lamppost.fCustomSizeMult != 0.45f)
        {
            // Regular lamppost
            float finalAlpha = baseAlpha * fDistantCoronaBrightness;
            float finalSize = fRadius * lamppost.fCustomSizeMult * fCoronaRadiusMultiplier * fDistantCoronaSize * 1270.5f;

            if (lamppost.nCoronaShowMode)
            {
                // Blinking light
                static float blinking = 1.0f;
                if (IsBlinkingNeeded(lamppost.nCoronaShowMode))
                    blinking -= *CTimer::fTimeStep / 1000.0f;
                else
                    blinking += *CTimer::fTimeStep / 1000.0f;

                blinking = std::clamp(blinking, 0.0f, 1.0f);
                finalAlpha *= blinking;
                finalSize = fRadius * lamppost.fCustomSizeMult * fCoronaRadiusMultiplier * 1270.5f;
            }

            DrawCorona2(reinterpret_cast<unsigned int>(&lamppost),
                lamppost.colour.r, lamppost.colour.g, lamppost.colour.b,
                finalAlpha,
                const_cast<CVector*>(&lamppost.vecPos),
                finalSize,
                0.0, 0.0, 0, 0.0, 0, 0, 0);
        }
        else
        {
            fRadius *= 1.3f;
            float finalSize = fRadius * lamppost.fCustomSizeMult * fCoronaRadiusMultiplier * 1270.5f;

            // Color detection
            bool isYellow = (lamppost.colour.r >= 250 && lamppost.colour.g >= 100 && lamppost.colour.b <= 100);
            bool isRed = (lamppost.colour.r >= 250 && lamppost.colour.g < 100 && lamppost.colour.b == 0);
            bool isGreen = (lamppost.colour.r == 0 && lamppost.colour.g >= 100 && lamppost.colour.b == 0);
            bool isGreenAlt = (lamppost.colour.r == 0 && lamppost.colour.g >= 250 && lamppost.colour.b == 0);

            // Time periods
            bool isYellowTime = (curMin == 9 || curMin == 19 || curMin == 29 || curMin == 39 || curMin == 49 || curMin == 59);
            bool isRedTime = ((curMin >= 0 && curMin < 9) || (curMin >= 20 && curMin < 29) || (curMin >= 40 && curMin < 49));
            bool isGreenTime = ((curMin > 9 && curMin < 19) || (curMin > 29 && curMin < 39) || (curMin > 49 && curMin < 59));

            // Heading check
            float absHeading = std::abs(lamppost.fHeading);
            bool isInHeadingRange = (absHeading >= (M_PI / 6.0f) && absHeading <= (5.0f * M_PI / 6.0f));

            bool shouldDraw = false;

            if (isYellow && isYellowTime)
            {
                shouldDraw = true;
            }
            else if (isInHeadingRange)
            {
                shouldDraw = (isRed && isRedTime) || (isGreen && isGreenTime);
            }
            else
            {
                shouldDraw = (isGreenAlt && isRedTime) || (isRed && isGreenTime);
            }

            if (shouldDraw)
            {
                DrawCorona2(reinterpret_cast<unsigned int>(&lamppost),
                    lamppost.colour.r, lamppost.colour.g, lamppost.colour.b,
                    baseAlpha,
                    const_cast<CVector*>(&lamppost.vecPos),
                    finalSize,
                    0.0, 0.0, 0, 0.0, 0, 0, 0);
            }
        }
    }
}

class LODLights
{
public:
    LODLights()
    {
        FusionFix::onInitEventAsync() += []()
        {
            static auto ll = GetModuleHandleW(L"IVLodLights.asi");
            if (ll)
                return;

            CIniReader iniReader("");
            fCoronaRadiusMultiplier = iniReader.ReadFloat("PROJECT2DFX", "CoronaRadiusMultiplier", 1.0f);
            fCoronaAlphaMultiplier = iniReader.ReadFloat("PROJECT2DFX", "CoronaAlphaMultiplier", 1.0f);
            bSlightlyIncreaseRadiusWithDistance = iniReader.ReadInteger("PROJECT2DFX", "SlightlyIncreaseRadiusWithDistance", 1) != 0;
            bDisableDefaultLodLights = iniReader.ReadInteger("PROJECT2DFX", "DisableDefaultLodLights", 1) != 0;

            auto pattern = find_pattern("05 ? ? ? ? 50 8D 4C 24 60", "05 ? ? ? ? D9 5C 24 04 8D 4C 24 38 D9");
            mTimeCycle = *pattern.get_first<Timecycle*>(1);

            pattern = find_pattern("8B 15 ? ? ? ? 56 8D 72 01", "A1 ? ? ? ? 56 8D 70 01 81 FE");
            DrawCorona2 = (int(__cdecl*)(int id, char r, char g, char b, float a5, CVector* pos, float radius, float a8, float a9, int a10, float a11, char a12, char a13, int a14))(pattern.get(0).get<uintptr_t>(0));

            pattern = find_pattern("E8 ? ? ? ? 83 3D ? ? ? ? ? 74 05 E8 ? ? ? ? 6A 05", "E8 ? ? ? ? 83 3D ? ? ? ? 00 74 05 E8 ? ? ? ? 6A 05");
            static raw_mem RegisterLODLightsAddr(pattern.get_first(0), { 0x90, 0x90, 0x90, 0x90, 0x90 });
            injector::MakeCALL(pattern.get_first(0), RegisterLODLights, true);

            static std::optional<raw_mem> LoadObjectInstanceHookAddr;
            pattern = hook::pattern("8B 75 08 8D 44 24 1C 50 FF 76 1C C6 44 24");
            if (!pattern.empty())
            {
                LoadObjectInstanceHookAddr = raw_mem(pattern.get_first(0), { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });
                struct LoadObjectInstanceHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        static bool bOnce = false;
                        if (!bOnce)
                        {
                            LoadDatFile();
                            RegisterCustomCoronas();
                            bOnce = true;
                        }

                        regs.esi = *(uintptr_t*)(regs.ebp + 0x8);
                        regs.eax = (regs.esp + 0x1C);

                        PossiblyAddThisEntity((WplInstance*)regs.esi);
                    }
                }; injector::MakeInline<LoadObjectInstanceHook>(pattern.get_first(0), pattern.get_first(7));
            }
            else
            {
                pattern = hook::pattern("8B 5D 08 8B 4B 1C 8D 44 24 14 50 51");
                LoadObjectInstanceHookAddr = raw_mem(pattern.get_first(0), { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });
                struct LoadObjectInstanceHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        static bool bOnce = false;
                        if (!bOnce)
                        {
                            LoadDatFile();
                            RegisterCustomCoronas();
                            bOnce = true;
                        }

                        regs.ebx = *(uintptr_t*)(regs.ebp + 0x8);
                        regs.ecx = *(uintptr_t*)(regs.ebx + 0x1C);

                        PossiblyAddThisEntity((WplInstance*)regs.ebx);
                    }
                }; injector::MakeInline<LoadObjectInstanceHook>(pattern.get_first(0), pattern.get_first(6));
            }

            static SafetyHookMid WaterMultiplierHook = {};
            pattern = find_pattern("F3 0F 59 45 ? F3 0F 11 45 ? 0F 28 85");
            if (!pattern.empty())
            {
                WaterMultiplierHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs) {
                    static auto dl = FusionFixSettings.GetRef("PREF_DISTANTLIGHTS");
                    if (dl->get())
                        regs.xmm0.f32[0] = 1.0f;
                });
            }
            else
            {
                pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 59 C4 F3 0F 11 45");
                static float* dword_F17AA8 = *pattern.get_first<float*>(4);
                injector::MakeNOP(pattern.get_first(0), 12, true);
                WaterMultiplierHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs) {
                    regs.xmm0.f32[0] = *dword_F17AA8;
                    static auto dl = FusionFixSettings.GetRef("PREF_DISTANTLIGHTS");
                    if (dl->get())
                        regs.xmm0.f32[0] = 1.0f;
                    regs.xmm0.f32[0] *= regs.xmm4.f32[0];
                });
            }

            static std::optional<raw_mem> DisableDefaultLodLightsHookAddr;
            pattern = hook::pattern("83 F8 08 0F 8C ? ? ? ? 83 3D");
            if (!pattern.empty())
            {
                DisableDefaultLodLightsHookAddr = raw_mem(pattern.get_first(0), { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });
                if (bDisableDefaultLodLights)
                {
                    static uintptr_t loc_D658B0 = resolve_next_displacement(pattern.get_first(0)).value();
                    struct DisableDefaultLodLights
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            static auto dl = FusionFixSettings.GetRef("PREF_DISTANTLIGHTS");
                            if (!dl->get() && regs.eax < 8)
                                force_return_address(loc_D658B0);
                        }
                    }; injector::MakeInline<DisableDefaultLodLights>(pattern.get_first(0), pattern.get_first(9));
                }
            }
            else
            {
                pattern = hook::pattern("0F 8C ? ? ? ? 83 3D ? ? ? ? ? 75 ? F3 0F 10 05");
                if (!pattern.empty())
                {
                    DisableDefaultLodLightsHookAddr = raw_mem(pattern.get_first(0), { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });
                    if (bDisableDefaultLodLights)
                    {
                        static uintptr_t loc_D658B0 = resolve_displacement(pattern.get_first(0)).value();
                        struct DisableDefaultLodLights
                        {
                            void operator()(injector::reg_pack& regs)
                            {
                                static auto dl = FusionFixSettings.GetRef("PREF_DISTANTLIGHTS");
                                if (!dl->get() && regs.ebx < 8)
                                    force_return_address(loc_D658B0);
                            }
                        }; injector::MakeInline<DisableDefaultLodLights>(pattern.get_first(0), pattern.get_first(6));
                    }
                }
            }

            static float f0 = 0.0f;
            pattern = find_pattern("F3 0F 5C 0D ? ? ? ? F3 0F 11 84 24 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 59 0D ? ? ? ? 0F 2F C8 F3 0F 11 4C 24", "F3 0F 5C 05 ? ? ? ? F3 0F 59 05 ? ? ? ? 0F 2F C1 76 08 F3 0F 11 4C 24 ? EB 06");
            injector::WriteMemory(pattern.get_first(4), &f0, true);

            auto InitIVLodLights = []()
            {
                RegisterLODLightsAddr.Restore();
                LoadObjectInstanceHookAddr->Restore();
                DisableDefaultLodLightsHookAddr->Restore();
                WaterMultiplierHook.reset();

                FileContent.clear();
                FileContentMMap.clear();
                m_Lampposts.clear();
            };

            CallbackHandler::RegisterCallback(L"IVLodLights.asi", InitIVLodLights);
        };
    }
} LODLights;

extern "C"
{
    int32_t __declspec(dllexport) GetDistantLightsPrefValue()
    {
        return FusionFixSettings("PREF_DISTANTLIGHTS");
    }
}