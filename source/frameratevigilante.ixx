module;

#include <common.hxx>

export module frameratevigilante;

import classext;
import common;
import comvars;
import natives;
import settings;

uint32_t CTimerExt::m_logicalFrameCounter;
uint32_t CTimerExt::m_logicalFramesPassed;

namespace CTimer
{
    SafetyHookInline shInit = {};
    void __cdecl Init()
    {
        shInit.unsafe_ccall();

        CTimerExt::m_logicalFrameCounter = 0;
        CTimerExt::m_logicalFramesPassed = 0;
    }
}

namespace CPhysics
{
    // Original function, for reference
    /*void __stdcall Update()
    {
        CPhysics::ScanForBuildings();
        CPhysics::UpdateRequestList();

        int v0 = *CWorld::ms_listProcessControlPtrs;
        while (v0)
        {
            DWORD* v1 = *(DWORD**)v0;
            v0 = *(DWORD*)(v0 + 4);

            if (v1)
            {
                uint32_t v2 = (v1[10] >> 6) & 0xF;
                if (v2 > 1 && v2 < 5)
                    (*(void(__thiscall**)(DWORD*))(*v1 + 256))(v1);
            }
        }

        CPhysics::ResetNumPoolGameCollisions();

        float v3 = 1.0f / (float)*CPhysics::ms_NumTimeSlices;

        for (int i = 0; i < *CPhysics::ms_NumTimeSlices; i++)
        {
            CPhysics::PreSimUpdate(*CTimer::fTimeStep * v3, i);
            CPhysics::SimUpdate(*CTimer::fTimeStep * v3);
            CPhysics::IterateOverManifolds();
            CPhysics::PostSimUpdate(i, *CTimer::fTimeStep * v3);
        }
    }*/

    SafetyHookInline shUpdate = {};
    void __stdcall Update()
    {
        CPhysics::ScanForBuildings();
        CPhysics::UpdateRequestList();

        int v0 = *CWorld::ms_listProcessControlPtrs;
        while (v0)
        {
            DWORD* v1 = *(DWORD**)v0;
            v0 = *(DWORD*)(v0 + 4);

            if (v1)
            {
                uint32_t v2 = (v1[10] >> 6) & 0xF;
                if (v2 > 1 && v2 < 5)
                    (*(void(__thiscall**)(DWORD*))(*v1 + 256))(v1);
            }
        }

        CPhysics::ResetNumPoolGameCollisions();

        // Correct halved time slices by timestep (Earlier than vanilla, i.e. not inside the function inputs anymore)
        float v3 = *CTimer::fTimeStep * (1.0f / (float)*CPhysics::ms_NumTimeSlices);

        // Add a cheat to speed up the simulation rate by 2x. Can be pretty fun.
        if (bSpeedupSimRateCheat)
        {
            v3 *= 2.0f;
        }

        // Floor timestep at half the max timestep (1.0f / 300.0f) for PreSim and PostSim updates. Prevents some overshooting at high fps.
        float v4 = std::clamp(v3, 1.0f / 150.0f, FLT_MAX);

        for (int i = 0; i < *CPhysics::ms_NumTimeSlices; i++)
        {
            CPhysics::PreSimUpdate(v4, i);
            CPhysics::SimUpdate(v3); // This is probably the most important thing to correct but its kind of volatile to touch. Better ideas are welcome for handling all this.
            CPhysics::IterateOverManifolds();
            CPhysics::PostSimUpdate(i, v4);
        }
    }
}

namespace CWater
{
    SafetyHookInline shAddToDynamicWaterSpeed = {};
    void __cdecl AddToDynamicWaterSpeed(int a1, int a2, float a3, char a4)
    {
        return shAddToDynamicWaterSpeed.unsafe_ccall(a1, a2, a3 * (*CTimer::fTimeStep / (1.0f / 30.0f)), a4);
    }

    SafetyHookInline shModifyDynamicWaterSpeed = {};
    void __cdecl ModifyDynamicWaterSpeed(int a1, int a2, float a3, float a4, char a5)
    {
        return shModifyDynamicWaterSpeed.unsafe_ccall(a1, a2, a3, a4 * (*CTimer::fTimeStep / (1.0f / 30.0f)), a5);
    }
}

namespace CPedIntelligence
{
    SafetyHookInline shProcessStaticCounter = {};
    void __fastcall ProcessStaticCounter(void* _this, void* edx)
    {
        // Retrieve CPedIntelligenceExt members, if not possible (Shouldn't happen), fall back to the original function.
        auto PIExt = GetPedIntelligenceExt((uintptr_t)_this);
        if (!PIExt)
        {
            return shProcessStaticCounter.unsafe_fastcall(_this, edx);
        }

        // Check a 30hz accumulator before calling CPedIntelligence::ProcessStaticCounter, which increments the task attempt counter.
        // Some CTasks check this attempt counter against a hardcoded limit of 30. At higher frame rates these attempts occur faster, causing them to hit the limit early and abort the task.
        PIExt->m_fTimeStepAccumulator += *CTimer::fTimeStep;

        while (PIExt->m_fTimeStepAccumulator >= (1.0f / 30.0f))
        {
            PIExt->m_fTimeStepAccumulator -= (1.0f / 30.0f);

            return shProcessStaticCounter.unsafe_fastcall(_this, edx);
        }
    }
}

