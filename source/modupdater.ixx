module;

#include <common.hxx>
#include "libmodupdater.h"

export module contributing;

import common;
import settings;

class ModUpdater
{
public:
    static inline bool bInitialized = false;

    static void Initialize()
    {
        if (!bInitialized)
        {
            bInitialized = true;
            HMODULE hm = NULL;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&FusionFix::onInitEvent, &hm);
            muSetUpdateURL(hm, "https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/releases/latest/download/GTAIV.EFLC.FusionFix.zip");
            //muSetDevUpdateURL(hm, "");
            //muSetAlwaysUpdate(hm, true);
            //muSetSkipUpdateCompleteDialog(hm, true);
            muInit();
        }
    }

    ModUpdater()
    {
        FusionFix::onInitEventAsync() += []()
        {
            if (FusionFixSettings.Get("PREF_UPDATE"))
                Initialize();
            
            FusionFixSettings.SetCallback("PREF_UPDATE", [](int32_t value) {
                if (value)
                    Initialize();
            });
        };
    }
} ModUpdater;