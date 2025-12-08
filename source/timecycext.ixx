module;

#include <common.hxx>

export module timecycext;

import common;
import comvars;

export constexpr auto NUMWEATHERS = 9;
export constexpr auto NUMHOURS = 11;
constexpr int ACTUAL_HOURS[NUMHOURS] = { 0, 5, 6, 7, 9, 12, 18, 19, 20, 21, 22 };
float (*RainDefaultLight)[4] = nullptr;

export std::vector<std::pair<int, float>> currentTimecycleModifiers;

namespace CTimeCycleModifier
{
    constexpr auto STRIDE = 47;
    export constexpr auto ARRAY_SIZE = 900;
    int (*msTimeCycleModifiers)[ARRAY_SIZE * STRIDE];
    bool bOverwrite = false;

    SafetyHookInline shApplyTimecycModifier = {};
    void __fastcall ApplyTimecycModifier(float* _this, void* edx, uintptr_t a2, float a3, char a4)
    {
        auto hash = *(uint32_t*)a2;
        if (hash && (a2 >= (uintptr_t)msTimeCycleModifiers))
        {
            auto diff = a2 - (uintptr_t)msTimeCycleModifiers;
            auto index = (diff / 47) / 4;
            currentTimecycleModifiers.emplace_back(index, a3);
            bOverwrite = false;
        }

        shApplyTimecycModifier.unsafe_fastcall(_this, edx, a2, a3, a4);
    }

    static int cachedIndex = -1;
    injector::hook_back<void(__fastcall*)(void*, void*, int, float)> hbBlendWithModifier;
    void __fastcall BlendWithModifier(void* _this, void* edx, int a2, float a3)
    {
        auto diff = a2 - (uintptr_t)msTimeCycleModifiers;
        cachedIndex = (diff / 47) / 4;

        return hbBlendWithModifier.fun(_this, edx, a2, a3);
    }

    void __fastcall BlendWithModifier2(void* _this, void* edx, int a2, float a3)
    {
        currentTimecycleModifiers.emplace_back(cachedIndex, a3);
        bOverwrite = true;

        return hbBlendWithModifier.fun(_this, edx, a2, a3);
    }

    int GetIndexFromHash(int hashValue)
    {
        for (auto index = 0; index < ARRAY_SIZE; index++)
        {
            if ((*msTimeCycleModifiers)[index * STRIDE] == hashValue)
            {
                return index;
            }
        }

        return 0;
    }

    int GetHashFromIndex(int index)
    {
        if (index >= 0 && index < ARRAY_SIZE)
        {
            return (*msTimeCycleModifiers)[index * STRIDE];
        }

        return 0;
    }
}

template <typename T>
void FillGaps(T(*out)[NUMWEATHERS], float (*in)[NUMWEATHERS])
{
    int w;
    for (w = 0; w < NUMWEATHERS; w++)
    {
        for (int h = 0; h < NUMHOURS; h++)
            out[h][w] = in[h][w];

        #define NEXT(h) (((h) + 1) % NUMHOURS)
        #define PREV(h) (((h) + NUMHOURS - 1) % NUMHOURS)
        int hend, h1, h2;
        for (hend = 0; hend < NUMHOURS; hend++)
            if (in[hend][w] != -1.0f)
                goto foundstart;
        return; // this should never happen
    foundstart:
        // Found the start of a block of filled in entries
        for (h1 = NEXT(hend); h1 != hend; h1 = h2)
        {
            // Skip filled in entries
            for (; h1 != hend; h1 = NEXT(h1))
                if (in[h1][w] == -1.0f)
                    goto foundfirst;
            break; // all filled in already
        foundfirst:
            // h1 is now the first -1 after n filled in values
            for (h2 = NEXT(h1);; h2 = NEXT(h2))
                if (in[h2][w] != -1.0f)
                    goto foundlast;
            break;
        foundlast:
            // h2 is now the first entry after a row of -1s
            h1 = PREV(h1); // make h1 the first before a row of -1s
            int n = (h2 - h1 + NUMHOURS) % NUMHOURS;
            if (n == 0)
                n = NUMHOURS; // can't happen
            float step = (in[h2][w] - in[h1][w]) / n;

            for (int i = 1; i < n; i++)
            {
                float f = (float)i / n;
                out[(h1 + i) % NUMHOURS][w] = T(in[h2][w] * f + in[h1][w] * (1.0f - f));
            }
        }
    }
}

