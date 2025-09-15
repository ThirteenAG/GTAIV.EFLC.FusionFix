module;

#include <common.hxx>
#include "ledsdk/LogitechLEDLib.h"

export module led;

import common;
import settings;
import natives;

bool bLogiLedInitialized = false;

void AmbientLighting(bool bForce = false)
{
    static auto prevCurrentEpisode = -1;
    if (Natives::GetCurrentEpisode() != prevCurrentEpisode || bForce)
    {
        LogiLedSetTargetDevice(LOGI_DEVICETYPE_ALL);

        if (Natives::GetCurrentEpisode() == 0)
        {
            if (CText::hasViceCityStrings())
                LogiLedSetLighting(99, 41, 98);
            else
                LogiLedSetLighting(94, 63, 0);
        }
        else if (Natives::GetCurrentEpisode() == 1)
        {
            LogiLedSetLighting(45, 5, 6);
        }
        if (Natives::GetCurrentEpisode() == 2)
        {
            static unsigned char TBoGTColor[LOGI_LED_BITMAP_SIZE] = {
                0x16, 0xa0, 0xce, 0xff, 0x16, 0xa0, 0xce, 0xff, 0x12, 0xa2, 0xcd, 0xff, 0x33, 0x7f, 0xd4, 0xff, 0x45, 0x44, 0xda, 0xff,
                0x43, 0x4b, 0xda, 0xff, 0x48, 0x48, 0xda, 0xff, 0x6b, 0x1b, 0xd7, 0xff, 0x6e, 0x0f, 0xd7, 0xff, 0x6d, 0x10, 0xd9, 0xff,
                0x7c, 0x19, 0xc3, 0xff, 0x8a, 0x1f, 0xaa, 0xff, 0x89, 0x1e, 0xac, 0xff, 0x8a, 0x23, 0xa9, 0xff, 0x92, 0x40, 0x87, 0xff,
                0x93, 0x42, 0x82, 0xff, 0x8f, 0x39, 0x84, 0xff, 0xb7, 0x7f, 0x6f, 0xff, 0xd6, 0xa7, 0x52, 0xff, 0xd4, 0xa4, 0x55, 0xff,
                0xd4, 0xa4, 0x55, 0xff, 0x16, 0xa0, 0xce, 0xff, 0x16, 0xa0, 0xce, 0xff, 0x12, 0xa2, 0xcd, 0xff, 0x33, 0x7f, 0xd4, 0xff,
                0x45, 0x44, 0xdb, 0xff, 0x43, 0x4b, 0xda, 0xff, 0x48, 0x48, 0xda, 0xff, 0x6b, 0x1b, 0xd7, 0xff, 0x6e, 0x0f, 0xd7, 0xff,
                0x6d, 0x10, 0xd9, 0xff, 0x7c, 0x19, 0xc3, 0xff, 0x8a, 0x1f, 0xaa, 0xff, 0x89, 0x1e, 0xac, 0xff, 0x8a, 0x23, 0xa9, 0xff,
                0x92, 0x40, 0x87, 0xff, 0x93, 0x42, 0x82, 0xff, 0x8f, 0x39, 0x85, 0xff, 0xb7, 0x7f, 0x6f, 0xff, 0xd6, 0xa7, 0x52, 0xff,
                0xd4, 0xa4, 0x55, 0xff, 0xd4, 0xa4, 0x55, 0xff, 0x16, 0xa0, 0xce, 0xff, 0x16, 0xa0, 0xce, 0xff, 0x12, 0xa2, 0xcd, 0xff,
                0x33, 0x7f, 0xd4, 0xff, 0x45, 0x44, 0xda, 0xff, 0x43, 0x4b, 0xda, 0xff, 0x48, 0x48, 0xda, 0xff, 0x6b, 0x1b, 0xd7, 0xff,
                0x6e, 0x0f, 0xd7, 0xff, 0x6d, 0x10, 0xd9, 0xff, 0x7c, 0x19, 0xc3, 0xff, 0x8a, 0x1f, 0xaa, 0xff, 0x89, 0x1e, 0xac, 0xff,
                0x8a, 0x23, 0xa9, 0xff, 0x92, 0x40, 0x87, 0xff, 0x93, 0x42, 0x82, 0xff, 0x8f, 0x39, 0x84, 0xff, 0xb7, 0x7f, 0x6f, 0xff,
                0xd6, 0xa7, 0x52, 0xff, 0xd4, 0xa4, 0x55, 0xff, 0xd4, 0xa4, 0x55, 0xff, 0x16, 0xa0, 0xce, 0xff, 0x16, 0xa0, 0xce, 0xff,
                0x12, 0xa2, 0xcd, 0xff, 0x33, 0x7f, 0xd4, 0xff, 0x45, 0x44, 0xdb, 0xff, 0x43, 0x4b, 0xda, 0xff, 0x48, 0x48, 0xda, 0xff,
                0x6b, 0x1b, 0xd7, 0xff, 0x6e, 0x0f, 0xd7, 0xff, 0x6d, 0x10, 0xd9, 0xff, 0x7c, 0x19, 0xc3, 0xff, 0x8a, 0x1f, 0xaa, 0xff,
                0x89, 0x1e, 0xac, 0xff, 0x8a, 0x23, 0xa9, 0xff, 0x92, 0x40, 0x87, 0xff, 0x93, 0x42, 0x82, 0xff, 0x8f, 0x39, 0x85, 0xff,
                0xb7, 0x7f, 0x6f, 0xff, 0xd6, 0xa7, 0x52, 0xff, 0xd4, 0xa4, 0x55, 0xff, 0xd4, 0xa4, 0x55, 0xff, 0x16, 0xa0, 0xce, 0xff,
                0x16, 0xa0, 0xce, 0xff, 0x12, 0xa2, 0xcd, 0xff, 0x33, 0x7f, 0xd4, 0xff, 0x45, 0x44, 0xdb, 0xff, 0x43, 0x4b, 0xda, 0xff,
                0x48, 0x48, 0xda, 0xff, 0x6b, 0x1b, 0xd7, 0xff, 0x6e, 0x0f, 0xd7, 0xff, 0x6d, 0x10, 0xd9, 0xff, 0x7c, 0x19, 0xc3, 0xff,
                0x8a, 0x1f, 0xaa, 0xff, 0x89, 0x1e, 0xac, 0xff, 0x8a, 0x23, 0xa9, 0xff, 0x92, 0x40, 0x87, 0xff, 0x93, 0x42, 0x82, 0xff,
                0x8f, 0x39, 0x85, 0xff, 0xb7, 0x7f, 0x6f, 0xff, 0xd6, 0xa7, 0x52, 0xff, 0xd4, 0xa4, 0x55, 0xff, 0xd4, 0xa4, 0x55, 0xff,
                0x16, 0xa0, 0xce, 0xff, 0x16, 0xa0, 0xce, 0xff, 0x12, 0xa2, 0xcd, 0xff, 0x33, 0x7f, 0xd4, 0xff, 0x45, 0x44, 0xdb, 0xff,
                0x43, 0x4b, 0xda, 0xff, 0x48, 0x48, 0xda, 0xff, 0x6b, 0x1b, 0xd7, 0xff, 0x6e, 0x0f, 0xd7, 0xff, 0x6d, 0x10, 0xd9, 0xff,
                0x7c, 0x19, 0xc3, 0xff, 0x8a, 0x1f, 0xaa, 0xff, 0x89, 0x1e, 0xac, 0xff, 0x8a, 0x23, 0xa9, 0xff, 0x92, 0x40, 0x87, 0xff,
                0x93, 0x42, 0x82, 0xff, 0x8f, 0x39, 0x85, 0xff, 0xb7, 0x7f, 0x6f, 0xff, 0xd6, 0xa7, 0x52, 0xff, 0xd4, 0xa4, 0x55, 0xff,
                0xd4, 0xa4, 0x55, 0xff,
            };

            LogiLedSetLighting(51, 26, 58);
            LogiLedSetLightingFromBitmap(TBoGTColor);
        }
    }
    prevCurrentEpisode = Natives::GetCurrentEpisode();
}

