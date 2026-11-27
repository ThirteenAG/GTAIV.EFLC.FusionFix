module;

#include <common.hxx>
#include <concepts>
#include <mutex>
#include <set>
#include <unordered_set>
#include <unordered_map>

export module shaders;

import common;
import comvars;
import d3dx9_43;
import hdr;
import memory;
import natives;
import seasonal;
import settings;
import shadows;
import timecycext;

template<typename T, typename ... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

template<typename T>
using remove_cvref_t = std::remove_cvref_t<std::remove_pointer_t<std::decay_t<T>>>;

export template <typename T> requires IsAnyOf<remove_cvref_t<T>, IDirect3DPixelShader9, IDirect3DVertexShader9>
int GetFusionShaderID(T pShader)
{
    if (pShader)
    {
        UINT len = 0;
        pShader->GetFunction(nullptr, &len);
        std::vector<uint8_t> pbFunc(len + len % 4);
        pShader->GetFunction(pbFunc.data(), &len);
        static auto sFusionShader = to_bytes("FusionShader");
        auto s = pattern_str(sFusionShader);
        while (s.back() == ' ' || s.back() == '0') s.pop_back();
        auto pattern = hook::range_pattern((uintptr_t)pbFunc.data(), (uintptr_t)pbFunc.data() + pbFunc.size(), s);
        if (!pattern.empty()) {
            auto id = *pattern.get_first<int>(sFusionShader.size() - 1);
            if constexpr (IsAnyOf<remove_cvref_t<T>, IDirect3DVertexShader9>)
                if(id >= 0)
                    id -= 2000;
            return id;
        }
    }
    return -1;
}

// The graphics driver compiles a pair of vertex and pixel shader the first time something is drawn with it, on its
// own thread, and the frame waits for it: 20 to 90 ms whenever a material comes into view for the first time since
// the game was started. The passes of the effects are drawn once with a triangle of no size behind the loading screen
// of a game being loaded: for the first game loaded for as long as it is shown, so nothing is left to compile while
// playing, and for effects loaded later a few at a time and for a limited time (what is left waits for the next loading
// screen). The driver compiles them after the draws, and whatever waits for the driver next (a texture lock, the
// present) waits for all of them: a batch of pairs is drawn only once the driver has finished the one before, so the
// loading screen keeps being shown and the game keeps loading. The first game's batches grow while the driver keeps up
// within a frame and shrink when it doesn't. The effects of the models the streaming has loaded go first, in the order
// they were loaded.
namespace ShaderWarmUp
{
    constexpr uint32_t nFramePairs = 24;            // Per frame of the loading screen
    constexpr uint32_t nLoadingScreenPairs = 600;   // Per loading screen
    constexpr double fLoadingScreenMs = 1500.0;     // What one loading screen is held up by at most
    constexpr double fLoadingFrameMs = 30.0;        // One frame of the loading screen at most
    constexpr uint32_t nFirstBatchPairs = 8;        // The first game's batches start this big and adapt
    constexpr uint32_t nMaxBatchPairs = 512;

    struct Pair
    {
        IDirect3DVertexShader9* pVertexShader;
        IDirect3DPixelShader9* pPixelShader;
    };

    std::mutex Lock;
    std::vector<uintptr_t> Effects;         // Loaded, their pairs not collected yet
    std::vector<uintptr_t> UsedEffects;     // A loaded model has a shader of it, its pairs not moved to the front yet
    std::unordered_set<uintptr_t> Used;

    // Render thread
    std::set<std::pair<IDirect3DVertexShader9*, IDirect3DPixelShader9*>> Known;
    std::unordered_map<uintptr_t, std::vector<Pair>> Pairs;     // By effect
    std::vector<Pair> First;

    SafetyHookInline shInitEffect = {};
    bool __fastcall InitEffect(uintptr_t effect, void* edx, const char* name, int32_t path)
    {
        auto ret = shInitEffect.unsafe_fastcall<bool>(effect, edx, name, path);
        if (ret)
        {
            std::lock_guard lock(Lock);
            Effects.push_back(effect);
        }
        return ret;
    }

    // grmShaderFx made from a resource: the shader of a loaded model, with its effect at 0x18
    SafetyHookInline shLoadShader = {};
    uintptr_t __fastcall LoadShader(uintptr_t shader, void* edx, void* resource)
    {
        auto ret = shLoadShader.unsafe_fastcall<uintptr_t>(shader, edx, resource);
        if (auto effect = *reinterpret_cast<uintptr_t*>(shader + 0x18))
        {
            std::lock_guard lock(Lock);
            if (Used.insert(effect).second)
                UsedEffects.push_back(effect);
        }
        return ret;
    }

    // grcEffect: techniques (16 bytes each, with their passes of 32 bytes), vertex and fragment programs (12 bytes
    // each, the Direct3D shader last); a pass starts with its vertex program's index, its fragment program's is at 12
    template<typename T>
    T* Shader(uintptr_t programs, uint32_t index)
    {
        if (index >= *reinterpret_cast<uint16_t*>(programs + 4))
            return nullptr;
        return *reinterpret_cast<T**>(*reinterpret_cast<uintptr_t*>(programs) + index * 12 + 8);
    }

