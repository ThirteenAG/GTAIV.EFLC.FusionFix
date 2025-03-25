module;

#include <common.hxx>

export module timecycext;

import common;
import comvars;

constexpr auto NUMWEATHERS = 8;
constexpr auto NUMHOURS = 11;
constexpr int ACTUAL_HOURS[NUMHOURS] = { 0, 5, 6, 7, 9, 12, 18, 19, 20, 21, 22 };

export std::vector<std::pair<int, float>> currentTimecycleModifiers;

namespace CTimeCycleModifier
{
    constexpr auto STRIDE = 47;
    constexpr auto ARRAY_SIZE = 900;
    int (*dword_15DE3A8)[ARRAY_SIZE];

    SafetyHookInline shApplyTimecycModifier = {};
    void __fastcall ApplyTimecycModifier(float* _this, void* edx, uintptr_t a2, float a3, char a4)
    {
        auto hash = *(uint32_t*)a2;
        if (hash && (a2 >= (uintptr_t)CTimeCycleModifier::dword_15DE3A8))
        {
            auto diff = a2 - (uintptr_t)CTimeCycleModifier::dword_15DE3A8;
            auto index = (diff / 47) / 4;
            currentTimecycleModifiers.emplace_back(index, a3);
        }

        shApplyTimecycModifier.unsafe_fastcall(_this, edx, a2, a3, a4);
    }

    injector::hook_back<void(__fastcall*)(void*, void*, int, float)> hbBlendWithModifier;
    void __fastcall BlendWithModifier(void* _this, void* edx, int a2, float a3)
    {
        auto diff = a2 - (uintptr_t)dword_15DE3A8;
        auto index = (diff / 47) / 4;
        currentTimecycleModifiers.emplace_back(index, a3);

        return hbBlendWithModifier.fun(_this, edx, a2, a3);
    }

    void __fastcall BlendWithModifier2(void* _this, void* edx, int a2, float a3)
    {
        currentTimecycleModifiers.back().second = a3;
        return hbBlendWithModifier.fun(_this, edx, a2, a3);
    }

    int GetIndexFromHash(int hashValue)
    {
        for (auto index = 0; index < ARRAY_SIZE; index++)
        {
            if ((*dword_15DE3A8)[index * STRIDE] == hashValue)
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
            return (*dword_15DE3A8)[index * STRIDE];
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
        float fVolFogDensity;
        float fVolFogHeightFalloff;
        float fVolFogAltitudeTweak;
        float fVolFogPower;
        float fVolFogColorFactor;

        float fVolFogDensityInterp;
        float fVolFogHeightFalloffInterp;
        float fVolFogAltitudeTweakInterp;
        float fVolFogPowerInterp;
        float fVolFogColorFactorInterp;
    };

    static std::string_view trim(std::string_view sv)
    {
        const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        auto start = std::ranges::find_if(sv, notSpace);
        auto rsv = sv.substr(std::distance(sv.begin(), start));
        auto end = std::ranges::find_if(rsv.rbegin(), rsv.rend(), notSpace);
        return rsv.substr(0, rsv.size() - std::distance(rsv.rbegin(), end));
    }

    static inline bool initialized = false;

public:
    static inline std::array<TimeCycleModifierExt, CTimeCycleModifier::ARRAY_SIZE> m_TimecycleModifiers;

    static void Initialise(const std::filesystem::path& path)
    {
        initialized = false;

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
            float fVolFogDensity, fVolFogHeightFalloff, fVolFogAltitudeTweak, fVolFogPower, fVolFogColorFactor;
            if (sscanf(svLine.data(), "%255s %f %f %f %f %f",
                keyBuffer, &fVolFogDensity, &fVolFogHeightFalloff,
                &fVolFogAltitudeTweak, &fVolFogPower, &fVolFogColorFactor) == 6)
            {
                m_TimecycleModifiers[CTimeCycleModifier::GetIndexFromHash(hashStringLowercaseFromSeed(keyBuffer, 0))] = { fVolFogDensity, fVolFogHeightFalloff, fVolFogAltitudeTweak, fVolFogPower, fVolFogColorFactor };
                initialized = true;
            }
        }
    }

    static bool IsInitialized()
    {
        return initialized;
    }
};

export class CTimeCycleExt
{
    static inline float m_fVolFogDensity[NUMHOURS][NUMWEATHERS];       // control overall fog density
    static inline float m_fVolFogHeightFalloff[NUMHOURS][NUMWEATHERS]; // how much the fog accumulates at low altitudes
    static inline float m_fVolFogAltitudeTweak[NUMHOURS][NUMWEATHERS]; // control fog density at very high altitudes
    static inline float m_fVolFogPower[NUMHOURS][NUMWEATHERS];         // control fog start distance
    static inline float m_fVolFogColorFactor[NUMHOURS][NUMWEATHERS];   // control blending between the near and far color

    static inline float m_fCurrentVolFogDensity;
    static inline float m_fCurrentVolFogHeightFalloff;
    static inline float m_fCurrentVolFogAltitudeTweak;
    static inline float m_fCurrentVolFogPower;
    static inline float m_fCurrentVolFogColorFactor;