static auto oldWantedLevel = 0;
void WantedLevelSiren(bool bForce = false)
{
    static float TIMERA = 0.0f;

    bool bCarSirenActive = false;
    Ped pPlayerPed = 0;
    Vehicle pPlayerCar = 0;
    Natives::GetPlayerChar(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId()), &pPlayerPed);
    if (pPlayerPed) {
        Natives::GetCarCharIsUsing(pPlayerPed, &pPlayerCar);
        if (pPlayerCar)
            bCarSirenActive = Natives::IsCarSirenOn(pPlayerCar);
    }

    uint32_t WantedLevel = 0;
    Natives::StoreWantedLevel(0, &WantedLevel);
    if (WantedLevel != oldWantedLevel || bForce)
    {
        if (WantedLevel > 0 || bCarSirenActive)
        {
            int duration = 1200;
            switch (WantedLevel)
            {
            case 0:
                duration = 1200;
                break;
            case 1:
                duration = 1600;
                break;
            case 2:
                duration = 1400;
                break;
            case 3:
                duration = 1200;
                break;
            case 4:
                duration = 1000;
                break;
            default:
                duration = 800;
                break;
            }

            int redVal = 100;
            int greenVal = 0;
            int blueVal = 0;

            int redFinishVal = 0;
            int greenFinishVal = 0;
            int blueFinishVal = 100;

            LogiLedPulseSingleKey(LogiLed::KeyName::F1, redFinishVal, greenFinishVal, blueFinishVal, redVal, greenVal, blueVal, duration, true);
            LogiLedPulseSingleKey(LogiLed::KeyName::F2, redFinishVal, greenFinishVal, blueFinishVal, redVal, greenVal, blueVal, duration, true);
            LogiLedPulseSingleKey(LogiLed::KeyName::F3, redFinishVal, greenFinishVal, blueFinishVal, redVal, greenVal, blueVal, duration, true);
            LogiLedPulseSingleKey(LogiLed::KeyName::F4, redFinishVal, greenFinishVal, blueFinishVal, redVal, greenVal, blueVal, duration, true);

            LogiLedPulseSingleKey(LogiLed::KeyName::F5, 100, 100, 100, 100, 100, 100, duration, true);
            LogiLedPulseSingleKey(LogiLed::KeyName::F6, 100, 100, 100, 100, 100, 100, duration, true);
            LogiLedPulseSingleKey(LogiLed::KeyName::F7, 100, 100, 100, 100, 100, 100, duration, true);
            LogiLedPulseSingleKey(LogiLed::KeyName::F8, 100, 100, 100, 100, 100, 100, duration, true);

            LogiLedPulseSingleKey(LogiLed::KeyName::F9, redVal, greenVal, blueVal, redFinishVal, greenFinishVal, blueFinishVal,  duration, true);
            LogiLedPulseSingleKey(LogiLed::KeyName::F10, redVal, greenVal, blueVal, redFinishVal, greenFinishVal, blueFinishVal, duration, true);
            LogiLedPulseSingleKey(LogiLed::KeyName::F11, redVal, greenVal, blueVal, redFinishVal, greenFinishVal, blueFinishVal, duration, true);
            LogiLedPulseSingleKey(LogiLed::KeyName::F12, redVal, greenVal, blueVal, redFinishVal, greenFinishVal, blueFinishVal, duration, true);
        }
        else
        {
            LogiLedStopEffectsOnKey(LogiLed::KeyName::F1);
            LogiLedStopEffectsOnKey(LogiLed::KeyName::F2);
            LogiLedStopEffectsOnKey(LogiLed::KeyName::F3);
            LogiLedStopEffectsOnKey(LogiLed::KeyName::F4);

            LogiLedStopEffectsOnKey(LogiLed::KeyName::F5);
            LogiLedStopEffectsOnKey(LogiLed::KeyName::F6);
            LogiLedStopEffectsOnKey(LogiLed::KeyName::F7);
            LogiLedStopEffectsOnKey(LogiLed::KeyName::F8);

            LogiLedStopEffectsOnKey(LogiLed::KeyName::F9);
            LogiLedStopEffectsOnKey(LogiLed::KeyName::F10);
            LogiLedStopEffectsOnKey(LogiLed::KeyName::F11);
            LogiLedStopEffectsOnKey(LogiLed::KeyName::F12);

            AmbientLighting(true);
        }
    }
    oldWantedLevel = WantedLevel;

    static bool bColorRed = false;
    if (TIMERA >= 400)
    {
        if (WantedLevel > 0 || bCarSirenActive)
        {
            int redVal = 100;
            int greenVal = 0;
            int blueVal = 0;

            int redFinishVal = 0;
            int greenFinishVal = 0;
            int blueFinishVal = 100;

            LogiLedSetTargetDevice(LOGI_DEVICETYPE_RGB);
            if (bColorRed)
                LogiLedPulseLighting(redVal, greenVal, blueVal, 400, 400);
            else
                LogiLedPulseLighting(redFinishVal, greenFinishVal, blueFinishVal, 400, 400);
            LogiLedSetTargetDevice(LOGI_DEVICETYPE_ALL);
        }

        bColorRed = !bColorRed;
        TIMERA = 0.0f;
    }
    else
    {
        TIMERA += 1000.0f * Natives::Timestep();
    }
}

