module;

#include <common.hxx>
#include <cmath>

export module cutscenecam;

import common;
import comvars;
import natives;
import settings;

// Observe cuts only after the final camera evaluation has been accepted.
namespace CutsceneCuts
{
    struct CameraEvaluation
    {
        uintptr_t camera;
        float phase;
    };

    std::vector<CameraEvaluation> cameraEvaluations;
    uint32_t cameraEvaluationFrame = 0;
    uintptr_t acceptedCamera = 0;
    uintptr_t pendingCameraCut = 0;
    uint32_t pendingCameraCutFrame = 0;
    void* acceptedAnimation = nullptr;
    int32_t acceptedSection = -1;
    float acceptedPhase = 0.0f;
    int32_t acceptedMarkerFrame = 0;
    void* (__thiscall* CCamAnimated__GetAnimation)(void*) = nullptr;
    void* (__thiscall* rage__crAnimation__FindTrack)(void*, uint8_t, uint16_t) = nullptr;
    bool (__thiscall* rage__crAnimation__CompositeDof)(void*, float, void*, bool, bool) = nullptr;
    uintptr_t crFrameDofFloatVtable = 0;
    void* cameraManager = nullptr;
    uintptr_t (__thiscall* CCameraManager__GetFinalEffectiveLeafNode)(void*) = nullptr;
    // The PC implementations do not use the CCamInterface receiver.
    bool (__cdecl* CCamInterface__IsLookingBehind)() = nullptr;
    void* (__thiscall* CCameraManager__GetCutsceneCam)(void*) = nullptr;
    float (__thiscall* CCamCutscene__GetCurrentCameraPhaseInUse)(void*, int32_t) = nullptr;
    int32_t* pCutsceneSection = nullptr;
    bool* pCutsceneFullyStreamed = nullptr;
    void (__cdecl* CCutsceneManager__UpdateLights)(float) = nullptr;

    void ObserveJumpCut(uintptr_t camera, float phase, uint32_t result)
    {
        auto frame = *CTimer::m_frameCount;
        if (cameraEvaluationFrame != frame)
        {
            cameraEvaluations.clear();
            cameraEvaluationFrame = frame;
        }
        if (result == 3)
            cameraEvaluations.push_back({ camera, phase });
    }

    bool SampleCutMarker(void* animation, int32_t frame)
    {
        // Same crFrameDofFloat layout, track and nearest-frame sampling as CCamAnimated::IsJumpCut.
        struct
        {
            uintptr_t vtable;
            uint32_t track;
            uint32_t pad[2];
            float value[4];
        } dof = { crFrameDofFloatVtable, 0x9012, {}, {} };
        return rage__crAnimation__CompositeDof(animation, float(frame) / 30.0f, &dof, true, true)
            && dof.value[0] == 1.0f;
    }

    export bool DetectCut()
    {
        auto frame = *CTimer::m_frameCount;
        auto state = *CCutsceneManager::ms_State;
        auto section = *pCutsceneSection;
        auto cutscene = state >= 8 && state <= 11 ? CCameraManager__GetCutsceneCam(cameraManager) : nullptr;
        // CCamCutscene owns 15 section cameras. GetCurrentCameraPhaseInUse reads this child and its phase at +0x144.
        auto camera = cutscene && section >= 0 && section < 15 ? ((uintptr_t*)cutscene)[80 + section] : 0;
        auto animation = camera ? CCamAnimated__GetAnimation((void*)camera) : nullptr;
        auto phase = camera ? *(float*)(camera + 0x144) : -1.0f;
        auto duration = animation ? *(float*)((uintptr_t)animation + 0xC) : 0.0f;
        if (!animation || !std::isfinite(phase) || phase < 0.0f || phase > 1.0f
            || !std::isfinite(duration) || duration <= 0.0f || duration * 30.0f >= float(INT32_MAX))
        {
            acceptedCamera = 0;
            acceptedAnimation = nullptr;
            cameraEvaluations.clear();
            return false;
        }

        bool animatedCut = false;
        for (const auto& evaluation : cameraEvaluations)
        {
            if (evaluation.camera != camera || cameraEvaluationFrame != frame)
                continue;
            // A jitter wrapper may roll a candidate back. Only retain the native release for the accepted phase.
            animatedCut |= evaluation.phase == phase;
        }

        auto markerFrame = int32_t(std::floor((duration * phase) * 30.0f));
        auto entry = camera != acceptedCamera || animation != acceptedAnimation || section != acceptedSection;
        auto rewind = !entry && phase < acceptedPhase;
        // A seek/hitch spanning over a second already invalidates the previous view; avoid scanning an entire scene.
        auto seek = !entry && markerFrame - acceptedMarkerFrame > 30;
        auto hasTrack = rage__crAnimation__FindTrack(animation, 0x90, 0) != nullptr;
        bool crossedMarker = false;
        if (hasTrack && !entry && !rewind && !seek && markerFrame > acceptedMarkerFrame)
        {
            auto previous = SampleCutMarker(animation, acceptedMarkerFrame);
            for (auto sample = acceptedMarkerFrame + 1; sample <= markerFrame; sample++)
            {
                auto marker = SampleCutMarker(animation, sample);
                if (marker && !previous)
                    crossedMarker = true;
                previous = marker;
            }
        }
        auto authored = entry || rewind || seek || crossedMarker;
        acceptedCamera = camera;
        acceptedAnimation = animation;
        acceptedSection = section;
        acceptedPhase = phase;
        acceptedMarkerFrame = markerFrame;
        cameraEvaluations.clear();
        return animatedCut || authored;
    }