namespace CHandShaker
{
    // Original function, for reference
    /*void __fastcall Process(float* _this, void* edx, float a2)
    {
        float v0 = *(_this + 16);
        float v1 = *(_this + 17);
        float v2 = *(_this + 18);
        float v3 = *(_this + 36);

        float v4 = (float)(fabs(v0 / *(_this + 20)) * (float)(*(_this + 37) - v3)) + v3;
        float v5 = (float)(fabs(v1 / *(_this + 21)) * (float)(*(_this + 37) - v3)) + v3;
        float v6 = (float)(fabs(v2 / *(_this + 22)) * (float)(*(_this + 37) - v3)) + v3;

        float v7 = v4;
        float v8 = v5;
        float v9 = v6;

        if (v0 > 0.0f && *(_this + 28) > 0.0f || v0 < 0.0f && *(_this + 28) < 0.0f)
            v7 = *(_this + 32) * v4;

        if (v1 > 0.0f && *(_this + 29) > 0.0f || v1 < 0.0f && *(_this + 29) < 0.0f)
            v8 = *(_this + 33) * v5;

        if (v2 > 0.0f && *(_this + 30) > 0.0f || v2 < 0.0f && *(_this + 30) < 0.0f)
            v9 = *(_this + 34) * v6;

        float v10 = *(_this + 26);
        float v11 = (float)game_rand() * 0.000030518509f;

        float v12 = *(_this + 25);
        float v13 = (float)game_rand() * 0.000030518509f;

        float v14 = *(_this + 24);
        float v15 = (float)game_rand() * 0.000030518509f;

        float v16 = (float)(v13 * v12) * v8;
        float v17 = (float)(v11 * v10) * v9;
        float v18 = (float)(v15 * v14) * v7;

        if (*(_this + 16) > 0.0f)
            v18 *= -1.0f;

        if (*(_this + 17) > 0.0f)
            v16 *= -1.0f;

        if (*(_this + 18) > 0.0f)
            v17 *= -1.0f;

        *(_this + 28) += v18;
        *(_this + 29) += v16;
        *(_this + 30) += v17;

        float v19 = *CTimer::fCamTimeStep * 30.0f;
        float v20 = *CTimer::fCamTimeStep * 30.0f;
        if (*CReplayMgr::dword_11F7060 == 1 || *CReplayMgr::dword_12088B4 != (HANDLE)-1 || *CReplayMgr::dword_1037720 == 18)
        {
            v19 = (float)((float)(uint32_t)*CReplayMgr::dword_11F704C * 0.001f) * 30.0f;
            v20 = v19;
        }

        int v21 = (int)v19 * (int)*(_this + 38);
        if ((int)(float)((float)((float)(uint16_t)game_rand() * 0.000030517578f) * (float)(v21 - 1)) == 1)
        {
            float v22 = *(_this + 39);
            float v23 = (float)game_rand() * 0.000030518509f;

            float v24 = *(_this + 39);
            float v25 = (float)game_rand() * 0.000030518509f;

            float v26 = *(_this + 39);
            float v27 = (float)game_rand() * 0.000030518509f;

            *(_this + 30) += (float)((float)((float)(v22 - (float)-v22) * v23) - v22);
            *(_this + 28) += (float)((float)((float)(v26 - (float)-v26) * v27) - v26);
            *(_this + 29) += (float)((float)((float)(v24 - (float)-v24) * v25) - v24);
        }

        float v28 = (float)(*(_this + 28) * v20) + *(_this + 16);
        float v29 = (float)(*(_this + 29) * v20) + *(_this + 17);
        float v30 = (float)(*(_this + 30) * v20) + *(_this + 18);

        *(_this + 16) = v28;
        *(_this + 17) = v29;
        *(_this + 18) = v30;

        float v31 = *(_this + 16);
        float v32 = *(_this + 17);
        float v33 = *(_this + 18);

        float v34 = *(_this + 20);
        float v35 = *(_this + 21);
        float v36 = *(_this + 22);

        if (v31 <= (float)-v34)
            v31 = -v34;

        if (v34 <= v31)
            v31 = v34;

        if (v32 <= (float)-v35)
            v32 = -v35;

        if (v35 <= v32)
            v32 = v35;

        if (v33 <= (float)-v36)
            v33 = -v36;

        if (v36 <= v33)
            v33 = v36;

        *(_this + 16) = v31;
        *(_this + 17) = v32;
        *(_this + 18) = v33;

        float v37[4] =
        {
            v31 * a2,
            v32 * a2,
            v33 * a2
        };

        rage::Matrix34::FromEulersXYZ(_this, edx, v37);
    }*/

    SafetyHookInline shProcess = {};
    void __fastcall Process(float* _this, void* edx, float a2)
    {
        float v0 = *(_this + 16);
        float v1 = *(_this + 17);
        float v2 = *(_this + 18);
        float v3 = *(_this + 36);

        float v4 = *CTimer::fCamTimeStep * 30.0f;
        float v5 = *CTimer::fCamTimeStep * 30.0f;
        if (*CReplayMgr::dword_11F7060 == 1 || *CReplayMgr::dword_12088B4 != (HANDLE)-1 || *CReplayMgr::dword_1037720 == 18)
        {
            v4 = (float)((float)(uint32_t)*CReplayMgr::dword_11F704C * 0.001f) * 30.0f;
            v5 = v4;
        }

        // Check a 30hz accumulator against the random noise generators and impulse accumulators, fixing their fps dependency.
        static float v6 = 0.0f;
        float v7 = *CTimer::fTimeStep;

        v6 += v7;

        while (v6 >= (1.0f / 30.0f))
        {
            v6 -= (1.0f / 30.0f);

            float v8 = (float)(fabs(v0 / *(_this + 20)) * (float)(*(_this + 37) - v3)) + v3;
            float v9 = (float)(fabs(v1 / *(_this + 21)) * (float)(*(_this + 37) - v3)) + v3;
            float v10 = (float)(fabs(v2 / *(_this + 22)) * (float)(*(_this + 37) - v3)) + v3;

            float v11 = v8;
            float v12 = v9;
            float v13 = v10;

            if (v0 > 0.0f && *(_this + 28) > 0.0f || v0 < 0.0f && *(_this + 28) < 0.0f)
                v11 = *(_this + 32) * v8;

            if (v1 > 0.0f && *(_this + 29) > 0.0f || v1 < 0.0f && *(_this + 29) < 0.0f)
                v12 = *(_this + 33) * v9;

            if (v2 > 0.0f && *(_this + 30) > 0.0f || v2 < 0.0f && *(_this + 30) < 0.0f)
                v13 = *(_this + 34) * v10;

            float v14 = *(_this + 26);
            float v15 = (float)game_rand() * 0.000030518509f;

            float v16 = *(_this + 25);
            float v17 = (float)game_rand() * 0.000030518509f;

            float v18 = *(_this + 24);
            float v19 = (float)game_rand() * 0.000030518509f;

            float v20 = (float)(v17 * v16) * v12;
            float v21 = (float)(v15 * v14) * v13;
            float v22 = (float)(v19 * v18) * v11;

            if (*(_this + 16) > 0.0f)
                v22 *= -1.0f;

            if (*(_this + 17) > 0.0f)
                v20 *= -1.0f;

            if (*(_this + 18) > 0.0f)
                v21 *= -1.0f;

            *(_this + 28) += v22;
            *(_this + 29) += v20;
            *(_this + 30) += v21;

            // This bit in the original version causes (int)v4 * (int)*(_this + 38) to truncate to 0 if the frame rate is even 1 frame above 30fps, thus killing the effect.
            // The fix for that is to cast their result to int, so that the effect can still run past 30fps, albeit sped up (Which is arguably a lot worse).
            // This is also the case on consoles, and for example the same bug can be noticed when emulating the Xbox 360 version (Officially or unofficially), due to the 60fps VSync cap.
            //
            // In this version of the function we do not need to scale *(_this + 38) by the camera timestep at all, because we gate everything here behind a 30hz timer which would mess up things.
            // Hence, we simply assign an int-cast *(_this + 38) to v23, which fixes fps dependencies further.
            int v23 = (int)*(_this + 38);
            if ((int)(float)((float)((float)(uint16_t)game_rand() * 0.000030517578f) * (float)(v23 - 1)) == 1)
            {
                float v24 = *(_this + 39);
                float v25 = (float)game_rand() * 0.000030518509f;

                float v26 = *(_this + 39);
                float v27 = (float)game_rand() * 0.000030518509f;

                float v28 = *(_this + 39);
                float v29 = (float)game_rand() * 0.000030518509f;

                *(_this + 30) += (float)((float)((float)(v24 - (float)-v24) * v25) - v24);
                *(_this + 28) += (float)((float)((float)(v28 - (float)-v28) * v29) - v28);
                *(_this + 29) += (float)((float)((float)(v26 - (float)-v26) * v27) - v26);
            }
        }

        float v30 = (float)(*(_this + 28) * v5) + *(_this + 16);
        float v31 = (float)(*(_this + 29) * v5) + *(_this + 17);
        float v32 = (float)(*(_this + 30) * v5) + *(_this + 18);

        *(_this + 16) = v30;
        *(_this + 17) = v31;
        *(_this + 18) = v32;

        float v33 = *(_this + 16);
        float v34 = *(_this + 17);
        float v35 = *(_this + 18);

        float v36 = *(_this + 20);
        float v37 = *(_this + 21);
        float v38 = *(_this + 22);

        if (v33 <= (float)-v36)
            v33 = -v36;

        if (v36 <= v33)
            v33 = v36;

        if (v34 <= (float)-v37)
            v34 = -v37;

        if (v37 <= v34)
            v34 = v37;

        if (v35 <= (float)-v38)
            v35 = -v38;

        if (v38 <= v35)
            v35 = v38;

        *(_this + 16) = v33;
        *(_this + 17) = v34;
        *(_this + 18) = v35;

        // Gate the output behind a menu option, which would allow disabling the shake (Of cameras only) for convenience.
        static auto CameraShake = FusionFixSettings.GetRef("PREF_CAMERASHAKE");
        if (CameraShake->get() || a2 == 75.0f /* Special condition to avoid disabling shaky helicopter searchlights if Camera Shake is disabled */)
        {
            float v39[4] =
            {
                v33 * a2,
                v34 * a2,
                v35 * a2
            };

            rage::Matrix34::FromEulersXYZ(_this, edx, v39);
        }
        else
        {
            float v39[4] =
            {
                0.0f,
                0.0f,
                0.0f
            };

            rage::Matrix34::FromEulersXYZ(_this, edx, v39);
        }
    }
}