void AmmoInClip()
{
    static std::vector<LogiLed::KeyName> keys = {
        LogiLed::KeyName::TILDE, LogiLed::KeyName::ONE, LogiLed::KeyName::TWO, LogiLed::KeyName::THREE,
        LogiLed::KeyName::FOUR,LogiLed::KeyName::FIVE,LogiLed::KeyName::SIX,LogiLed::KeyName::SEVEN,
        LogiLed::KeyName::EIGHT,LogiLed::KeyName::NINE,LogiLed::KeyName::ZERO,LogiLed::KeyName::MINUS,
        LogiLed::KeyName::EQUALS,
    };

    Ped pPlayerPed = 0;
    uint32_t pPlayerWeapon = 0;
    Natives::GetPlayerChar(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId()), &pPlayerPed);
    if (pPlayerPed)
        Natives::GetCurrentCharWeapon(pPlayerPed, &pPlayerWeapon);
    
    if (pPlayerPed && pPlayerWeapon)
    {
        static auto oldAmmoInClip = -1;
        uint32_t ammoInClip = 0;
        Natives::GetAmmoInClip(pPlayerPed, pPlayerWeapon, &ammoInClip);
        if (ammoInClip != oldAmmoInClip)
        {
            uint32_t maxAmmoInClip = -1;
            Natives::GetMaxAmmoInClip(pPlayerPed, pPlayerWeapon, &maxAmmoInClip);
            float ammoInClipPercent = ((float)ammoInClip / (float)maxAmmoInClip) * 100.0f;

            for (size_t i = 0; i < keys.size(); i++)
            {
                auto indexInPercent = ((float)i / (float)keys.size()) * 100.0f;
                if (ammoInClipPercent > indexInPercent)
                    LogiLedSetLightingForKeyWithKeyName(keys[i], 100, 100, 100);
                else
                    LogiLedSetLightingForKeyWithKeyName(keys[i], 10, 10, 10);
            }
        }
        oldAmmoInClip = ammoInClip;
    }
}