    export void SubmitLights()
    {
        // Match CCutsceneManager::PostSceneUpdate's active/fully-streamed gate,
        // without running its unrelated scene updates twice.
        auto state = *CCutsceneManager::ms_State;
        if (state < 8 || state > 11 || !*pCutsceneFullyStreamed)
            return;
        auto camera = CCameraManager__GetCutsceneCam(cameraManager);
        auto phase = camera ? CCamCutscene__GetCurrentCameraPhaseInUse(camera, *pCutsceneSection) : -1.0f;
        CCutsceneManager__UpdateLights(phase);
    }

    export void Reset()
    {
        pendingCameraCut = 0;
        acceptedCamera = 0;
        cameraEvaluations.clear();
    }

    // A gameplay camera can cut without changing leaf. Wait for its changed view to reach the final viewport.
    export void NotifyCameraCut(uintptr_t camera)
    {
        pendingCameraCut = camera;
        pendingCameraCutFrame = *CTimer::m_frameCount;
    }

    export bool ConsumeCameraCut(uintptr_t leaf, bool changedView)
    {
        if (!pendingCameraCut || leaf != pendingCameraCut || uint32_t(*CTimer::m_frameCount - pendingCameraCutFrame) > 2)
        {
            pendingCameraCut = 0;
            return false;
        }
        if (!changedView)
            return false;
        pendingCameraCut = 0;
        return true;
    }

    export uintptr_t FinalLeaf()
    {
        return CCameraManager__GetFinalEffectiveLeafNode(cameraManager);
    }

    export bool IsLookingBehind()
    {
        return CCamInterface__IsLookingBehind();
    }