    // The pairs of the effects loaded since the last time, held until they are drawn, and those of the effects found
    // in use to the front
    void Collect()
    {
        std::vector<uintptr_t> effects, used;
        {
            std::lock_guard lock(Lock);
            effects.swap(Effects);
            used.swap(UsedEffects);
        }
        for (auto effect : effects)
        {
            auto& pairs = Pairs[effect];
            auto techniques = *reinterpret_cast<uintptr_t*>(effect);
            auto techniqueCount = *reinterpret_cast<uint16_t*>(effect + 4);
            for (uint32_t t = 0; t < techniqueCount; t++)
            {
                auto passes = *reinterpret_cast<uintptr_t*>(techniques + t * 16 + 8);
                auto passCount = *reinterpret_cast<uint16_t*>(techniques + t * 16 + 12);
                for (uint32_t p = 0; p < passCount; p++)
                {
                    auto pass = reinterpret_cast<uint32_t*>(passes + p * 32);
                    auto vs = Shader<IDirect3DVertexShader9>(effect + 16, pass[0]);
                    auto ps = Shader<IDirect3DPixelShader9>(effect + 24, pass[3]);
                    if (!vs || !ps || !Known.emplace(vs, ps).second)
                        continue;
                    vs->AddRef();
                    ps->AddRef();
                    pairs.push_back({ vs, ps });
                }
            }
        }
        for (auto effect : used)
        {
            if (auto it = Pairs.find(effect); it != Pairs.end())
            {
                First.insert(First.end(), it->second.begin(), it->second.end());
                Pairs.erase(it);
            }
        }
    }

    bool Next(Pair& pair)
    {
        if (First.empty())
        {
            while (!Pairs.empty() && Pairs.begin()->second.empty())
                Pairs.erase(Pairs.begin());
            if (Pairs.empty())
                return false;
            First.swap(Pairs.begin()->second);
            Pairs.erase(Pairs.begin());
        }
        pair = First.back();
        First.pop_back();
        return true;
    }

    // A loading screen's time is the whole of its frames in which pairs were drawn: the compiling shows in the frame,
    // not in the draws
    int64_t nTicksLeft = 0;
    uint32_t nPairsLeft = 0;
    int64_t nLastFrame = 0;
    uint32_t nLoadedBefore = 0;
    bool bLoading = false;
    bool bDrewLastFrame = false;
    bool bFirstGame = true;

    // The end of the batch drawn last, while the driver works on it
    IDirect3DQuery9* pBatchDone = nullptr;
    uint32_t nPendingPairs = 0;
    int64_t nBatchStart = 0;        // Valid only on the loading screen that issued the query
    uint32_t nBatchPairs = nFirstBatchPairs;

    void Release()
    {
        if (pBatchDone)
            pBatchDone->Release();
        pBatchDone = nullptr;
        nPendingPairs = 0;
        nBatchStart = 0;
    }

    // The driver has finished the last batch: then the time it took sets the size of the next one, for about a frame's
    // time of the loading screen (no more than twice as many as before)
    bool BatchDone(int64_t now, int64_t frequency)
    {
        if (!nPendingPairs)
            return true;
        auto result = pBatchDone->GetData(nullptr, 0, 0);
        if (result == S_FALSE)
            return false;
        if (result == S_OK && nBatchStart)
        {
            auto ms = std::max((now - nBatchStart) * 1000.0 / frequency, 1.0);
            auto next = static_cast<uint32_t>(nPendingPairs * std::min(fLoadingFrameMs / ms, 2.0));
            if (ms < fLoadingFrameMs)
                next = std::max(next, nPendingPairs + 1); // A small batch can round back down to its current size.
            nBatchPairs = std::clamp(next, 1u, nMaxBatchPairs);
        }
        nPendingPairs = 0;
        return true;
    }

    void Draw()
    {
        LARGE_INTEGER now{}, frequency{};
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&frequency);
        auto loading = (CMenuManager::bLoadscreenShown && *CMenuManager::bLoadscreenShown) || bLoadingShown;
        if (loading && !bLoading)
        {
            nTicksLeft = static_cast<int64_t>(fLoadingScreenMs * frequency.QuadPart / 1000.0);
            nPairsLeft = nLoadingScreenPairs;
            nLoadedBefore = LoadingScreenWork::nResourcesLoaded;
        }
        else if (loading && (bDrewLastFrame || (nPendingPairs && nBatchStart)))
            nTicksLeft -= now.QuadPart - nLastFrame;
        else if (!loading && bLoading)
        {
            nBatchStart = 0; // Keep an outstanding query, but stop timing it against this screen.
            if (LoadingScreenWork::nResourcesLoaded - nLoadedBefore >= LoadingScreenWork::nGameResources)
                bFirstGame = false;
        }
        bLoading = loading;
        nLastFrame = now.QuadPart;
        bDrewLastFrame = false;

        Collect();
        if (!BatchDone(now.QuadPart, frequency.QuadPart) || !loading ||
            LoadingScreenWork::nResourcesLoaded - nLoadedBefore < LoadingScreenWork::nGameResources)
            return;
        if (!bFirstGame && (nTicksLeft <= 0 || !nPairsLeft))
            return;

        Pair pair{};
        if (!Next(pair))
            return;

        auto device = rage::grcDevice::GetD3DDevice();
        if (!pBatchDone)
            device->CreateQuery(D3DQUERYTYPE_EVENT, &pBatchDone);
        IDirect3DVertexShader9* vertexShader = nullptr;
        IDirect3DPixelShader9* pixelShader = nullptr;
        IDirect3DVertexDeclaration9* declaration = nullptr;
        IDirect3DVertexBuffer9* stream = nullptr;
        UINT offset = 0, stride = 0;
        device->GetVertexShader(&vertexShader);
        device->GetPixelShader(&pixelShader);
        device->GetVertexDeclaration(&declaration);
        device->GetStreamSource(0, &stream, &offset, &stride);