void CurrentHealth(bool bForce = false)
{
    static float TIMERA = 0.0f;

    static std::vector<LogiLed::KeyName> keys = {
        LogiLed::KeyName::G_1, LogiLed::KeyName::G_2, LogiLed::KeyName::G_3, LogiLed::KeyName::G_4, LogiLed::KeyName::G_5,
    };

    Ped pPlayerPed = 0;
    uint32_t pPlayerHealth = 0;
    Natives::GetPlayerChar(Natives::ConvertIntToPlayerIndex(Natives::GetPlayerId()), &pPlayerPed);
    if (pPlayerPed)
    {
        Natives::GetCharHealth(pPlayerPed, &pPlayerHealth);
        static uint32_t oldHealth = 0;
        if (pPlayerHealth != oldHealth || bForce)
        {
            int32_t maxPlayerHealth = -1;
            Natives::GetPlayerMaxHealth(Natives::GetPlayerId(), &maxPlayerHealth);
            float healthPercent = (((float)pPlayerHealth - 100.0f) / ((float)maxPlayerHealth - 100.0f)) * 100.0f;

            for (size_t i = 0; i < keys.size(); i++)
            {
                auto indexInPercent = ((float)i / (float)keys.size()) * 100.0f;
                if (healthPercent >= 0 && healthPercent > indexInPercent)
                {
                    if (i == 0 && healthPercent < 20.0f)
                    {
                        LogiLedSetLightingForKeyWithKeyName(keys[i], 100, 0, 0);
                    }
                    else if (Natives::GetCurrentEpisode() == 0)
                    {
                        if (CText::hasViceCityStrings())
                            LogiLedSetLightingForKeyWithKeyName(keys[i], 97, 22, 75);
                        else
                            LogiLedSetLightingForKeyWithKeyName(keys[i], 34, 49, 35);
                    }
                    else if (Natives::GetCurrentEpisode() == 1)
                        LogiLedSetLightingForKeyWithKeyName(keys[i], 59, 61, 61);
                    else if (Natives::GetCurrentEpisode() == 2)
                        LogiLedSetLightingForKeyWithKeyName(keys[i], 86, 86, 87);
                }
                else
                {
                    LogiLedStopEffectsOnKey(LogiLed::KeyName::G_1);
                    LogiLedSetLightingForKeyWithKeyName(keys[i], 0, 0, 0);
                }
            }
        }
        else
        {
            if (TIMERA >= 800)
            {
                int32_t maxPlayerHealth = -1;
                Natives::GetPlayerMaxHealth(Natives::GetPlayerId(), &maxPlayerHealth);
                float healthPercent = (((float)pPlayerHealth - 100.0f) / ((float)maxPlayerHealth - 100.0f)) * 100.0f;

                if (healthPercent >= 0 && healthPercent < 20.0f)
                {
                    LogiLedFlashSingleKey(LogiLed::KeyName::G_1, 100, 0, 0, 0, 400);
                }
                TIMERA = 0.0f;
            }
            else
            {
                TIMERA += 1000.0f * Natives::Timestep();
            }
        }
        oldHealth = pPlayerHealth;
    }
}