    // Independent of the optional jitter/timing patches: also supports vanilla camera evaluation.
    export void Init(bool legacy)
    {
        auto jumpCut = hook::pattern("E8 ? ? ? ? F3 0F 10 4D 08 8B F8 F3 0F 11 8E 44 01 00 00 85 FF 75");
        auto cutsceneState = hook::pattern("A1 ? ? ? ? 85 C0 75 0C 39 05 ? ? ? ? 0F 84 ? ? ? ? 80 3D ? ? ? ? 00 0F 84 ? ? ? ? 83 F8 08 74 13 83 F8 09");
        auto cutsceneSequence = hook::pattern("51 56 B9 ? ? ? ? E8 ? ? ? ? F3 0F 10 05 ? ? ? ? 8B F0 F3 0F 11 44 24 04 85 F6 74 1D FF 35");
        auto cutsceneLights = hook::pattern("55 8B EC 83 E4 F0 A1 ? ? ? ? 83 EC 78 8B 04 85 ? ? ? ? 56 57 85 C0 0F 84 ? ? ? ? 83 B8 F4 03 01 00 00");
        auto finalLeaf = hook::pattern("8B 49 04 81 EC F0 00 00 00 8D 04 24 56 50 E8 ? ? ? ? 8B F0 4E 78 18");
        auto lookingBehind = hook::pattern("6A 01 6A 01 6A 03 E8 ? ? ? ? 84 C0 0F 95 C0 C3");
        if (legacy)
        {
            jumpCut = hook::pattern("E8 ? ? ? ? F3 0F 10 45 08 8B C8 85 C9 89 4C 24 2C F3 0F 11 86 44 01 00 00");
            cutsceneSequence = hook::pattern("51 56 B9 ? ? ? ? E8 ? ? ? ? F3 0F 10 05 ? ? ? ? 8B F0 85 F6 F3 0F 11 44 24 04 74 ?");
            cutsceneLights = hook::pattern("55 8B EC 83 E4 F0 A1 ? ? ? ? 8B 04 85 ? ? ? ? 83 EC 74 85 C0 53 56 57 0F 84 ? ? ? ? 83 B8 F4 03 01 00 00");
            finalLeaf = hook::pattern("8B 49 04 81 EC F0 00 00 00 56 8D 44 24 04 50 E8 ? ? ? ? 8B F0 83 EE 01");
        }
        auto sequence = cutsceneSequence.get_first<uint8_t>();
        auto nativeJumpCut = injector::GetBranchDestination(jumpCut.get_first()).get<uint8_t>();
        auto cameraPhase = injector::GetBranchDestination(sequence + 0x28).get<uint8_t>();
        auto animationCall = legacy ? 0x3C : 0x48;
        auto trackCall = legacy ? 0x127 : 0x148;
        auto compositeCall = legacy ? 0x169 : 0x181;
        cameraManager = *(void**)(sequence + 3);
        CCameraManager__GetFinalEffectiveLeafNode = (decltype(CCameraManager__GetFinalEffectiveLeafNode))finalLeaf.get_first();
        CCamInterface__IsLookingBehind = (decltype(CCamInterface__IsLookingBehind))lookingBehind.get_first();
        CCameraManager__GetCutsceneCam = (decltype(CCameraManager__GetCutsceneCam))injector::GetBranchDestination(sequence + 7).as_int();
        CCamCutscene__GetCurrentCameraPhaseInUse = (decltype(CCamCutscene__GetCurrentCameraPhaseInUse))cameraPhase;
        pCutsceneSection = *(int32_t**)(sequence + (legacy ? 0x21 : 0x22));
        pCutsceneFullyStreamed = *cutsceneState.get_first<bool*>(0x17);
        CCamAnimated__GetAnimation = (decltype(CCamAnimated__GetAnimation))injector::GetBranchDestination(nativeJumpCut + animationCall).as_int();
        rage__crAnimation__FindTrack = (decltype(rage__crAnimation__FindTrack))injector::GetBranchDestination(nativeJumpCut + trackCall).as_int();
        rage__crAnimation__CompositeDof = (decltype(rage__crAnimation__CompositeDof))injector::GetBranchDestination(nativeJumpCut + compositeCall).as_int();
        crFrameDofFloatVtable = *(uintptr_t*)(nativeJumpCut + (legacy ? 0x154 : 0x178));
        CCutsceneManager__UpdateLights = (decltype(CCutsceneManager__UpdateLights))cutsceneLights.get_first();
        static auto JumpCutHook = safetyhook::create_mid(jumpCut.get_first(5), [](SafetyHookContext& regs)
        {
            // IsJumpCut may replace its phase argument; ESI is the camera, EBP the caller's frame pointer.
            ObserveJumpCut(regs.esi, *(float*)(regs.ebp + 8), regs.eax);
        });
    }
}

uint8_t* g_CutsceneAudioEntity = nullptr;
int* pGetTimeStepStaticsInitGuard = nullptr;

// The cutscene's time, smooth and locked to its audio. The audio thread updates the audio position in steps (and
// stops it when the stream stalls), so the time follows the real frame time between those updates and is pulled
// toward the audio position, never runs further ahead of it than one update and never goes back, unless the audio
// jumped (a new section's audio, a skip).
namespace CutsceneClock
{
    constexpr double ResyncMs = 250.0;   // the audio jumped, or the game hitched: take its position as it is
    constexpr double SlowPullMs = 500.0; // the time it takes to make up a small difference
    constexpr double FastPullMs = 100.0; // ... a larger one (30 ms or more)

    bool bLocked = false;
    double fClockMs = 0.0;      // the smooth audio position
    double fLastNowMs = 0.0;
    int nLastAudioMs = -1;
    double fAudioStepMs = 0.0;  // how far the audio position moves when it moves