namespace CCamFollowVehicle
{
    float ms_accelLimit = 5.0f;
    float dword_103B984 = 5.0f;

    float ms_vehAccelForce = 0.025f;
    float dword_103B974 = 0.025f;

    float ms_springForce = 7.0f;
    float dword_103B9A4 = 3.0f;
    float dword_103B9A8 = 0.0f;

    float ms_dampForce = 0.018f;
    float dword_103B994 = 0.01f;
    float dword_103B998 = 0.0f;

    // Original function, for reference
    /*void __fastcall SprungMounting(int _this, void* edx, DWORD* a2, float* a3, float* a4, float a5)
    {
        float v0 = *CTimer::fTimeStep;

        float v1[4];
        CPhysical::GetLocalSpeed(a2, edx, v1, a3, 0, 0);

        float* v2 = (float*)a2[8];
        float v3 = (float)(v1[1] - (float)(*(float*)(_this + 724) + (float)((float)(*(float*)(_this + 744) * *a3) - (float)(*(float*)(_this + 736) * a3[2])))) * *(CTimer::fTimeStep + 1);
        float v4 = (float)(v1[0] - (float)(*(float*)(_this + 720) + (float)((float)(*(float*)(_this + 740) * a3[2]) - (float)(*(float*)(_this + 744) * a3[1])))) * *(CTimer::fTimeStep + 1);
        float v5 = (float)(v1[2] - (float)(*(float*)(_this + 728) + (float)((float)(*(float*)(_this + 736) * a3[1]) - (float)(*(float*)(_this + 740) * *a3)))) * *(CTimer::fTimeStep + 1);

        float v6 = v2[1] * v3;
        float v7 = v2[5] * v3;

        float v8 = (float)(v6 + (float)(*v2 * v4)) + (float)(v2[2] * v5);
        float v9 = (float)(v7 + (float)(v4 * v2[4])) + (float)(v2[6] * v5);

        float v10 = ms_accelLimit;
        float v11 = -ms_accelLimit;

        if ((float)-ms_accelLimit <= v8)
            v11 = v8;

        if (v11 <= ms_accelLimit)
            v10 = v11;

        float v12 = dword_103B984;
        float v13 = -dword_103B984;

        if ((float)-dword_103B984 <= v9)
            v13 = v9;

        if (v13 <= dword_103B984)
            v12 = v13;

        float v14 = *(float*)(_this + 704);
        float v15 = *(float*)(_this + 708);
        float v16 = *(float*)(_this + 712);
        
        *(float*)(_this + 688) += (float)((float)(ms_vehAccelForce * v0) * v12);
        *(float*)(_this + 692) += (float)((float)(dword_103B974 * v0) * v10);

        *(float*)(_this + 688) -= (float)((float)(v14 * ms_springForce) * v0);
        *(float*)(_this + 692) -= (float)((float)(v15 * dword_103B9A4) * v0);
        *(float*)(_this + 696) -= (float)((float)(v16 * dword_103B9A8) * v0);

        *(float*)(_this + 688) *= powf(ms_dampForce, v0);
        *(float*)(_this + 692) *= powf(dword_103B994, v0);
        *(float*)(_this + 696) *= powf(dword_103B998, v0);

        *(float*)(_this + 704) += *(float*)(_this + 688);
        *(float*)(_this + 708) += *(float*)(_this + 692);
        *(float*)(_this + 712) += *(float*)(_this + 696);

        DWORD v17[4];
        DWORD* v18 = (DWORD*)(*(int(__thiscall**)(DWORD*, DWORD*))(*a2 + 236))(a2, v17);

        *(DWORD*)(_this + 720) = *v18;
        *(DWORD*)(_this + 724) = v18[1];
        *(DWORD*)(_this + 728) = v18[2];
        *(DWORD*)(_this + 732) = v18[3];

        int* v19 = (int*)CPhysical::GetTurnSpeed(a2, edx, (float*)v17);

        *(DWORD*)(_this + 736) = *v19;
        *(DWORD*)(_this + 740) = v19[1];
        *(DWORD*)(_this + 744) = v19[2];
        *(DWORD*)(_this + 748) = v19[3];

        float v20 = *(float*)(_this + 704) * a5;
        float v21 = *(float*)(_this + 708) * a5;
        float v22 = *(float*)(_this + 712) * a5;

        a4[0] += v20;
        a4[1] += v21;
        a4[2] += v22;
    }*/