export class CTimeCycleModifiersExt
{
    struct TimeCycleModifierExt
    {
        float fVolFogDensity = -1.0f;
        float fVolFogHeightFalloff = -1.0f;
        float fVolFogAltitudeTweak = -1.0f;
        float fVolFogPower = -1.0f;
        float fSSIntensity = -1.0f;
        float fCHSSLightSize = -1.0f;
    };

    static std::string_view trim(std::string_view sv)
    {
        const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        auto start = std::ranges::find_if(sv, notSpace);
        auto rsv = sv.substr(std::distance(sv.begin(), start));
        auto end = std::ranges::find_if(rsv.rbegin(), rsv.rend(), notSpace);
        return rsv.substr(0, rsv.size() - std::distance(rsv.rbegin(), end));
    }

public:
    static inline std::array<TimeCycleModifierExt, CTimeCycleModifier::ARRAY_SIZE> m_TimecycleModifiers;

    static void Initialise(const std::filesystem::path& path)
    {
        std::ifstream is(path, std::ios::binary);
        if (!is)
            return;

        std::string fileContent((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
        std::istringstream iss(fileContent);
        std::string line;
        while (std::getline(iss, line)) {
            std::string_view svLine = trim(line);
            if (svLine.empty() || svLine.front() == '/' || svLine.front() == '#')
                continue;

            char keyBuffer[256] = { 0 };
            float fVolFogDensity = -1.0f;
            float fVolFogHeightFalloff = -1.0f;
            float fVolFogAltitudeTweak = -1.0f;
            float fVolFogPower = -1.0f;
            float fSSIntensity = -1.0f;
            float fCHSSLightSize = -1.0f;
            sscanf(svLine.data(), "%255s %f %f %f %f %f %f", keyBuffer, &fVolFogDensity, &fVolFogHeightFalloff, &fVolFogAltitudeTweak, &fVolFogPower, &fSSIntensity, &fCHSSLightSize);
            m_TimecycleModifiers[CTimeCycleModifier::GetIndexFromHash(hashStringLowercaseFromSeed(keyBuffer, 0))] = { fVolFogDensity, fVolFogHeightFalloff, fVolFogAltitudeTweak, fVolFogPower, fSSIntensity, fCHSSLightSize };
        }
    }
};

export class CTimeCycleExt
{
    static inline float m_fVolFogDensity[NUMHOURS][NUMWEATHERS];       // control overall fog density
    static inline float m_fVolFogHeightFalloff[NUMHOURS][NUMWEATHERS]; // how much the fog accumulates at low altitudes
    static inline float m_fVolFogAltitudeTweak[NUMHOURS][NUMWEATHERS]; // control fog density at very high altitudes
    static inline float m_fVolFogPower[NUMHOURS][NUMWEATHERS];         // control fog start distance
    static inline float m_fSSIntensity[NUMHOURS][NUMWEATHERS];         // control sun shafts intensity
    static inline float m_fCHSSLightSize[NUMHOURS][NUMWEATHERS];       // control CHSS penumbra scaling

    static inline float m_fDirLightColorR[NUMHOURS][NUMWEATHERS];
    static inline float m_fDirLightColorG[NUMHOURS][NUMWEATHERS];
    static inline float m_fDirLightColorB[NUMHOURS][NUMWEATHERS];
    static inline float m_fDirLightMultiplier[NUMHOURS][NUMWEATHERS];

    static inline float m_fCurrentVolFogDensity;
    static inline float m_fCurrentVolFogHeightFalloff;
    static inline float m_fCurrentVolFogAltitudeTweak;
    static inline float m_fCurrentVolFogPower;
    static inline float m_fCurrentSSIntensity;
    static inline float m_fCurrentCHSSLightSize;

    static inline float m_fCurrentDirLightColorR;
    static inline float m_fCurrentDirLightColorG;
    static inline float m_fCurrentDirLightColorB;
    static inline float m_fCurrentDirLightMultiplier;

    static inline float tmp_fVolFogDensity[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fVolFogHeightFalloff[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fVolFogAltitudeTweak[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fVolFogPower[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fSSIntensity[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fCHSSLightSize[NUMHOURS][NUMWEATHERS];

public:
    static inline float tmp_fDirLightColorR[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fDirLightColorG[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fDirLightColorB[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fDirLightMultiplier[NUMHOURS][NUMWEATHERS];

private:
    static inline float interp_c0, interp_c1, interp_c2, interp_c3;

public:
    static FusionFix::Event<>&  onCTimeCycleExtInit() { static FusionFix::Event<> ev; return ev; }

    static float GetVolFogDensity() { return m_fCurrentVolFogDensity; }
    static float GetVolFogHeightFalloff() { return m_fCurrentVolFogHeightFalloff; }
    static float GetVolFogAltitudeTweak() { return m_fCurrentVolFogAltitudeTweak; }
    static float GetVolFogPower() { return m_fCurrentVolFogPower; }
    static float GetSSIntensity() { return m_fCurrentSSIntensity; }
    static float GetCHSSLightSize() { return m_fCurrentCHSSLightSize; }

    static float GetDirLightColorR() { return m_fCurrentDirLightColorR; }
    static float GetDirLightColorG() { return m_fCurrentDirLightColorG; }
    static float GetDirLightColorB() { return m_fCurrentDirLightColorB; }
    static float GetDirLightMultiplier() { return m_fCurrentDirLightMultiplier; }

    static void Initialise(std::filesystem::path path)
    {
        for (int h = 0; h < NUMHOURS; ++h)
        {
            for (int w = 0; w < NUMWEATHERS; ++w)
            {
                tmp_fVolFogDensity[h][w] = 0.004f;
                tmp_fVolFogHeightFalloff[h][w] = 0.015f;
                tmp_fVolFogAltitudeTweak[h][w] = 0.7f;
                tmp_fVolFogPower[h][w] = 1.0f;
                tmp_fSSIntensity[h][w] = 0.002f;
                tmp_fCHSSLightSize[h][w] = 500.0f;
            }
        }

        std::ifstream is(path, std::ios::binary);
        if (!is)
            return;

        std::string fileContent((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
        std::string_view contentView(fileContent);
        size_t pos = 0;

        for (int w = 0; w < NUMWEATHERS; ++w)
        {
            for (int h = 0; h < NUMHOURS; ++h)
            {
                while (pos < contentView.size())
                {
                    size_t nextNewline = contentView.find('\n', pos);
                    if (nextNewline == std::string_view::npos)
                        nextNewline = contentView.size();

                    std::string_view line = contentView.substr(pos, nextNewline - pos);
                    pos = nextNewline < contentView.size() ? nextNewline + 1 : nextNewline;
                    line.remove_prefix(std::min(line.find_first_not_of(" \t\r"), line.size()));

                    if (!line.empty() && line.front() != '/' && line.front() != '#')
                    {
                        float fVolFogDensity = 0.004f;
                        float fVolFogHeightFalloff = 0.015f;
                        float fVolFogAltitudeTweak = 0.7f;
                        float fVolFogPower = 1.0f;
                        float fSSIntensity = 0.002f;
                        float fCHSSLightSize = 500.0f;
                        sscanf(line.data(), "%f %f %f %f %f %f", &fVolFogDensity, &fVolFogHeightFalloff, &fVolFogAltitudeTweak, &fVolFogPower, &fSSIntensity, &fCHSSLightSize);
                        tmp_fVolFogDensity[h][w] = fVolFogDensity;
                        tmp_fVolFogHeightFalloff[h][w] = fVolFogHeightFalloff;
                        tmp_fVolFogAltitudeTweak[h][w] = fVolFogAltitudeTweak;
                        tmp_fVolFogPower[h][w] = fVolFogPower;
                        tmp_fSSIntensity[h][w] = fSSIntensity;
                        tmp_fCHSSLightSize[h][w] = fCHSSLightSize;
                        break;
                    }
                }
            }
        }

        {
            for (int w = 0; w < NUMWEATHERS; ++w)
            {
                for (int h = 0; h < NUMHOURS; ++h)
                {
                    auto& DirLightColorR = CTimeCycleExt::tmp_fDirLightColorR[h][w];
                    auto& DirLightColorG = CTimeCycleExt::tmp_fDirLightColorG[h][w];
                    auto& DirLightColorB = CTimeCycleExt::tmp_fDirLightColorB[h][w];
                    auto& DirLightMultip = CTimeCycleExt::tmp_fDirLightMultiplier[h][w];

                    if (DirLightColorR == 0.0f && DirLightColorG == 0.0f && DirLightColorB == 0.0f && DirLightMultip < 1.0f)
                    {
                        DirLightColorR = (*RainDefaultLight)[0] * (1.0f - DirLightMultip);
                        DirLightColorG = (*RainDefaultLight)[1] * (1.0f - DirLightMultip);
                        DirLightColorB = (*RainDefaultLight)[2] * (1.0f - DirLightMultip);
                        DirLightMultip = (*RainDefaultLight)[3] * (1.0f - DirLightMultip);
                    }
                }
            }
        }

        UpdateArrays();
    }

    static void UpdateArrays()
    {
        FillGaps(m_fVolFogDensity, tmp_fVolFogDensity);
        FillGaps(m_fVolFogHeightFalloff, tmp_fVolFogHeightFalloff);
        FillGaps(m_fVolFogAltitudeTweak, tmp_fVolFogAltitudeTweak);
        FillGaps(m_fVolFogPower, tmp_fVolFogPower);
        FillGaps(m_fSSIntensity, tmp_fSSIntensity);
        FillGaps(m_fCHSSLightSize, tmp_fCHSSLightSize);

        FillGaps(m_fDirLightColorR, tmp_fDirLightColorR);
        FillGaps(m_fDirLightColorG, tmp_fDirLightColorG);
        FillGaps(m_fDirLightColorB, tmp_fDirLightColorB);
        FillGaps(m_fDirLightMultiplier, tmp_fDirLightMultiplier);
    }

    static float Interpolate(float* a, float* b)
    {
        return a[*CWeather::OldWeatherType] * interp_c0 +
            b[*CWeather::OldWeatherType] * interp_c1 +
            a[*CWeather::NewWeatherType] * interp_c2 +
            b[*CWeather::NewWeatherType] * interp_c3;
    }

    static void Update()
    {
        int currentHour = CClock::GetHours();
        int currentMinute = CClock::GetMinutes();
        int currentSecond = CClock::GetSeconds();

        auto currentTime = currentHour + (currentMinute / 60.0f) + (currentSecond / 3600.0f);
        int h1 = 0, h2 = 0;
        float timeInterp = 0.0f;

        if (currentTime >= ACTUAL_HOURS[NUMHOURS - 1])
        {
            h1 = NUMHOURS - 1;
            h2 = 0;
            auto startTime = static_cast<float>(ACTUAL_HOURS[h1]);
            auto endTime = static_cast<float>(ACTUAL_HOURS[h2] + 24);
            timeInterp = (currentTime - startTime) / (endTime - startTime);
        }
        else
        {
            for (auto i = 0; i < NUMHOURS - 1; i++)
            {
                if (currentTime >= ACTUAL_HOURS[i] && currentTime < ACTUAL_HOURS[i + 1])
                {
                    h1 = i;
                    h2 = i + 1;
                    break;
                }
            }
            float startTime = static_cast<float>(ACTUAL_HOURS[h1]);
            float endTime = static_cast<float>(ACTUAL_HOURS[h2]);
            timeInterp = (currentTime - startTime) / (endTime - startTime);
        }

        timeInterp = std::clamp(timeInterp, 0.0f, 1.0f);

        interp_c0 = (1.0f - timeInterp) * (1.0f - *CWeather::InterpolationValue);
        interp_c1 = timeInterp * (1.0f - *CWeather::InterpolationValue);
        interp_c2 = (1.0f - timeInterp) * *CWeather::InterpolationValue;
        interp_c3 = timeInterp * *CWeather::InterpolationValue;

        auto INTERP = [&h1, &h2](auto& v) {
            return Interpolate(v[h1], v[h2]);
        };

        float baseVolFogDensity = INTERP(m_fVolFogDensity);
        float baseVolFogHeightFalloff = INTERP(m_fVolFogHeightFalloff);
        float baseVolFogAltitudeTweak = INTERP(m_fVolFogAltitudeTweak);
        float baseVolFogPower = INTERP(m_fVolFogPower);
        float baseSSIntensity = INTERP(m_fSSIntensity);
        float baseCHSSLightSize = INTERP(m_fCHSSLightSize);

        for (const auto& it : currentTimecycleModifiers)
        {
            if (it.first >= 0 && it.first < CTimeCycleModifier::ARRAY_SIZE)
            {
                auto& modifier = CTimeCycleModifiersExt::m_TimecycleModifiers[it.first];
                if (modifier.fVolFogDensity != -1.0f)
                    baseVolFogDensity = baseVolFogDensity * (1.0f - it.second) + modifier.fVolFogDensity * it.second;
                if (modifier.fVolFogHeightFalloff != -1.0f)
                    baseVolFogHeightFalloff = baseVolFogHeightFalloff * (1.0f - it.second) + modifier.fVolFogHeightFalloff * it.second;
                if (modifier.fVolFogAltitudeTweak != -1.0f)
                    baseVolFogAltitudeTweak = baseVolFogAltitudeTweak * (1.0f - it.second) + modifier.fVolFogAltitudeTweak * it.second;
                if (modifier.fVolFogPower != -1.0f)
                    baseVolFogPower = baseVolFogPower * (1.0f - it.second) + modifier.fVolFogPower * it.second;
                if (modifier.fSSIntensity != -1.0f)
                    baseSSIntensity = baseSSIntensity * (1.0f - it.second) + modifier.fSSIntensity * it.second;
                if (modifier.fCHSSLightSize != -1.0f)
                    baseCHSSLightSize = baseCHSSLightSize * (1.0f - it.second) + modifier.fCHSSLightSize * it.second;
            }
        }

        m_fCurrentVolFogDensity = baseVolFogDensity;
        m_fCurrentVolFogHeightFalloff = baseVolFogHeightFalloff;
        m_fCurrentVolFogAltitudeTweak = baseVolFogAltitudeTweak;
        m_fCurrentVolFogPower = baseVolFogPower;
        m_fCurrentSSIntensity = baseSSIntensity;
        m_fCurrentCHSSLightSize = baseCHSSLightSize;

        m_fCurrentDirLightColorR = INTERP(m_fDirLightColorR);
        m_fCurrentDirLightColorG = INTERP(m_fDirLightColorG);
        m_fCurrentDirLightColorB = INTERP(m_fDirLightColorB);
        m_fCurrentDirLightMultiplier = INTERP(m_fDirLightMultiplier);

        currentTimecycleModifiers.clear();
    }
};

class TimecycExt
{
public:
    TimecycExt()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = find_pattern("D9 1D ? ? ? ? 6A ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? B9", "D9 1D ? ? ? ? 6A 00 68 38 6E DC 00", "D9 1D ? ? ? ? 6A ? 68 ? ? ? ? E8 ? ? ? ? D9 05");
            RainDefaultLight = *pattern.get_first<decltype(RainDefaultLight)>(2);

            pattern = find_pattern("56 E8 ? ? ? ? 83 C4 ? E8 ? ? ? ? 5F 5E 5B 8B E5 5D C3", "53 E8 ? ? ? ? 83 C4 04 E8 ? ? ? ? 8B 8C 24");
            static auto CTimeCycleInitialiseHook = safetyhook::create_mid(pattern.get_first(14), [](SafetyHookContext& regs)
            {
                CTimeCycleExt::Initialise(currentEpisodePath() / "pc" / "data" / "timecycext.dat");
                CTimeCycleExt::onCTimeCycleExtInit().executeAll();
                
                static const auto moduleParentPath = GetModulePath(GetModuleHandleW(NULL)).parent_path();
                CTimeCycleModifiersExt::Initialise(moduleParentPath / "pc" / "data" / "timecyclemodifiersext.dat");
            });

            pattern = find_pattern("FF 35 ? ? ? ? FF 15 ? ? ? ? 85 C0 75 ? 38 05 ? ? ? ? 74 ? 38 05 ? ? ? ? 75 ? 33 C0 EB ? B8 ? ? ? ? 0A 05 ? ? ? ? 0A 05 ? ? ? ? 0F 85 ? ? ? ? E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? 8B 0D ? ? ? ? 8B 15",
                "51 E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? E8");
            static auto CClockUpdateHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                CTimeCycleExt::Update();
            });

            pattern = hook::pattern("F3 0F 11 15 ? ? ? ? 76 ? F3 0F 10 44 24 ? 0F 2F 47 ? 76 ? F3 0F 5C 15 ? ? ? ? 0F 57 C0 F3 0F 59 15 ? ? ? ? 0F 2F C2 77");
            if (!pattern.empty())
            {
                static auto OverwriteCurBlendHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    if (CTimeCycleModifier::bOverwrite && !currentTimecycleModifiers.empty())
                    {
                        CTimeCycleModifier::bOverwrite = false;
                        auto& back = currentTimecycleModifiers.back().second;
                        if (back == 1.0f)
                            back = regs.xmm2.f32[0];
                    }
                });
            }
            else
            {
                pattern = hook::pattern("F3 0F 11 05 ? ? ? ? 76 ? F3 0F 10 53");
                static auto OverwriteCurBlendHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    if (CTimeCycleModifier::bOverwrite && !currentTimecycleModifiers.empty())
                    {
                        CTimeCycleModifier::bOverwrite = false;
                        auto& back = currentTimecycleModifiers.back().second;
                        if (back == 1.0f)
                            back = regs.xmm0.f32[0];
                    }
                });
            }

            pattern = find_pattern("8D B0 ? ? ? ? 6A ? 51", "8D B8 ? ? ? ? B9 ? ? ? ? D9 1C 24");
            CTimeCycleModifier::msTimeCycleModifiers = *pattern.get_first<decltype(CTimeCycleModifier::msTimeCycleModifiers)>(2);

            pattern = find_pattern("51 F3 0F 10 64 24 ? 0F 57 F6", "F3 0F 10 4C 24 ? 0F 57 F6 0F 2F CE");
            CTimeCycleModifier::shApplyTimecycModifier = safetyhook::create_inline(pattern.get_first(), CTimeCycleModifier::ApplyTimecycModifier);

            pattern = find_pattern("E8 ? ? ? ? F3 0F 10 84 B4 ? ? ? ? 51", "E8 ? ? ? ? D9 44 24 ? 51 8B D1");
            CTimeCycleModifier::hbBlendWithModifier.fun = injector::MakeCALL(pattern.get_first(), CTimeCycleModifier::BlendWithModifier).get();

            pattern = find_pattern("E8 ? ? ? ? 8B 4C 24 ? 8B 54 24 ? 0F 57 C0 0F 2F 81 ? ? ? ? 0F B6 D2 B8", "E8 ? ? ? ? 0F 57 C0 0F 2F 86 ? ? ? ? 72");
            CTimeCycleModifier::hbBlendWithModifier.fun = injector::MakeCALL(pattern.get_first(), CTimeCycleModifier::BlendWithModifier2).get();
        };
    }
} TimecycExt;