    double NowMs()
    {
        static LARGE_INTEGER Frequency = []() { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
        LARGE_INTEGER Counter;
        QueryPerformanceCounter(&Counter);
        return double(Counter.QuadPart) * 1000.0 / double(Frequency.QuadPart);
    }

    void Unlock()
    {
        bLocked = false;
    }

    double Update(int AudioMs)
    {
        auto Now = NowMs();
        auto FrameMs = Now - fLastNowMs;
        fLastNowMs = Now;

        if (!bLocked)
        {
            bLocked = true;
            fClockMs = AudioMs;
            nLastAudioMs = AudioMs;
            fAudioStepMs = 0.0;
            return fClockMs;
        }

        if (AudioMs != nLastAudioMs)
        {
            auto Step = double(AudioMs - nLastAudioMs);
            if (Step > 0.0 && Step < ResyncMs)
                fAudioStepMs = fAudioStepMs > 0.0 ? fAudioStepMs + (Step - fAudioStepMs) * 0.1 : Step;
            nLastAudioMs = AudioMs;
        }

        auto Error = double(AudioMs) - fClockMs;
        if (FrameMs < 0.0 || FrameMs > ResyncMs || std::abs(Error) > ResyncMs)
        {
            // A jump in the audio (take it as it is), or a long frame: loading, a pause, the window in the background
            // (catch up with the audio, without going back)
            fClockMs = std::abs(Error) > ResyncMs ? double(AudioMs) : std::max(fClockMs, double(AudioMs));
            return fClockMs;
        }

        auto Clock = fClockMs + FrameMs;
        Error = double(AudioMs) - Clock;
        Clock += Error * std::min(1.0, FrameMs / (std::abs(Error) < 30.0 ? SlowPullMs : FastPullMs));

        // While the audio doesn't move (it stalled), wait for it
        auto MaxLead = std::max(fAudioStepMs, 10.0) + 5.0;
        Clock = std::min(Clock, double(AudioMs) + MaxLead);

        fClockMs = std::max(fClockMs, Clock);
        return fClockMs;
    }

}

namespace CCutsceneManager
{
    static inline float e2_int_time0;
    static inline float e2_int_offset0;
    static inline float e2_int_rate0;
    static inline float e2_int_time1;
    static inline float e2_int_offset1;
    static inline float e2_int_rate1;
    static inline float e2_int_time2;
    static inline float e2_int_offset2;

    static inline float GT06_AA_time0;
    static inline float GT06_AA_offset0;
    static inline float GT06_AA_rate0;
    static inline float GT06_AA_time1;
    static inline float GT06_AA_offset1;
    static inline float GT06_AA_rate1;
    static inline float GT06_AA_time2;
    static inline float GT06_AA_offset2;