    SafetyHookInline shSprungMounting = {};
    void __fastcall SprungMounting(int _this, void* edx, DWORD* a2, float* a3, float* a4, float a5)
    {
        // Fixed 30fps delta variables
        float v0 = 1.0f / 30.0f;
        float v1 = 1.0f / v0;

        // Retrieve CCamFollowVehicleExt members, if not possible (Shouldn't happen), bail out.
        auto CFVExt = GetCamFollowVehicleExt((uintptr_t)_this);
        if (!CFVExt)
        {
            return;
        }

        // The original function updates the spring simulation just by CTimer::fTimeStep, which naturally causes fps dependencies.
        // Check a 30hz accumulator against the spring simulation, replace the old timestep usage with fixed 30fps values and interpolate the result later to gain back the lost smoothness.
        CFVExt->m_fTimeStepAccumulator += *CTimer::fTimeStep;

        while (CFVExt->m_fTimeStepAccumulator >= v0)
        {
            CFVExt->m_fTimeStepAccumulator -= v0;

            float v2[4];
            CPhysical::GetLocalSpeed(a2, edx, v2, a3, 0, 0);

            float* v3 = (float*)a2[8];
            float v4 = (float)(v2[1] - (float)(*(float*)(_this + 724) + (float)((float)(*(float*)(_this + 744) * *a3) - (float)(*(float*)(_this + 736) * a3[2])))) * v1;
            float v5 = (float)(v2[0] - (float)(*(float*)(_this + 720) + (float)((float)(*(float*)(_this + 740) * a3[2]) - (float)(*(float*)(_this + 744) * a3[1])))) * v1;
            float v6 = (float)(v2[2] - (float)(*(float*)(_this + 728) + (float)((float)(*(float*)(_this + 736) * a3[1]) - (float)(*(float*)(_this + 740) * *a3)))) * v1;

            float v7 = v3[1] * v4;
            float v8 = v3[5] * v4;

            float v9 = (float)(v7 + (float)(*v3 * v5)) + (float)(v3[2] * v6);
            float v10 = (float)(v8 + (float)(v5 * v3[4])) + (float)(v3[6] * v6);

            float v11 = ms_accelLimit;
            float v12 = -ms_accelLimit;

            if ((float)-ms_accelLimit <= v9)
                v12 = v9;

            if (v12 <= ms_accelLimit)
                v11 = v12;

            float v13 = dword_103B984;
            float v14 = -dword_103B984;

            if ((float)-dword_103B984 <= v10)
                v14 = v10;

            if (v14 <= dword_103B984)
                v13 = v14;

            // Save the previous spring offsets for interpolating against later.
            CFVExt->m_fOldSpringOffsetX = *(float*)(_this + 704);
            CFVExt->m_fOldSpringOffsetY = *(float*)(_this + 708);
            CFVExt->m_fOldSpringOffsetZ = *(float*)(_this + 712);

            float v15 = *(float*)(_this + 704);
            float v16 = *(float*)(_this + 708);
            float v17 = *(float*)(_this + 712);

            *(float*)(_this + 688) += (float)((float)(ms_vehAccelForce * v0) * v13);
            *(float*)(_this + 692) += (float)((float)(dword_103B974 * v0) * v11);

            *(float*)(_this + 688) -= (float)((float)(v15 * ms_springForce) * v0);
            *(float*)(_this + 692) -= (float)((float)(v16 * dword_103B9A4) * v0);
            *(float*)(_this + 696) -= (float)((float)(v17 * dword_103B9A8) * v0);

            *(float*)(_this + 688) *= powf(ms_dampForce, v0);
            *(float*)(_this + 692) *= powf(dword_103B994, v0);
            *(float*)(_this + 696) *= powf(dword_103B998, v0);

            *(float*)(_this + 704) += *(float*)(_this + 688);
            *(float*)(_this + 708) += *(float*)(_this + 692);
            *(float*)(_this + 712) += *(float*)(_this + 696);

            DWORD v18[4];
            DWORD* v19 = (DWORD*)(*(int(__thiscall**)(DWORD*, DWORD*))(*a2 + 236))(a2, v18);

            *(DWORD*)(_this + 720) = *v19;
            *(DWORD*)(_this + 724) = v19[1];
            *(DWORD*)(_this + 728) = v19[2];
            *(DWORD*)(_this + 732) = v19[3];

            int* v20 = (int*)CPhysical::GetTurnSpeed(a2, edx, (float*)v18);

            *(DWORD*)(_this + 736) = *v20;
            *(DWORD*)(_this + 740) = v20[1];
            *(DWORD*)(_this + 744) = v20[2];
            *(DWORD*)(_this + 748) = v20[3];
        }

        // Interpolate between the previous and current spring states to smooth out the 30hz visual rate.
        float v21 = CFVExt->m_fTimeStepAccumulator / v0;

        float v22 = CFVExt->m_fOldSpringOffsetX + (*(float*)(_this + 704) - CFVExt->m_fOldSpringOffsetX) * v21;
        float v23 = CFVExt->m_fOldSpringOffsetY + (*(float*)(_this + 708) - CFVExt->m_fOldSpringOffsetY) * v21;
        float v24 = CFVExt->m_fOldSpringOffsetZ + (*(float*)(_this + 712) - CFVExt->m_fOldSpringOffsetZ) * v21;

        a4[0] += v22 * a5;
        a4[1] += v23 * a5;
        a4[2] += v24 * a5;
    }

    SafetyHookInline shProcessHandBrakeSwing = {};
    double __fastcall ProcessHandBrakeSwing(DWORD* _this, void* edx, DWORD* a2, int a3)
    {
        float f = 1.0f;

        if (!Natives::IsUsingController())
        {
            f = 3.0f;
        }

        return shProcessHandBrakeSwing.unsafe_fastcall<double>(_this, edx, a2, a3) * (*CTimer::fTimeStep / (1.0f / 30.0f)) * f;
    }
}

injector::hook_back<decltype(&Natives::SlideObject)> hbSLIDE_OBJECT;
bool __cdecl NATIVE_SLIDE_OBJECT_1(Object object, float x, float y, float z, float xs, float ys, float zs, bool flag)
{
    float f = *CTimer::fTimeStep / (1.0f / 30.0f);

    return hbSLIDE_OBJECT.fun(object, x, y, z, xs * f, ys * f, zs * f, flag);
}

SafetyHookInline shNATIVE_SLIDE_OBJECT = {};
bool __cdecl NATIVE_SLIDE_OBJECT_2(Object object, float x, float y, float z, float xs, float ys, float zs, bool flag)
{
    float f = *CTimer::fTimeStep / (1.0f / 30.0f);

    return shNATIVE_SLIDE_OBJECT.unsafe_ccall<bool>(object, x, y, z, xs * f, ys * f, zs * f, flag);
}