        static const float triangle[9] = {};
        device->SetFVF(D3DFVF_XYZ);
        uint32_t nDrawn = 0;
        for (;;)
        {
            device->SetVertexShader(pair.pVertexShader);
            device->SetPixelShader(pair.pPixelShader);
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, sizeof(float) * 3);
            pair.pVertexShader->Release();
            pair.pPixelShader->Release();
            if (nPairsLeft)
                nPairsLeft--;
            if (++nDrawn >= (bFirstGame ? nBatchPairs : nFramePairs) || (!bFirstGame && !nPairsLeft))
                break;
            LARGE_INTEGER drawn{};
            QueryPerformanceCounter(&drawn);
            if (drawn.QuadPart - now.QuadPart >= static_cast<int64_t>(fLoadingFrameMs * frequency.QuadPart / 1000.0))
                break;
            if (!Next(pair))
                break;
        }
        bDrewLastFrame = true;
        if (pBatchDone && SUCCEEDED(pBatchDone->Issue(D3DISSUE_END)))
        {
            nPendingPairs = nDrawn;
            nBatchStart = now.QuadPart;
        }

        device->SetVertexDeclaration(declaration);
        device->SetStreamSource(0, stream, offset, stride);
        device->SetVertexShader(vertexShader);
        device->SetPixelShader(pixelShader);
        if (declaration)
            declaration->Release();
        if (stream)
            stream->Release();
        if (vertexShader)
            vertexShader->Release();
        if (pixelShader)
            pixelShader->Release();
    }
}

class Shaders
{
    static void OnBeforeLighting()
    {
        auto pDevice = rage::grcDevice::GetD3DDevice();

        {
            static auto AzimuthColorIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "AzimuthColor");
            static auto AzimuthHeightIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "AzimuthHeight");
            auto& AzimuthColor = rage::grmShaderInfo::getShaderParamData(AzimuthColorIdx);
            auto& AzimuthHeight = rage::grmShaderInfo::getShaderParamData(AzimuthHeightIdx);

            static float arr[4];
            arr[0] = AzimuthColor[0];
            arr[1] = AzimuthColor[1];
            arr[2] = AzimuthColor[2];
            arr[3] = AzimuthHeight[0];

            pDevice->SetPixelShaderConstantF(212, &arr[0], 1);
            pDevice->SetVertexShaderConstantF(228, &arr[0], 1);
        }

        {
            static auto AzimuthColorEastIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "AzimuthColorEast");
            static auto AzimuthStrengthIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "AzimuthStrength");
            auto& AzimuthColorEast = rage::grmShaderInfo::getShaderParamData(AzimuthColorEastIdx);
            auto& AzimuthStrength = rage::grmShaderInfo::getShaderParamData(AzimuthStrengthIdx);

            static float arr[4];
            arr[0] = AzimuthColorEast[0];
            arr[1] = AzimuthColorEast[1];
            arr[2] = AzimuthColorEast[2];
            arr[3] = AzimuthStrength[0];

            pDevice->SetPixelShaderConstantF(213, &arr[0], 1);
            pDevice->SetVertexShaderConstantF(229, &arr[0], 1);
        }

        {
            static auto SkyColorIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "SkyColor");
            static auto HDRExposureIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "HDRExposure");
            auto& SkyColor = rage::grmShaderInfo::getShaderParamData(SkyColorIdx);
            auto& HDRExposure = rage::grmShaderInfo::getShaderParamData(HDRExposureIdx);

            static float arr[4];
            arr[0] = SkyColor[0];
            arr[1] = SkyColor[1];
            arr[2] = SkyColor[2];
            arr[3] = HDRExposure[0];

            pDevice->SetPixelShaderConstantF(214, &arr[0], 1);
            pDevice->SetVertexShaderConstantF(234, &arr[0], 1);
        }

        {
            static auto SunColorIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "SunColor");
            static auto HDRSunExposureIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "HDRSunExposure");
            auto& SunColor = rage::grmShaderInfo::getShaderParamData(SunColorIdx);
            auto& HDRSunExposure = rage::grmShaderInfo::getShaderParamData(HDRSunExposureIdx);

            static float arr[4];
            arr[0] = SunColor[0];
            arr[1] = SunColor[1];
            arr[2] = SunColor[2];
            arr[3] = HDRSunExposure[0];

            pDevice->SetPixelShaderConstantF(215, &arr[0], 1);
            pDevice->SetVertexShaderConstantF(231, &arr[0], 1);
        }

        {
            static auto SunDirectionIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "SunDirection");
            auto& SunDirection = rage::grmShaderInfo::getShaderParamData(SunDirectionIdx);

            static float arr[4];
            arr[0] = SunDirection[0];
            arr[1] = (-1.0f * SunDirection[2]);
            arr[2] = SunDirection[1];
            arr[3] = 0.0f;

            pDevice->SetPixelShaderConstantF(216, &arr[0], 1);
            pDevice->SetVertexShaderConstantF(232, &arr[0], 1);
        }
    }