    SafetyHookInline shGetTimeStep = {};
    void __cdecl GetTimeStep(float* a1)
    {
        float InTimeMs = *a1;
        float AudioTimeOffset = 0.0f;
        int PlayTimeMs = audCutsceneAudioEntity::GetPlayTimeMs(g_CutsceneAudioEntity);

        if (*_dwCurrentEpisode == 2)
        {
            int StaticsInitGuard = *pGetTimeStepStaticsInitGuard;

            if ((*pGetTimeStepStaticsInitGuard & 1) == 0)
            {
                StaticsInitGuard = *pGetTimeStepStaticsInitGuard | 1;
                *pGetTimeStepStaticsInitGuard |= 1u;
                e2_int_time0 = 120.0f;
                e2_int_offset0 = 0.0f;
                e2_int_time1 = 180.0f;
                e2_int_offset1 = -300.0f;
                e2_int_time2 = 350.0f;
                e2_int_offset2 = -500.0f;
                e2_int_rate0 = 0.016666668f;
                e2_int_rate1 = 0.0058823531f;
            }

            if ((StaticsInitGuard & 2) == 0)
            {
                *pGetTimeStepStaticsInitGuard = StaticsInitGuard | 2;
                GT06_AA_time0 = 45.0f;
                GT06_AA_offset0 = 0.0f;
                GT06_AA_time1 = 180.0f;
                GT06_AA_offset1 = -400.0f;
                GT06_AA_time2 = 350.0f;
                GT06_AA_offset2 = -400.0f;
                GT06_AA_rate0 = 0.0074074073f;
                GT06_AA_rate1 = 0.0058823531f;
            }

            if (!_stricmp(pszCurrentCutsceneName, "e2_int"))
            {
                float PlayTimeSec = PlayTimeMs * 0.001f;

                if ((PlayTimeSec - e2_int_time0) < 0.0f)
                    AudioTimeOffset = e2_int_offset0;
                else
                    AudioTimeOffset = (((PlayTimeSec - e2_int_time0) * e2_int_rate0) * (e2_int_offset1 - e2_int_offset0)) + e2_int_offset0;

                if ((PlayTimeSec - e2_int_time1) >= 0.0f)
                    AudioTimeOffset = (((PlayTimeSec - e2_int_time1) * e2_int_rate1) * (e2_int_offset2 - e2_int_offset1)) + e2_int_offset1;

                if ((PlayTimeSec - e2_int_time2) >= 0.0f)
                    AudioTimeOffset = e2_int_offset2;
            }
            else if (!_stricmp(pszCurrentCutsceneName, "GT06_AA"))
            {
                float PlayTimeSec = PlayTimeMs * 0.001f;

                if ((PlayTimeSec - GT06_AA_time0) < 0.0f)
                    AudioTimeOffset = GT06_AA_offset0;
                else
                    AudioTimeOffset = (((PlayTimeSec - GT06_AA_time0) * GT06_AA_rate0) * (GT06_AA_offset1 - GT06_AA_offset0)) + GT06_AA_offset0;

                if ((PlayTimeSec - GT06_AA_time1) >= 0.0f)
                    AudioTimeOffset = (((PlayTimeSec - GT06_AA_time1) * GT06_AA_rate1) * (GT06_AA_offset2 - GT06_AA_offset1)) + GT06_AA_offset1;

                if ((PlayTimeSec - GT06_AA_time2) >= 0.0f)
                    AudioTimeOffset = GT06_AA_offset2;
            }
            else
            {
                AudioTimeOffset = 0.0f;
            }
        }

        // Audio playback position with the drift ramp applied
        int AudioTimeMs = 0;

        if (PlayTimeMs == -1)
        {
            AudioTimeMs = -1;
        }
        else
        {
            AudioTimeMs = PlayTimeMs + (int)AudioTimeOffset;
            if (AudioTimeMs < 0)
                AudioTimeMs = 0;
        }

        float OutTimeMs = 0.0f;

        if (*CCutsceneManager::ms_State != 8)
        {
            // Not playing yet or switching sections: the game's time step, as vanilla
            CutsceneClock::Unlock();
            OutTimeMs = (*CTimer::m_gameTime * 1000.0f) + InTimeMs;
        }
        else if (AudioTimeMs == -1)
        {
            // No audio: the unscaled frame time, as vanilla
            CutsceneClock::Unlock();
            OutTimeMs = (*CTimer::m_systemTime * 1000.0f) + InTimeMs;
        }
        else
        {
            // Vanilla takes the audio position as it is. It only moves when the audio thread updates it, in steps that
            // don't line up with the frames, so the animations jitter at any frame rate. The clock below follows it
            // smoothly instead.
            OutTimeMs = float(CutsceneClock::Update(AudioTimeMs) - *CCutsceneManager::ms_fTimePassedSinceLastAudioStart);
            if (OutTimeMs < 0.0f)
                OutTimeMs = 0.0f;
        }

        *a1 = OutTimeMs;
    }
}

class CutsceneCam
{
public:
    CutsceneCam()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // Skip two additional checks in cutscene camera update code added by Toronto on PC
            // which made cutscenes zoom erratically depending on fps and their jump cuts "flickery" because of it
            auto pattern = find_pattern("83 3D ? ? ? ? ? 0F 8E ? ? ? ? 83 FF", "83 3D ? ? ? ? ? 0F 8E ? ? ? ? 83 F8");
            injector::MakeNOP(pattern.get_first(0), 27, true);