class FramerateVigilante
{
public:
    FramerateVigilante()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // Add a logical frame counter alongside the regular one that counts frames as if we're running at 30 fps, which is going to be useful for fixing some fps issues
            {
                // Initialize logical frame counter variables to 0 in CTimer::Init
                auto pattern = find_pattern("E8 ? ? ? ? FF 74 24 ? E8 ? ? ? ? E8", "E8 ? ? ? ? 8B 44 24 ? 50 E8 ? ? ? ? E8");
                CTimer::shInit = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first(0)).get<void*>(), CTimer::Init);

                // Implement the logical frame counter right next to the regular one in CTimer::Update
                pattern = hook::pattern("FF 05 ? ? ? ? F3 0F 2C C0 F3 0F 10 05");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CTimer__Update_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        ++*CTimer::m_frameCount;

                        // Logical frame counter
                        static double logicalFrameTime = 0.0;

                        CTimerExt::m_logicalFramesPassed = 0;

                        logicalFrameTime += *(float*)(regs.esp + 0x14 - 0x10) * 1000.0;

                        while (logicalFrameTime >= (1000.0 / 30.0))
                        {
                            logicalFrameTime -= (1000.0 / 30.0);

                            ++CTimerExt::m_logicalFramesPassed;
                        }

                        CTimerExt::m_logicalFrameCounter += CTimerExt::m_logicalFramesPassed;
                    });
                }
                else
                {
                    pattern = hook::pattern("83 05 ? ? ? ? ? D9 3C 24");
                    injector::MakeNOP(pattern.get_first(0), 7, true);
                    static auto CTimer__Update_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        ++*CTimer::m_frameCount;

                        // Logical frame counter
                        static double logicalFrameTime = 0.0;

                        CTimerExt::m_logicalFramesPassed = 0;

                        logicalFrameTime += *(float*)(regs.esp + 0x10 - 0xC) * 1000.0;

                        while (logicalFrameTime >= (1000.0 / 30.0))
                        {
                            logicalFrameTime -= (1000.0 / 30.0);

                            ++CTimerExt::m_logicalFramesPassed;
                        }

                        CTimerExt::m_logicalFrameCounter += CTimerExt::m_logicalFramesPassed;
                    });
                }
            }

            // Fix frame rate issues caused by the original frame counter
            // Note: There are 111 instances of the original counter across the executable. It's likely at least half of those need replacing with the logical one MANUALLY. Craziness...
            {
                // UI animations
                {
                    // Cop blips' animation speed
                    auto pattern = find_pattern("A1 ? ? ? ? 6B C0 ? 53", "A1 ? ? ? ? 6B C0 ? C1 EA");
                    injector::MakeNOP(pattern.get_first(0), 5, true);
                    static auto CVehicle__UpdateChaseRadarBlip_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.eax = CTimerExt::GetLogicalFrameCounter();
                    });

                    // Saving/Loading spinner animation speed
                    pattern = find_pattern("8B 0D ? ? ? ? F3 0F 11 44 24 ? 39 0D", "8B 0D ? ? ? ? 39 0D ? ? ? ? 74");
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CHelpMessage__DrawTextMessages_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.ecx = CTimerExt::GetLogicalFrameCounter();
                    });
                }

                // Misc
                {
                    // Helicopter blinkers' speed
                    auto pattern = hook::pattern("03 0D ? ? ? ? F3 0F 10 0D");
                    if (!pattern.empty())
                    {
                        injector::MakeNOP(pattern.get_first(0), 6, true);
                        static auto CHeli__PreRender2_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.ecx += CTimerExt::GetLogicalFrameCounter();
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("8B 15 ? ? ? ? F3 0F 10 15 ? ? ? ? F3 0F 10 0D");
                        injector::MakeNOP(pattern.get_first(0), 6, true);
                        static auto CHeli__PreRender2_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.edx = CTimerExt::GetLogicalFrameCounter();
                        });
                    }
                }
            }

            // Improve physics (Affects automobile physics, object physics, Euphoria ragdolls, etc.)
            {
                auto pattern = hook::pattern("51 56 E8 ? ? ? ? E8");
                CPhysics::shUpdate = safetyhook::create_inline(pattern.get_first(0), CPhysics::Update);
            }

            // Fix water physics and effects
            {
                // Helicopter downwash force
                auto pattern = find_pattern("83 3D ? ? ? ? ? 74 ? A1 ? ? ? ? 3B 05 ? ? ? ? 75 ? 83 3D ? ? ? ? ? 74 ? 33 C0 EB ? B8 ? ? ? ? 3A 44 24",
                                            "B8 ? ? ? ? 39 05 ? ? ? ? 74 ? 8B 0D ? ? ? ? 3B 0D ? ? ? ? 75 ? 83 3D ? ? ? ? ? 74 ? 33 C0 3A 44 24");
                CWater::shAddToDynamicWaterSpeed = safetyhook::create_inline(pattern.get_first(0), CWater::AddToDynamicWaterSpeed);

                // Buoyancy (Affects everything floating on any body of water that is flagged as physical)
                pattern = find_pattern("83 3D ? ? ? ? ? 74 ? A1 ? ? ? ? 3B 05 ? ? ? ? 75 ? 83 3D ? ? ? ? ? 74 ? 33 C0 EB ? B8 ? ? ? ? 8A 4C 24",
                                       "B8 ? ? ? ? 39 05 ? ? ? ? 74 ? 8B 0D ? ? ? ? 3B 0D ? ? ? ? 75 ? 83 3D ? ? ? ? ? 74 ? 33 C0 53");
                CWater::shModifyDynamicWaterSpeed = safetyhook::create_inline(pattern.get_first(0), CWater::ModifyDynamicWaterSpeed);

                // Helicopter downwash wind particles
                pattern = hook::pattern("F7 F7 85 D2 75");
                if (!pattern.empty())
                {
                    static auto CVehicleFx__UpdateFxHeliDownwash_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        float f = std::min(*CTimer::fTimeStep / (1.0f / 30.0f), 1.0f);

                        regs.edi = std::max((int)((float)regs.edi / f), 1);
                    });
                }
                else
                {
                    pattern = hook::pattern("F7 F1 85 D2 75 ? D9 44 24");
                    static auto CVehicleFx__UpdateFxHeliDownwash_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        float f = std::min(*CTimer::fTimeStep / (1.0f / 30.0f), 1.0f);

                        regs.ecx = std::max((int)((float)regs.ecx / f), 1);
                    });
                }

                // Boat ripple particles
                pattern = hook::pattern("F7 F1 85 D2 75 ? 8B 45");
                static auto CWaterFx__RegisterWakePoint_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    float f = std::min(*CTimer::fTimeStep / (1.0f / 30.0f), 1.0f);

                    regs.ecx = std::max((int)((float)regs.ecx / f), 1);
                });

                // Swim ripple particles
                pattern = find_pattern("F7 F1 85 D2 0F 85 ? ? ? ? FF 74 24", "F7 F1 85 D2 0F 85 ? ? ? ? 8B 54 24");
                static auto CBuoyancy__ProcessSplashVfx_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    float f = std::min(*CTimer::fTimeStep / (1.0f / 30.0f), 1.0f);

                    regs.ecx = std::max((int)((float)regs.ecx / f), 1);
                });
            }

            // Fix NPC pathfinding
            {
                auto pattern = find_pattern("83 EC ? 56 8B F1 57 8B 46 ? C7 44 24", "55 8B EC 83 E4 ? 83 EC ? 56 8B F1 8B 46 ? 8B 88 ? ? ? ? 57");
                CPedIntelligence::shProcessStaticCounter = safetyhook::create_inline(pattern.get_first(0), CPedIntelligence::ProcessStaticCounter);
            }

            // Fix stalls when trying to climb ladders
            // Change the heading angle threshold in CTaskComplexClimbLadder passed to CTaskSimpleMoveAchieveHeading to match the threshold checked by CTaskSimpleSlideToCoord. Fixes the wrong order of tasks being completed at high FPS, causing said stalls.
            {
                auto pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 83 EC ? 8B C8 F3 0F 11 44 24 ? F3 0F 10 44 24 ? C7 44 24 ? ? ? ? ? F3 0F 11 04 24 E8 ? ? ? ? EB");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CTaskComplexClimbLadder__CreateSubTask_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm0.f32[0] = 0.1f; // 0.02f --> 0.1f to match CTaskSimpleSlideToCoord
                    });
                }
                else
                {
                    static float flt_DD5CFC = 0.1f;
                    pattern = hook::pattern("D9 05 ? ? ? ? 83 EC ? D9 5C 24 ? 8B C8 D9 05 ? ? ? ? D9 5C 24 ? D9 44 24 ? D9 1C 24 E8 ? ? ? ? EB ? 33 C0 D9 EE 51 D9 1C 24 8B CE");
                    injector::WriteMemory(pattern.get_first(2), &flt_DD5CFC, true); // 0.02f --> 0.1f to match CTaskSimpleSlideToCoord
                }
            }

            // Fix vehicle steer biases while drunk
            {
                // Automobiles
                auto pattern = hook::pattern("F3 0F 58 8F ? ? ? ? 0F 2F C8");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CAutomobile__ProcessControlInputs_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] += *(float*)(regs.edi + 0x1084) * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 58 86 ? ? ? ? F3 0F 58 C1 F3 0F 10 0D");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CAutomobile__ProcessControlInputs_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm0.f32[0] += *(float*)(regs.esi + 0x10D4) * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }

                // Bikes
                pattern = hook::pattern("F3 0F 59 15 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 58 96");
                if (!pattern.empty())
                {
                    static auto dword_FE8830 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CBike__ProcessControlInputs_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] *= *dword_FE8830 * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 58 86 ? ? ? ? F3 0F 10 0D ? ? ? ? 0F 2F C1 F3 0F 11 86 ? ? ? ? 77");
                    static auto dword_D95B68 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CBike__ProcessControlInputs_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm0.f32[0] *= *dword_D95B68 * *CTimer::fTimeStep / (1.0f / 30.0f);
                    });
                }
            }

            // Fix camera animations
            {
                // Camera shake
                // The function is used by several cameras and not only cameras. Even helicopter searchlights use it so that the light doesn't stay so glued to the player. Pretty cool.
                {
                    auto pattern = find_pattern("55 8B EC 83 E4 ? 83 EC ? 56 57 8B F9 F3 0F 10 05", "55 8B EC 83 E4 ? 0F 57 E4 F3 0F 10 1D");
                    CHandShaker::shProcess = safetyhook::create_inline(pattern.get_first(0), CHandShaker::Process);
                }

                // Vehicle first person "hood-mode" camera bumps
                {
                    auto pattern = find_pattern("55 8B EC 83 E4 ? 83 EC ? F3 0F 10 05 ? ? ? ? 56 8B 75 ? 57 6A", "55 8B EC 83 E4 ? 83 EC ? F3 0F 10 05 ? ? ? ? 53 8B 5D ? 56 57 8B 7D ? 6A");
                    CCamFollowVehicle::shSprungMounting = safetyhook::create_inline(pattern.get_first(0), CCamFollowVehicle::SprungMounting);
                }

                // Handbrake Cam swings
                {
                    auto pattern = find_pattern("55 8B EC 83 E4 ? 83 EC ? 83 3D ? ? ? ? ? 56 57 89 4C 24 ? C7 44 24", "55 8B EC 83 E4 ? 83 EC ? 83 3D ? ? ? ? ? 0F 57 C0 53");
                    CCamFollowVehicle::shProcessHandBrakeSwing = safetyhook::create_inline(pattern.get_first(0), CCamFollowVehicle::ProcessHandBrakeSwing);
                }

                // CCamFollowVehicle auto centering force
                // Skips some clamps set in the vehicle camera code that prevent auto centering from scaling properly with the frame rate (These also seem to be done on Xbox, maybe this is just north vision?)
                {
                    auto pattern = find_pattern("77 ? 0F 28 C2 F3 0F 5C 8F", "77 ? 0F 28 D3 F3 0F 10 8E");
                    if (!pattern.empty())
                    {
                        injector::MakeNOP(pattern.get_first(0), 2, true);
                    }

                    pattern = find_pattern("76 ? 0F 28 C8 EB ? F3 0F 10 4C 24 ? 80 7C 24", "76 ? 0F 28 CE EB ? 0F 28 CF 84 D2");
                    if (!pattern.empty())
                    {
                        injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                    }
                }

                // Aim zooming
                {
                    // Weapons
                    auto pattern = find_pattern("F3 0F 10 15 ? ? ? ? F3 0F 59 CA F3 0F 58 4E", "F3 0F 10 15 ? ? ? ? F3 0F 5C C1 F3 0F 59 C2 F3 0F 58 C1 74");
                    static auto dword_FE8830 = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCamAimWeapon__AimFree_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm2.f32[0] = 1.0f - powf(1.0f - *dword_FE8830, *CTimer::fTimeStep / (1.0f / 30.0f));
                    });

                    // Melee
                    pattern = hook::pattern("F3 0F 59 0D ? ? ? ? F3 0F 58 4C 24 ? F3 0F 11 49");
                    if (!pattern.empty())
                    {
                        static auto dword_FE8830 = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CCamAimWeapon__AimFree_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm1.f32[0] *= 1.0f - powf(1.0f - *dword_FE8830, *CTimer::fTimeStep / (1.0f / 30.0f));
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 58 C1 F3 0F 11 41");
                        static auto dword_DB6F80 = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CCamAimWeapon__AimFree_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm0.f32[0] *= 1.0f - powf(1.0f - *dword_DB6F80, *CTimer::fTimeStep / (1.0f / 30.0f));
                        });
                    }
                }
            }

            // Fix vehicle issues
            {
                // Helicopter rotor/tail break times
                {
                    // Main rotors
                    auto pattern = hook::pattern("F3 0F 59 15 ? ? ? ? F3 0F 58 D0 F3 0F 10 87");
                    if (!pattern.empty())
                    {
                        static auto dword_FE8830 = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CHeli__ApplyCollisionInternal_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm2.f32[0] *= *dword_FE8830 * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("D8 0D ? ? ? ? D9 44 24 ? D8 0D");
                        static auto dword_D95B68 = *pattern.get_first<float*>(2);
                        struct CHeli__ApplyCollisionInternal_Hook1
                        {
                            void operator()(injector::reg_pack& regs)
                            {
                                float f = *dword_D95B68 * *CTimer::fTimeStep / (1.0f / 30.0f);

                                _asm { fmul dword ptr [f] };
                            }
                        }; injector::MakeInline<CHeli__ApplyCollisionInternal_Hook1>(pattern.get_first(0), pattern.get_first(6));
                    }

                    // Rear rotors
                    pattern = hook::pattern("F3 0F 59 D0 F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? F3 0F 58 D0");
                    if (!pattern.empty())
                    {
                        injector::MakeNOP(pattern.get_first(0), 4, true);
                        static auto CHeli__ApplyCollisionInternal_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm2.f32[0] *= regs.xmm0.f32[0] * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });

                        // Scale the addend (mulss xmm0, [rate]; addss xmm2, xmm0) like main rotors.
                        // Hook2 only scaled xmm2 *= xmm0; the per-frame increment stayed fps-dependent.
                        static auto rear_break_rate = *pattern.get_first<float*>(14);
                        injector::MakeNOP(pattern.get_first(10), 8, true);
                        static auto CHeli__ApplyCollisionInternal_Hook2b = safetyhook::create_mid(pattern.get_first(10), [](SafetyHookContext& regs)
                        {
                            regs.xmm0.f32[0] *= *rear_break_rate * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("DE C9 D9 44 24 ? D8 0D");
                        struct CHeli__ApplyCollisionInternal_Hook2
                        {
                            void operator()(injector::reg_pack& regs)
                            {
                                float f = *CTimer::fTimeStep / (1.0f / 30.0f);

                                _asm
                                {
                                    fld [f]

                                    fmulp st(1), st
                                    fmulp st(1), st

                                    fld [esp + 0x100 - 0xEC]
                                };
                            }
                        }; injector::MakeInline<CHeli__ApplyCollisionInternal_Hook2>(pattern.get_first(0), pattern.get_first(6));
                    }

                    // Tails ???
                    pattern = hook::pattern("F3 0F 59 0D ? ? ? ? F3 0F 59 4C 24 ? F3 0F 5C C1");
                    if (!pattern.empty())
                    {
                        static auto dword_FE8AE0 = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CHeli__ApplyCollisionInternal_Hook3 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm1.f32[0] *= *dword_FE8AE0 * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("D8 0D ? ? ? ? D8 4C 24 ? D9 5C 24 ? F3 0F 5C 44 24");
                        static auto flt_D7A9C8 = *pattern.get_first<float*>(2);
                        struct CHeli__ApplyCollisionInternal_Hook3
                        {
                            void operator()(injector::reg_pack& regs)
                            {
                                float f = *flt_D7A9C8 * *CTimer::fTimeStep / (1.0f / 30.0f);

                                _asm { fmul dword ptr [f] };
                            }
                        }; injector::MakeInline<CHeli__ApplyCollisionInternal_Hook3>(pattern.get_first(0), pattern.get_first(6));
                    }
                }

                // Helicopter rotor speeds
                {
                    auto pattern = hook::pattern("F3 0F 59 05 ? ? ? ? F3 0F 59 C4 F3 0F 5C C8");
                    if (!pattern.empty())
                    {
                        static auto dword_1046AF0 = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CHeli__PreRender_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm0.f32[0] *= *dword_1046AF0 * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("F3 0F 59 15 ? ? ? ? F3 0F 59 D4");
                        static auto dword_F46598 = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CHeli__PreRender_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm2.f32[0] *= *dword_F46598 * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }

                    pattern = hook::pattern("F3 0F 59 1D ? ? ? ? F3 0F 10 87 ? ? ? ? F3 0F 59 DC");
                    if (!pattern.empty())
                    {
                        static auto dword_1046AF4 = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CHeli__PreRender_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm3.f32[0] *= *dword_1046AF4 * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("F3 0F 59 0D ? ? ? ? F3 0F 10 86 ? ? ? ? F3 0F 59 CC");
                        static auto dword_F46594 = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CHeli__PreRender_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm1.f32[0] *= *dword_F46594 * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }
                }
            }

            // Fix UI issues
            {
                // Loading screen animations' speed
                {
                    // Fix Y axis loading screen animations not scaling properly with the frame rate. Seems like the devs forgot to also scale this.
                    auto pattern = find_pattern("F3 0F 58 2D ? ? ? ? F3 0F 11 AC 18", "F3 0F 58 15 ? ? ? ? F3 0F 11 94 37");
                    static auto dword_18B6F30 = *pattern.get_first<float*>(4);

                    pattern = hook::pattern("F3 0F 58 EC F3 0F 11 AC 18");
                    if (!pattern.empty())
                    {
                        injector::MakeNOP(pattern.get_first(0), 4, true);
                        static auto CLoadingScreens__RenderSegmentSprites_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm5.f32[0] += *dword_18B6F30; // regs.xmm5.f32[0] --> dword_18B6F30
                        });
                    }

                    // Fix loading screen animations running at double the intended speed.
                    // This was made into an issue on PC because the devs thought capping loading screens to 64fps would be a good idea,
                    // so the loading screen clocks (Which actually affect the overall animation speed instead of a fixed rate) were also tied to that target frame rate,
                    // which essentially doubles the speed of loading screen animations.
                    //
                    // On consoles, there are no such clocks in place. Instead, loading screen animation speed is entirely fps dependent (On both axis),
                    // and this means the Xbox 360 version also exhibits the same issue as PC essentially, due to the 60fps target with VSync even on real hardware.
                    //
                    // The PS3 version, being capped at 30fps, behaves correctly here. So we change these clocks to be 33.3f so that it matches the 30fps speed.
                    // As a side effect however, this significantly slows down PC loading screens, so these should probably have their speed values doubled in the .dat files instead.
                    pattern = hook::pattern("F3 0F 59 0D ? ? ? ? C7 84 18");
                    if (!pattern.empty())
                    {
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CLoadingScreens__RenderSegmentSprites_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm1.f32[0] *= 33.3f; // 66.6f --> 33.3f
                        });

                        pattern = hook::pattern("F3 0F 59 25 ? ? ? ? C7 84 18");
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CLoadingScreens__RenderSegmentSprites_Hook3 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm4.f32[0] *= 33.3f; // 66.6f --> 33.3f
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("F3 0F 10 15 ? ? ? ? F3 0F 59 E3 F3 0F 59 E2");
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CLoadingScreens__RenderSegmentSprites_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm2.f32[0] = 33.3f; // 66.6f --> 33.3f
                        });

                        pattern = hook::pattern("F3 0F 10 15 ? ? ? ? F3 0F 10 A4 37");
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CLoadingScreens__RenderSegmentSprites_Hook3 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm2.f32[0] = 33.3f; // 66.6f --> 33.3f
                        });
                    }
                }

                // Loading text animations' speed
                {
                    // Loading text flashing (IV and TLAD)
                    // Skips an else path which post-processes the flash speed unnecessarily.
                    // The speed was previously fixed to work correctly at variable frame rates on patches 1050 and lower,
                    // but this check added in patch 1060 along with TBoGT's sparks prevents it from working properly.
                    auto pattern = hook::pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 C1 F3 0F 11 05 ? ? ? ? EB");
                    if (!pattern.empty())
                    {
                        injector::MakeNOP(pattern.get_first(0), 20, true);
                    }
                    else
                    {
                        pattern = hook::pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 05 ? ? ? ? F3 0F 11 05 ? ? ? ? EB");
                        injector::MakeNOP(pattern.get_first(0), 24, true);
                    }

                    // This just slightly corrects the devs' old fix from 1050 and lower to be fully accurate to what we generally do here.
                    pattern = hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 58 05");
                    if (!pattern.empty())
                    {
                        injector::MakeNOP(pattern.get_first(0), 22, true);
                        static auto CRenderThreadInterface__LoadingRenderFunction_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            // Equivalent to: dword_1175C40 += dword_1032790 * SomeTimer * (1000.0f / 30.0f);
                            regs.xmm0.f32[0] = *(float*)(regs.esp + 0x34);
                            regs.xmm0.f32[0] *= (1000.0f / 30.0f);
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 58 05");
                        injector::MakeNOP(pattern.get_first(0), 14, true);
                        static auto CRenderThreadInterface__LoadingRenderFunction_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            // Equivalent to: dword_11FB434 += dword_F38420 * SomeTimer * (1000.0f / 30.0f);
                            regs.xmm0.f32[0] = *(float*)(regs.esp + 0x0EE0 - 0xE9C);
                            regs.xmm0.f32[0] *= (1000.0f / 30.0f);
                        });
                    }

                    // Loading text sparks (TBoGT)
                    // So for IV's flashing we had an initial speed and then a secondary speed (Which was unnecessary).
                    // Here its similar, however the secondary speed might actually drive everything as the initial one is just * 0.001f while the secondary one is * 0.085f.
                    // So we're just taking the timer variable Toronto used for IV in 1040 and scale the secondary value with that.
                    pattern = hook::pattern("F3 0F 58 0D ? ? ? ? 0F 5B C0 F3 0F 11 0D");
                    if (!pattern.empty())
                    {
                        static auto dword_E81598 = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CRenderThreadInterface__LoadingRenderFunction_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            // Equivalent to: dword_1175770 += dword_E81598 * SomeTimer * (1000.0f / 30.0f);
                            regs.xmm1.f32[0] += *dword_E81598 * *(float*)(regs.esp + 0x34) * (1000.0f / 30.0f);
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("F3 0F 58 05 ? ? ? ? F3 0F 2A 0D");
                        static auto flt_DEF584 = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CRenderThreadInterface__LoadingRenderFunction_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            // Equivalent to: dword_11FB41C += flt_DEF584 * SomeTimer * (1000.0f / 30.0f);
                            regs.xmm0.f32[0] += *flt_DEF584 * *(float*)(regs.esp + 0x0EE0 - 0xE9C) * (1000.0f / 30.0f);
                        });
                    }
                }

                // Radar zoom animations' speed
                {
                    // This skips a check that gates how often the entire radar update code is updated.
                    // Normally it updates every 30 milliseconds, which visually makes the entire radar always update at a 30hz rate regardless of frame rate.
                    auto pattern = find_pattern("0F 86 ? ? ? ? F3 0F 10 15 ? ? ? ? 0F 2E CA", "0F 86 ? ? ? ? F3 0F 10 0D ? ? ? ? 0F 2E C1");
                    injector::MakeNOP(pattern.get_first(0), 6, true);

                    // This makes the radar zoom-in speed fps independent, as we removed the update throttle and thus made this an issue.
                    pattern = hook::pattern("F3 0F 58 15 ? ? ? ? 0F 2F CA EB");
                    if (!pattern.empty())
                    {
                        static auto CRadar__fRange = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CRadarNY__UpdateTask_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm2.f32[0] += *CRadar__fRange * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("F3 0F 58 0D ? ? ? ? 0F 2F C1 EB");
                        static auto CRadar__fRange = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CRadarNY__UpdateTask_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm1.f32[0] += *CRadar__fRange * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }

                    // This makes the radar zoom-out speed fps independent, as we removed the update throttle and thus also made this an issue.
                    pattern = hook::pattern("F3 0F 5C 15 ? ? ? ? 0F 2F D1 76 ? 0F 28 CA");
                    if (!pattern.empty())
                    {
                        static auto CRadar__fRange = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CRadarNY__UpdateTask_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm2.f32[0] -= *CRadar__fRange * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("F3 0F 5C 0D ? ? ? ? 0F 2F C8 76 ? 0F 28 C1");
                        static auto CRadar__fRange = *pattern.get_first<float*>(4);
                        injector::MakeNOP(pattern.get_first(0), 8, true);
                        static auto CRadarNY__UpdateTask_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            regs.xmm1.f32[0] -= *CRadar__fRange * *CTimer::fTimeStep / (1.0f / 30.0f);
                        });
                    }
                }
            }

            // Native patches
            hbSLIDE_OBJECT.fun = NativeOverride::Register(Natives::NativeHashes::SLIDE_OBJECT, NATIVE_SLIDE_OBJECT_1, "E8 ? ? ? ? 0F B6 C8", 107);
            if (!hbSLIDE_OBJECT.fun)
            {
                auto pattern = hook::pattern("55 8B EC 83 E4 ? 8B 45 ? 8B 0D ? ? ? ? 81 EC ? ? ? ? 56");
                shNATIVE_SLIDE_OBJECT = safetyhook::create_inline(pattern.get_first(0), NATIVE_SLIDE_OBJECT_2);
            }
        };
    }
} FramerateVigilante;