    static inline float tmp_fVolFogDensity[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fVolFogHeightFalloff[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fVolFogAltitudeTweak[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fVolFogPower[NUMHOURS][NUMWEATHERS];
    static inline float tmp_fVolFogColorFactor[NUMHOURS][NUMWEATHERS];

    static inline float interp_c0, interp_c1, interp_c2, interp_c3;

    static inline bool initialized = false;

public:
    static float IsInitialized() { return initialized; }
    static float GetVolFogDensity() { return m_fCurrentVolFogDensity; }
    static float GetVolFogHeightFalloff() { return m_fCurrentVolFogHeightFalloff; }
    static float GetVolFogAltitudeTweak() { return m_fCurrentVolFogAltitudeTweak; }
    static float GetVolFogPower() { return m_fCurrentVolFogPower; }
    static float GetVolFogColorFactor() { return m_fCurrentVolFogColorFactor; }

    static void Initialise(std::filesystem::path path)
    {
        initialized = false;

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
                    line.remove_prefix(min(line.find_first_not_of(" \t\r"), line.size()));

                    if (!line.empty() && line.front() != '/' && line.front() != '#')
                    {
                        float fVolFogDensity, fVolFogHeightFalloff, fVolFogAltitudeTweak, fVolFogPower, fVolFogColorFactor;
                        if (sscanf(line.data(), "%f %f %f %f %f",
                            &fVolFogDensity, &fVolFogHeightFalloff,
                            &fVolFogAltitudeTweak, &fVolFogPower,
                            &fVolFogColorFactor) == 5)
                        {
                            tmp_fVolFogDensity[h][w] = fVolFogDensity;
                            tmp_fVolFogHeightFalloff[h][w] = fVolFogHeightFalloff;
                            tmp_fVolFogAltitudeTweak[h][w] = fVolFogAltitudeTweak;
                            tmp_fVolFogPower[h][w] = fVolFogPower;
                            tmp_fVolFogColorFactor[h][w] = fVolFogColorFactor;
                            initialized = true;
                        }
                        break;
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
        FillGaps(m_fVolFogColorFactor, tmp_fVolFogColorFactor);
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
        float baseVolFogColorFactor = INTERP(m_fVolFogColorFactor);

        for (const auto& it : currentTimecycleModifiers)
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
            if (modifier.fVolFogColorFactor != -1.0f)
                baseVolFogColorFactor = baseVolFogColorFactor * (1.0f - it.second) + modifier.fVolFogColorFactor * it.second;
        }

        m_fCurrentVolFogDensity = baseVolFogDensity;
        m_fCurrentVolFogHeightFalloff = baseVolFogHeightFalloff;
        m_fCurrentVolFogAltitudeTweak = baseVolFogAltitudeTweak;
        m_fCurrentVolFogPower = baseVolFogPower;
        m_fCurrentVolFogColorFactor = baseVolFogColorFactor;

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
            auto pattern = find_pattern("68 50 79 EA 00", "68 20 74 D7 00");
            static auto CTimeCycleInitialiseHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                static std::vector<std::filesystem::path> episodicPaths = {
                    std::filesystem::path(""),
                    std::filesystem::path("TLAD"),
                    std::filesystem::path("TBoGT"),
                };

                auto filePath1 = GetModulePath(GetModuleHandleW(NULL)).parent_path() / episodicPaths[*_dwCurrentEpisode] / "pc" / "data";
                auto filePath2 = GetModulePath(GetModuleHandleW(NULL)).parent_path() / "pc" / "data";
                CTimeCycleExt::Initialise(filePath1 / "timecycext.dat");
                CTimeCycleModifiersExt::Initialise(filePath2 / "timecyclemodifiersext.dat");
            });

            pattern = find_pattern("E8 ? ? ? ? B9 B8 CE 6D 01 E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8", 
                "E8 ? ? ? ? B9 48 97 18 01 E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8",
                "E8 ? ? ? ? B9 60 0D 00 01 E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8");
            static auto CClockUpdateHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                CTimeCycleExt::Update();
            });

            pattern = hook::pattern("F3 0F 11 15 ? ? ? ? 76");
            if (!pattern.empty())
            {
                static auto OverwriteCurBlendHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    if (!currentTimecycleModifiers.empty())
                    {
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
                    if (!currentTimecycleModifiers.empty())
                    {
                        auto& back = currentTimecycleModifiers.back().second;
                        if (back == 1.0f)
                            back = regs.xmm0.f32[0];
                    }
                });
            }

            pattern = find_pattern("8D B0 ? ? ? ? 6A ? 51", "8D 8E ? ? ? ? D9 1C 24 51 8D 4C 24");
            CTimeCycleModifier::dword_15DE3A8 = *pattern.get_first<decltype(CTimeCycleModifier::dword_15DE3A8)>(2);

            pattern = find_pattern("51 F3 0F 10 64 24 ? 0F 57 F6", "F3 0F 10 4C 24 ? 0F 57 F6 0F 2F CE");
            CTimeCycleModifier::shApplyTimecycModifier = safetyhook::create_inline(pattern.get_first(), CTimeCycleModifier::ApplyTimecycModifier);
            
            pattern = find_pattern("E8 ? ? ? ? F3 0F 10 84 B4 ? ? ? ? 51", "E8 ? ? ? ? D9 44 24 ? 51 8B D1");
            CTimeCycleModifier::hbBlendWithModifier.fun = injector::MakeCALL(pattern.get_first(), CTimeCycleModifier::BlendWithModifier).get();

            pattern = find_pattern("E8 ? ? ? ? 8B 4C 24 ? 8B 54 24 ? 0F 57 C0", "E8 ? ? ? ? 0F 57 C0 0F 2F 86 ? ? ? ? 72");
            CTimeCycleModifier::hbBlendWithModifier.fun = injector::MakeCALL(pattern.get_first(), CTimeCycleModifier::BlendWithModifier2).get();
        };
    }
} TimecycExt;