void test()
{
    static auto old = 0;
    uint32_t cur = 0;
    if (cur != old)
    {

    }
    old = cur;
}

void ProcessLEDEvents()
{
    AmbientLighting();
    WantedLevelSiren();
    AmmoInClip();
    CurrentHealth();
}

class LED
{
public:
    LED()
    {
        FusionFix::onGameInitEvent() += []()
        {
            if (FusionFixSettings("PREF_LEDILLUMINATION"))
                bLogiLedInitialized = LogiLedInit();

            FusionFixSettings.SetCallback("PREF_LEDILLUMINATION", [](int32_t value) {
                if (value)
                {
                    if (!bLogiLedInitialized) {
                        bLogiLedInitialized = LogiLedInit();
                        if (bLogiLedInitialized)
                            AmbientLighting(true);
                    }
                }
                else if (bLogiLedInitialized) {
                    LogiLedShutdown();
                    bLogiLedInitialized = false;
                }
            });
        };

        FusionFix::onGameProcessEvent() += []()
        {
            if (bLogiLedInitialized)
            {
                ProcessLEDEvents();
            }
        };

        FusionFix::onMenuEnterEvent() += []()
        {
            LogiLedSaveCurrentLighting();
            LogiLedSetTargetDevice(LOGI_DEVICETYPE_ALL);
            AmbientLighting(true);
        };

        FusionFix::onMenuExitEvent() += []()
        {
            oldWantedLevel = -1;
            LogiLedRestoreLighting();
            WantedLevelSiren(true);
            CurrentHealth(true);
        };

        FusionFix::onShutdownEvent() += []()
        {
            if (bLogiLedInitialized) {
                LogiLedShutdown();
                bLogiLedInitialized = false;
            }
        };

        IATHook::Replace(GetModuleHandleA(NULL), "KERNEL32.DLL",
            std::forward_as_tuple("ExitProcess", static_cast<void(__stdcall*)(UINT)>([](UINT uExitCode) {
                if (bLogiLedInitialized) {
                    LogiLedShutdown();
                    bLogiLedInitialized = false;
                }
                ExitProcess(uExitCode);
            }))
        );
    }
} LED;