            // Cutscene jitter workarounds
            {
                // By Sergeanur
                pattern = find_pattern("E8 ? ? ? ? 8B 4C 24 2C 5F 5E 33 CC B0 01", "E8 ? ? ? ? 8B 4C 24 2C 5F 5E 5B");

                static void* patchOffset = pattern.get_first(0);

                static void* pUpdateCameraFromPhase = injector::GetBranchDestination(patchOffset).get<void*>();

                pattern = find_pattern("C6 44 24 ? ? A1 ? ? ? ? 83 FF 03", "C6 44 24 ? ? 83 F9 03");
                static void* pJumpCutFlagByte = pattern.get_first(4);
                static double incrementalTimeStep = 0.0;

                struct CCamAnimated
                {
                    uint8_t pad0[0x20];
                    rage::Vector3 m_matrixB;
                    uint8_t pad1[0x14];
                    rage::Vector3 m_matrixPos;
                    uint8_t pad2[0x4B4];

                    bool UpdateCameraFromPhase(float fPhase)
                    {
                        return ((bool(__thiscall*)(CCamAnimated*, float))pUpdateCameraFromPhase)(this, fPhase);
                    }

                    bool UpdateCameraFromPhaseHook(float fPhase)
                    {
#if 1
                        incrementalTimeStep += *CTimer::fTimeStep;

                        CCamAnimated temp = *this;

                        injector::WriteMemory<uint8_t>(pJumpCutFlagByte, 1, true);
                        bool result = UpdateCameraFromPhase(fPhase) != 0.0;

                        CCamAnimated temp2 = *this;

                        if (incrementalTimeStep < 0.3333)
                            return result;

                        *this = temp;

                        injector::WriteMemory<uint8_t>(pJumpCutFlagByte, 0, true);
                        bool result2 = UpdateCameraFromPhase(fPhase) != 0.0;

                        temp = *this;

                        if (fabs(temp.m_matrixB.x - temp2.m_matrixB.x) > 0.03333f
                            || fabs(temp.m_matrixB.y - temp2.m_matrixB.y) > 0.03333f
                            || fabs(temp.m_matrixB.z - temp2.m_matrixB.z) > 0.03333f
                            || fabs(temp.m_matrixPos.x - temp2.m_matrixPos.x) > 0.3333f
                            || fabs(temp.m_matrixPos.y - temp2.m_matrixPos.y) > 0.3333f
                            || fabs(temp.m_matrixPos.z - temp2.m_matrixPos.z) > 0.3333f)
                        {
                            incrementalTimeStep = 0.0;
                            *this = temp2;
                            return result;
                        }
                        return result2;
#else
                        return UpdateCameraFromPhase(fPhase) != 0.0;
#endif
                    }
                };

                auto dest = &CCamAnimated::UpdateCameraFromPhaseHook;
                injector::MakeCALL(patchOffset, *(void**)&dest, true);

                pattern = find_pattern("E8 ? ? ? ? 8B CD 88 44 24 0F", "E8 ? ? ? ? 8B CF 88 44 24 0F");
                injector::MakeCALL(pattern.get_first(0), *(void**)&dest, true);

                // Camera cuts: near a cut (flagged in the camera animation, in 30 fps frames) the camera keeps its last
                // phase and stores the 30 fps frame it held at (+0x1B4), then keeps using that frame. Not storing it keeps
                // the hold to the frames that are really at the cut, at any frame rate.
                pattern = find_pattern("F3 0F 11 86 ? ? ? ? 5E 5B 8B 4C 24 30 33 CC E8 ? ? ? ? 83 C4 34 C2 04 00", "F3 0F 11 8F ? ? ? ? 5F 5E B8 ? ? ? ? 5D 83 C4 2C C2 04 00");
                injector::MakeNOP(pattern.get_first(0), 8, true);

                pattern = find_pattern("F3 0F 11 86 ? ? ? ? 5F 5E B8 ? ? ? ? 5B 8B 4C 24 30 33 CC E8 ? ? ? ? 83 C4 34 C2 04 00", "F3 0F 11 8F ? ? ? ? C6 87 ? ? ? ? ? 5F 5E B8 ? ? ? ? 5D 83 C4 2C C2 04 00");
                injector::MakeNOP(pattern.get_first(0), 8, true);
            }

            // Cutscene animation-audio sync adjustments
            {
                pattern = hook::pattern("B9 ? ? ? ? F3 0F 11 44 24 ? E8 ? ? ? ? 83 3D");
                g_CutsceneAudioEntity = *pattern.get_first<uint8_t*>(1);

                pattern = hook::pattern("8B 0D ? ? ? ? F6 C1 ? 75 ? 83 C9 ? 89 0D ? ? ? ? C7 05");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    pGetTimeStepStaticsInitGuard = *pattern.get_first<int*>(2);
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    pattern = hook::pattern("A1 ? ? ? ? A8 ? F3 0F 10 05");
                    pGetTimeStepStaticsInitGuard = *pattern.get_first<int*>(1);
                }

                pattern = find_pattern("51 56 8B 74 24 ? 57 F3 0F 10 06", "51 56 57 8B 7C 24 ? F3 0F 10 07");
                CCutsceneManager::shGetTimeStep = safetyhook::create_inline(pattern.get_first(0), CCutsceneManager::GetTimeStep);
            }
        };
    }
} CutsceneCam;