public:
    Shaders()
    {
        FusionFix::onInitEvent() += []()
        {
            auto pattern = hook::pattern("81 EC 04 01 00 00 53 55 56 57 8B F9 E8 ? ? ? ? 8B 9C 24 18 01 00 00 85 DB 0F 84");
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
            {
                ShaderWarmUp::shInitEffect = safetyhook::create_inline(pattern.get_first(0), ShaderWarmUp::InitEffect);
                pattern = hook::pattern("81 EC 04 04 00 00 53 56 57 8B F1 E8 ? ? ? ? 8B 9C 24 14 04 00 00 53 8D 4E 14 C7 06");
                ShaderWarmUp::shLoadShader = safetyhook::create_inline(pattern.get_first(0), ShaderWarmUp::LoadShader);

                FusionFix::onEndScene() += []()
                {
                    ShaderWarmUp::Draw();
                };

                FusionFix::onBeforeReset() += []()
                {
                    ShaderWarmUp::Release();
                };
            }
        };

        static rage::grcRenderTargetPC* pHDRTexQuarter = nullptr;
        static float fOverrideTreeAlpha = 0.0f;

        static float fSHADOWFILTERSHARPShadowSoftness = 1.5f;
        static float fSHADOWFILTERSHARPShadowBias = 5.0f;

        static float fSHADOWFILTERSOFTShadowSoftness = 3.0f;
        static float fSHADOWFILTERSOFTShadowBias = 8.0f;

        static float fSHADOWFILTERCHSSShadowSoftness = 1.5f;
        static float fSHADOWFILTERCHSSShadowBias = 5.0f;
        static float fSHADOWFILTERCHSSMaxSoftness = 10.0f;

        static float fSSDensity = 0.9f;
        static float fSSDecay = 0.95f;

        static float fCascadeBlendSize = 0.1f;

        static float fMaxPQValue = 100.0f;

        static int nForceShadowFilter = 0;

        static bool bSmoothShorelines = true;

        static bool bSmoothLightVolumes = true;

        static bool bNoBloomColorShift = true;

        static auto bNoWindSway = false;

        FusionFix::onInitEvent() += []()
        {
            SettingsReader reader;
            fOverrideTreeAlpha = std::clamp(reader.ReadFloat("MISC", "OverrideTreeAlpha", 0.0f), 0.0f, 255.0f);
            fSHADOWFILTERSHARPShadowSoftness = reader.ReadFloat("SHADOWFILTERSHARP", "ShadowSoftness", 1.5f);
            fSHADOWFILTERSHARPShadowBias = reader.ReadFloat("SHADOWFILTERSHARP", "ShadowBias", 5.0f);
            fSHADOWFILTERSOFTShadowSoftness = reader.ReadFloat("SHADOWFILTERSOFT", "ShadowSoftness", 3.0f);
            fSHADOWFILTERSOFTShadowBias = reader.ReadFloat("SHADOWFILTERSOFT", "ShadowBias", 8.0f);

            fSHADOWFILTERCHSSShadowSoftness = reader.ReadFloat("SHADOWFILTERCHSS", "ShadowSoftness", 1.5f);
            fSHADOWFILTERCHSSShadowBias = reader.ReadFloat("SHADOWFILTERCHSS", "ShadowBias", 5.0f);
            fSHADOWFILTERCHSSMaxSoftness = reader.ReadFloat("SHADOWFILTERCHSS", "MaxSoftness", 10.0f);

            fSSDensity = std::clamp(reader.ReadFloat("SUNSHAFTS", "SunShaftsDensity", 0.9f), 0.0f, 1.0f);
            fSSDecay = std::clamp(reader.ReadFloat("SUNSHAFTS", "SunShaftsDecay", 0.95f), 0.0f, 1.0f);

            fCascadeBlendSize = std::clamp(reader.ReadFloat("SHADOWS", "CascadeBlendSize", 0.1f), 0.0f, 1.0f);
            nForceShadowFilter = std::clamp(reader.ReadInteger("SHADOWS", "ForceShadowFilter", 0), 0, 2);
            bool bConsoleCarReflectionsAndDirt = reader.ReadInteger("MISC", "ConsoleCarReflectionsAndDirt", 1) != 0;
            bSmoothShorelines = reader.ReadInteger("MISC", "SmoothShorelines", 1) != 0;
            bSmoothLightVolumes = reader.ReadInteger("MISC", "SmoothLightVolumes", 1) != 0;

            bNoBloomColorShift = reader.ReadInteger("MISC", "NoBloomColorShift", 1) != 0;
            fMaxPQValue = std::max(reader.ReadFloat("MISC", "MaxPQValue", 100.0f), 0.0000001f);

            // Redirect path to one unified folder
            auto pattern = hook::pattern("8B 04 8D ? ? ? ? A3 ? ? ? ? 8B 44 24 04");
            if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
            {
                static auto off_1045520 = *pattern.get_first<const char**>(3);
                struct ShaderPathHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.ecx = 0;
                        *(const char**)&regs.eax = *off_1045520;
                    }
                }; injector::MakeInline<ShaderPathHook>(pattern.get_first(0), pattern.get_first(7));
            }
            else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
            {
                pattern = hook::pattern("8B 14 85 ? ? ? ? A3 ? ? ? ? 8B 44 24 04");
                static auto off_1045520 = *pattern.get_first<const char**>(3);
                struct ShaderPathHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.eax = 0;
                        *(const char**)&regs.edx = *off_1045520;
                    }
                }; injector::MakeInline<ShaderPathHook>(pattern.get_first(0), pattern.get_first(7));
            }

            // Redirect common\shaders\win32_30\rage_perlinnoise.fxc
            {
                std::ifstream is(GetModulePath(GetModuleHandleW(NULL)).parent_path() / "common" / "shaders" / "win32_30" / "rage_perlinnoise.fxc", std::ios::binary);
                if (is)
                {
                    static std::string rage_perlinnoise((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());

                    // Each embedded shader registers a static { name, data } entry; the entries and their names are
                    // in the exe's image, other stored values are not
                    auto image = reinterpret_cast<uintptr_t>(GetModuleHandleW(NULL));
                    auto imageEnd = image + reinterpret_cast<IMAGE_NT_HEADERS*>(image + reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew)->OptionalHeader.SizeOfImage;
                    auto inImage = [&](const void* p) { return reinterpret_cast<uintptr_t>(p) >= image && reinterpret_cast<uintptr_t>(p) < imageEnd; };
                    static constexpr char name[] = "win32_30/rage_perlinnoise.fxc";
                    void** entry = nullptr;
                    auto pattern = hook::pattern("A1 ? ? ? ? A3 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? C3");
                    for (size_t i = 0; i < pattern.size() && !entry; ++i)
                    {
                        auto candidate = *pattern.get(i).get<void**>(16);
                        if (inImage(candidate) && inImage(candidate[0]) && std::strncmp(static_cast<const char*>(candidate[0]), name, sizeof(name)) == 0)
                            entry = candidate;
                    }
                    injector::WriteMemory(&entry[1], rage_perlinnoise.data(), true);
                }
            }

            // Actually read the rain lighting settings in the visualsettings.dat
            {
                auto pattern = hook::pattern("8D 44 24 2C 50 FF 33 F3 0F 11 44 24 ? 56 E8");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(3), 0x34, true);
                    pattern = hook::pattern("8D 44 24 2C 50 FF 73 04 F3 0F 11 44 24 ? 56 E8");
                    injector::WriteMemory<uint8_t>(pattern.get_first(3), 0x30, true);
                    pattern = hook::pattern("8D 44 24 2C 50 FF 73 08 F3 0F 11 44 24 ? 56 E8");
                    injector::WriteMemory<uint8_t>(pattern.get_first(3), 0x38, true);
                    pattern = hook::pattern("8D 44 24 2C 50 FF 73 0C F3 0F 11 44 24 ? 56 E8");
                    injector::WriteMemory<uint8_t>(pattern.get_first(3), 0x3C, true);
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 53 56 57 8B 7C 24 10 6A 05");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    pattern = hook::pattern("F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? 68 ? ? ? ? F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? 50");
                    injector::MakeNOP(pattern.get_first(0), 14, true);
                    pattern = hook::pattern("F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? 50 F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? 8D 5F 14");
                    injector::MakeNOP(pattern.get_first(0), 14, true);
                    pattern = hook::pattern("F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? 8D 5F 14 53 F3 0F 11 44 24");
                    injector::MakeNOP(pattern.get_first(0), 14, true);
                    pattern = hook::pattern("F3 0F 11 44 24 ? E8 ? ? ? ? 8B 0D ? ? ? ? 8B 56 10");
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                }
            }

            // Override reflection multiplier that resulted in weaker reflections on PC, a fix/workaround used to be in place on the shader side, move it in here instead.
            {
                static float dw103F984 = 1.0f; // 0.6f -> 1.0f
                auto pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 59 C1 51 F3 0F 11 04 24 E8", "F3 0F 10 0D ? ? ? ? 51 F3 0F 59 C8 F3 0F 11 0C 24 E8");
                injector::WriteMemory(pattern.get_first(4), &dw103F984, true);
            }

            // Restore console car reflections and dirt level settings. Any car on console could have dirt when they would spawn while on PC some cars _always_ spawn fully cleaned.
            if (bConsoleCarReflectionsAndDirt)
            {
                auto pattern = find_pattern("75 0C C7 87 ? ? ? ? ? ? ? ? EB 20 66 0F 6E C2 F3 0F E6 C0 C1 EA 1F F2 0F 58 04 D5", "75 0D 0F 57 C0 F3 0F 11 86 ? ? ? ? EB 18 85 D2 89 54 24 10 DB 44 24 10 7D 06 D8 05");
                injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                pattern = find_pattern("75 37 C7 47 ? ? ? ? ? C7 87 ? ? ? ? ? ? ? ? C7 87", "75 3A F3 0F 10 05 ? ? ? ? F3 0F 11 46 ? F3 0F 11 86");
                injector::MakeNOP(pattern.get_first(0), 2, true);
            }

            // Adjust mirror plane offset, fixes stuff like the graffiti decals not appearing on the mirror in the Middle Park public toilet.
            {
                static float dwMirrorOffset = 0.0125f; // 0.035 (PC) -> 0.0125 (Xbox)
                auto pattern = find_pattern("F3 0F 10 15 ? ? ? ? 0F 28 CC F3 0F 59 CB", "F3 0F 10 0D ? ? ? ? 8B 0D ? ? ? ? 0F 28 D0 F3 0F 59 D3");
                injector::WriteMemory(pattern.get_first(4), &dwMirrorOffset, true);
            }

            // Contrast slider ticks 0 and 1 are the same visually on the Xbox 360 version. This is not proper behavior, so it's a bug, but it was never fixed for that version,
            // so we need to enforce this behavior to have faithful Xbox 360 gamma. On PC and PS3, all ticks on the slider correctly change the contrast/gamma.
            {
                auto pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 59 C6 F3 0F 11 4C 24", "F3 0F 10 05 ? ? ? ? F3 0F 59 C6 F3 0F 11 44 24 ? F3 0F 10 05");
                static auto rage__CPostFX__sm_contrastFrontEnd = *pattern.get_first<float*>(4);
                injector::MakeNOP(pattern.get_first(0), 12, true);
                static auto rage__CPostFX__ProcessPostProcess_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    regs.xmm0.f32[0] = *rage__CPostFX__sm_contrastFrontEnd;

                    static auto ConsoleGamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
                    if (ConsoleGamma->get() == 1 && regs.xmm0.f32[0] < 1.3f)
                        regs.xmm0.f32[0] += 0.06f;

                    regs.xmm0.f32[0] *= regs.xmm6.f32[0];
                });
            }

            // Force the water surface render target resolution to always be 256x256. This matches the water tiling on the console versions.
            {
                static uint32_t dwWaterQuality = 1; // MO_MED?

                auto pattern = find_pattern("8B 0D ? ? ? ? 53 BB ? ? ? ? D3 E3 85 D2 0F 85", "8B 0D ? ? ? ? BF ? ? ? ? D3 E7 85 C0 0F 85");
                injector::WriteMemory(pattern.get_first(2), &dwWaterQuality, true); // MO_LOW?

                pattern = find_pattern("8B 0D ? ? ? ? F3 0F 10 0D ? ? ? ? B8 ? ? ? ? D3 E0 8B 0D", "8B 0D ? ? ? ? F3 0F 10 05 ? ? ? ? 6A 02 6A 01 BA");
                injector::WriteMemory(pattern.get_first(2), &dwWaterQuality, true); // MO_HIGH?

                pattern = find_pattern("8B 0D ? ? ? ? BE ? ? ? ? D3 E6 83 3D", "8B 0D ? ? ? ? F3 0F 11 0D ? ? ? ? F3 0F 10 0D");
                injector::WriteMemory(pattern.get_first(2), &dwWaterQuality, true); // MO_VHIGH?
            }

            // z-fighting fix helpers
            {
                auto pattern = find_pattern("39 35 ? ? ? ? 75 ? 8B CE E8 ? ? ? ? 5E 8B E5 5D C3", "? 39 35 ? ? ? ? 75 ? 56 E8 ? ? ? ? 8B E5");
                static auto grcViewPortUpdateTransformHook = safetyhook::create_mid(pattern.get_first(10), [](SafetyHookContext& regs)
                {
                    auto pDevice = rage::grcDevice::GetD3DDevice();

                    if (pDevice)
                    {
                        auto viewport = rage::GetCurrentViewport();
                        if (viewport)
                        {
                            static float arr[4];
                            static float cachedNearClip = 0.0f;
                            static float cachedFarClip = 0.0f;
                            static float cachedLog2Value = 0.0f;

                            arr[0] = 1.0f / viewport->mNearClip;

                            // Only recalculate log2 if clip planes have changed
                            if (viewport->mNearClip != cachedNearClip || viewport->mFarClip != cachedFarClip)
                            {
                                cachedNearClip = viewport->mNearClip;
                                cachedFarClip = viewport->mFarClip;
                                cachedLog2Value = 1.0f / log2(viewport->mFarClip / viewport->mNearClip);
                            }

                            arr[1] = cachedLog2Value;
                            arr[2] = viewport->mFarClip / viewport->mNearClip;
                            arr[3] = viewport->mNearClip;

                            pDevice->SetVertexShaderConstantF(227, &arr[0], 1);
                            pDevice->SetPixelShaderConstantF(209, &arr[0], 1);
                        }
                    }
                });
            }
        };

        // Installed before the render thread starts (it runs BeginDraw), active once the game has initialized
        static std::atomic<bool> bGameInitialized{ false };
        FusionFix::onGameInitEvent() += []()
        {
            bGameInitialized = true;
        };

        FusionFix::onInitEvent() += []()
        {
            static auto dw103E49C = *find_pattern("8B 0D ? ? ? ? 8B 01 FF 50 ? B9 ? ? ? ? E9", "8B 0D ? ? ? ? 8B 11 8B 42 ? FF D0 B9").get_first<void**>(2);
            auto pattern = find_pattern("80 7C 24 ? ? 74 ? 80 BE ? ? ? ? ? 74 ? 8B 0D", "80 7C 24 ? ? 74 ? 80 BE ? ? ? ? ? 74 ? A1");
            static auto grcSetup_BeginDraw_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                if (!bGameInitialized)
                    return;

                auto pDevice = rage::grcDevice::GetD3DDevice();

                // Setup variables for shaders
                auto bLoadscreenActive = (CMenuManager::bLoadscreenShown && *CMenuManager::bLoadscreenShown) || bLoadingShown;

                if (*dw103E49C && !bLoadscreenActive)
                {
                    // DynamicShadowsForTrees wind sway
                    {
                        static float arr2[4];

                        arr2[0] = (Natives::IsInteriorScene() || bNoWindSway) ? 0.0f : *CTreeImposters::ms_windAng;
                        arr2[1] = SeasonalManager::GetCurrent() == SeasonalType::Snow ? 0.005f : std::clamp(*CTimer::m_fTimeScale * 0.015f, 0.0015f, 0.015f);
                        arr2[2] = 0.0f;
                        arr2[3] = 0.0f;

                        pDevice->SetVertexShaderConstantF(233, &arr2[0], 1);
                    }

                    // Shadow Ini Settings
                    {
                        static float arr7[4];
                        static float arr9[4];
                        static auto shadowFilter = FusionFixSettings.GetRef("PREF_SHADOWFILTER");

                        if (shadowFilter->get() == FusionFixSettings.ShadowFilterText.eSoft)
                        {
                            arr7[0] = fSHADOWFILTERSOFTShadowSoftness;
                            arr7[1] = fSHADOWFILTERSOFTShadowBias;
                        }
                        else if (shadowFilter->get() == FusionFixSettings.ShadowFilterText.eSharp)
                        {
                            arr7[0] = fSHADOWFILTERSHARPShadowSoftness;
                            arr7[1] = fSHADOWFILTERSHARPShadowBias;
                        }
                        else //eCHSS
                        {
                            arr7[0] = fSHADOWFILTERCHSSShadowSoftness;
                            arr7[1] = fSHADOWFILTERCHSSShadowBias;
                        }

                        static auto sq = FusionFixSettings.GetRef("PREF_SHADOW_QUALITY");
                        if (sq->get() >= 4) // Very High
                            arr7[2] = shadowFilter->get() == FusionFixSettings.ShadowFilterText.eCHSS ? 1.0f : 0.0f;
                        else
                            arr7[2] = 0.0f;

                        arr7[3] = fCascadeBlendSize;

                        pDevice->SetPixelShaderConstantF(218, &arr7[0], 1);

                        arr9[0] = bHighResolutionShadows ? fSHADOWFILTERCHSSMaxSoftness * 2.0f : fSHADOWFILTERCHSSMaxSoftness;
                        arr9[1] = bHighResolutionShadows ? CTimeCycleExt::GetCHSSLightSize() * 2.0f : CTimeCycleExt::GetCHSSLightSize();

                        // SDR tone mapping would clip the highlights HDR output keeps
                        static auto tm = FusionFixSettings.GetRef("PREF_TONEMAPPING");
                        arr9[2] = HDROutput::IsActive() ? 0.0f : static_cast<float>(tm->get());

                        arr9[3] = bNoBloomColorShift && tm->get() ? 1.0f : 0.0f;



                        pDevice->SetPixelShaderConstantF(217, &arr9[0], 1);

                        SetCascadeTileBounds(pDevice);
                    }

                    // Water reflection half-pixel offset
                    {
                        static float arr[4];
                        static auto wq = FusionFixSettings.GetRef("PREF_WATER_QUALITY");
                        switch (wq->get())
                        {
                            case 0:
                                arr[0] = (0.5f / 160.0f);
                                arr[1] = (0.5f / 64.0f);
                                break;
                            case 1:
                                arr[0] = (0.5f / 320.0f);
                                arr[1] = (0.5f / 128.0f);
                                break;
                            case 2:
                                arr[0] = (0.5f / 640);
                                arr[1] = (0.5f / 256.0f);
                                break;
                            case 3:
                            default:
                                arr[0] = (0.5f / 1280.0f);
                                arr[1] = (0.5f / 512.0f);
                                break;
                        }

                        static auto unclamplighting = FusionFixSettings.GetRef("PREF_UNCLAMPLIGHTING");

                        arr[2] = unclamplighting->get() ? 1.0f : 4.0f / 3.0f;
                        arr[3] = unclamplighting->get() ? 0.0f : -1.0f / 3.0f;

                        pDevice->SetPixelShaderConstantF(220, &arr[0], 1);
                    }

                    // Current Settings
                    {
                        static float arr5[4];
                        static auto treealpha = FusionFixSettings.GetRef("PREF_TREEALPHA");

                        if (nForceShadowFilter == 0)
                        {
                            static auto definition = FusionFixSettings.GetRef("PREF_DEFINITION");
                            arr5[0] = static_cast<float>(definition->get());
                        }
                        else if (nForceShadowFilter == 1)
                        {
                            arr5[0] = 0.0f;
                        }
                        else if (nForceShadowFilter == 2)
                        {
                            arr5[0] = 1.0f;
                        }

                        arr5[1] = SeasonalManager::GetCurrent() == SeasonalType::Snow ? 1.0f : 0.0f;
                        arr5[2] = fMenuBlur;

                        static float alphamul = 4.0f;
                        if (fOverrideTreeAlpha == 0.0f)
                        {
                            switch (treealpha->get())
                            {
                            case FusionFixSettings.TreeFxText.ePC: alphamul = 0.625f; break;
                            case FusionFixSettings.TreeFxText.ePCPlus: alphamul = 1.0f; break;
                            case FusionFixSettings.TreeFxText.eConsole: alphamul = 4.0f; break;
                            }
                        }
                        else
                            alphamul = fOverrideTreeAlpha;

                        arr5[3] = alphamul;
                        pDevice->SetPixelShaderConstantF(221, &arr5[0], 1);
                    }

                    // FXAA, Gamma
                    {
                        //static auto gamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
                        static auto mblur = FusionFixSettings.GetRef("PREF_MOTIONBLUR");
                        static auto ae = FusionFixSettings.GetRef("PREF_AUTOEXPOSURE");
                        static float arr3[4];
                        arr3[0] = (ae->get() ? 1.0f / 9.0f : 1.0f / 16.0f);
                        arr3[1] = (bSmoothShorelines ? 1.0f : 0.0f);
                        //arr3[2] = static_cast<float>(gamma->get());
                        static float mblurscale = 1.0f;
                        switch(mblur->get())
                        {
                            case 0: mblurscale = 0.0f; break;
                            case 1: mblurscale = 0.125f; break;
                            case 2: mblurscale = 0.25f; break;
                            case 3: mblurscale = 0.5f; break;
                            case 4: mblurscale = 1.0f; break;
                            default: mblurscale = 1.0f; break;
                        }
                        arr3[3] = mblurscale / (30.0f * Natives::Timestep());
                        pDevice->SetPixelShaderConstantF(222, &arr3[0], 1);
                    }

                    // Cascaded Shadow Map Res, Time of Day, Tree Translucency
                    {
                        static Cam cam = 0;
                        Natives::GetRootCam(&cam);
                        if (cam)
                        {
                            float fov = 0.0f;
                            uint32_t hour = -1;
                            uint32_t minute = -1;
                            Natives::GetTimeOfDay(&hour, &minute);
                            Natives::GetCamFov(cam, &fov);
                            static auto tree_lighting = FusionFixSettings.GetRef("PREF_TREE_LIGHTING");
                            static auto shadowFilter = FusionFixSettings.GetRef("PREF_SHADOW_FILTER");
                            static auto definition = FusionFixSettings.GetRef("PREF_DEFINITION");
                            static float arr4[4];
                            arr4[0] = static_cast<float>(tree_lighting->get() - FusionFixSettings.TreeFxText.ePC);
                            arr4[1] = (fov > 45.0f) ? pow((fov / 45.0f), 1.25f) : 1.0f;
                            arr4[2] = static_cast<float>(definition->get());
                            arr4[3] = (((hour == 6 && minute >= 45) || (hour > 6)) && ((hour == 19 && minute < 15) || (hour < 19))) ? 0.0f : 1.0f;
                            pDevice->SetPixelShaderConstantF(223, &arr4[0], 1);
                        }
                    }

                    {
                        static auto fog = FusionFixSettings.GetRef("PREF_VOLUMETRICFOG");

                        static float arr10[4];
                        arr10[0] = std::max(CTimeCycleExt::GetVolFogDensity(), 0.0f);
                        arr10[1] = std::max(CTimeCycleExt::GetVolFogHeightFalloff(), 0.0000001f);
                        arr10[2] = std::max(CTimeCycleExt::GetVolFogPower(), 0.0f);
                        arr10[3] = std::clamp(CTimeCycleExt::GetVolFogAltitudeTweak(), 0.0f, 1.0f);
                        pDevice->SetPixelShaderConstantF(211, &arr10[0], 1);
                        pDevice->SetVertexShaderConstantF(235, &arr10[0], 1);

                        static float arr11[4];
                        arr11[0] = 1.0f / fMaxPQValue;
                        arr11[1] = static_cast<float>(fog->get());
                        arr11[2] = bSmoothLightVolumes ? 1.0f : 0.0f;
                        arr11[3] = 0.0f;

                        if (bIsQUB3D)
                            arr11[1] = 0.0f;

                        static float arr12[4];
                        arr12[0] = CTimeCycleExt::GetDirLightColorR();
                        arr12[1] = CTimeCycleExt::GetDirLightColorG();
                        arr12[2] = CTimeCycleExt::GetDirLightColorB();
                        arr12[3] = CTimeCycleExt::GetDirLightMultiplier();

                        pDevice->SetPixelShaderConstantF(210, &arr11[0], 1);
                        pDevice->SetVertexShaderConstantF(236, &arr11[0], 1);
                        pDevice->SetVertexShaderConstantF(237, &arr12[0], 1);

                        static float arr13[4];
                        arr13[0] = std::max(CTimeCycleExt::GetSSIntensity(), 0.0f);
                        arr13[1] = fSSDensity;
                        arr13[2] = fSSDecay;
                        arr13[3] = 0.0f;

                        pDevice->SetPixelShaderConstantF(208, &arr13[0], 1);
                    }
                }
            });
        };

        FusionFix::onInitEventAsync() += []()
        {
            // Skip SetRenderState D3DRS_ADAPTIVETESS_* entirely (silences warnings in DXVK logs)
            auto pattern = find_pattern("74 24 F3 0F 10 44 24 ? 51 8D 44 24 44", "74 1C D9 44 24 14 51 8D 4C 24 44");
            injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);

            pattern = find_pattern("74 0A 6A 00 E8 ? ? ? ? 83 C4 04 8B 7C 24 1C 83 EF 80", "74 0A 6A 00 E8 ? ? ? ? 83 C4 04 8B 5C 24 20 81 C6");
            injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);

            //rain drop refraction fix
            pattern = find_pattern("E8 ? ? ? ? 8B 5E 74 8B 46 0C", "E8 ? ? ? ? 8B 4D 50 85 C9 5F 5E");
            static auto grcEffect__SetTexture = static_cast<void(__thiscall*)(uintptr_t, uintptr_t, uint32_t, rage::grcTexturePC*)>(injector::GetBranchDestination(pattern.get_first(0)).get());

            pattern = find_pattern("55 8B EC 83 E4 F8 83 EC 18 56 57 8B F9 83 7F 04 00", "55 8B EC 83 E4 F8 83 EC 10 56 8B F0");
            static auto stackSize = *pattern.get_first<uint8_t>(8);
            static auto SetCommonParticleVarsHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                if (!pHDRTexQuarter)
                {
                    pHDRTexQuarter = rage::grcTextureFactoryPC::GetRTByName("FullScreenTex_temp1");
                }

                uintptr_t thisPtr;
                if (stackSize == 0x10)
                {
                    thisPtr = regs.eax;
                }
                else
                {
                    thisPtr = regs.ecx;
                }

                uint32_t frameMapVar = *(uint32_t*)(thisPtr + 0xA0);
                uintptr_t shaderFx = *(uintptr_t*)(thisPtr + 0x4);
                uintptr_t instanceData = shaderFx + 0x14;
                uintptr_t effect = *(uintptr_t*)(instanceData + 0x4);

                if (frameMapVar)
                {
                    grcEffect__SetTexture(effect, instanceData, frameMapVar, reinterpret_cast<rage::grcTexturePC*>(pHDRTexQuarter));
                }
            });

            FusionFix::onGameInitEvent() += []()
            {
                if (CText::hasViceCityStrings())
                {
                    bNoWindSway = true;
                }
            };

            FusionFix::onBeforeReset() += []()
            {
                pHDRTexQuarter = nullptr;
            };

            if (GetD3DX9_43DLL())
            {
                CRenderPhaseDeferredLighting_LightsToScreen::OnBuildRenderList() += []()
                {
                    auto mBeforeLightingCB = new T_CB_Generic_NoArgs(OnBeforeLighting);
                    if (mBeforeLightingCB)
                        mBeforeLightingCB->Append();
                };
            }
        };
    };
} Shaders;