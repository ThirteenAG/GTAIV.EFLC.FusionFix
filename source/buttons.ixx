module;

#include <common.hxx>
#include <string>

export module buttons;

import common;
import settings;
import natives;

class Buttons
{
private:
    static inline std::vector<std::string> btnPrefix = {
        "", //XBOX360
        "XBONE_",
        "PS3_",
        "PS4_",
        "PS5_",
        "SWITCH_",
        "SD_",
        "SC_",
    };

    static inline std::vector<std::string> buttons = {
        "UP_ARROW", "DOWN_ARROW", "LEFT_ARROW", "RIGHT_ARROW", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT", "DPAD_NONE",
        "DPAD_ALL", "DPAD_UPDOWN", "DPAD_LEFTRIGHT", "LSTICK_UP", "LSTICK_DOWN", "LSTICK_LEFT", "LSTICK_RIGHT", "LSTICK_NONE",
        "LSTICK_ALL", "LSTICK_UPDOWN", "LSTICK_LEFTRIGHT", "RSTICK_UP", "RSTICK_DOWN", "RSTICK_LEFT", "RSTICK_RIGHT", "RSTICK_NONE",
        "RSTICK_ALL", "RSTICK_UPDOWN", "RSTICK_LEFTRIGHT", "A_BUTT", "B_BUTT", "X_BUTT", "Y_BUTT", "LB_BUTT", "LT_BUTT", "RB_BUTT",
        "RT_BUTT", "START_BUTT", "BACK_BUTT", "A_BUTT", "B_BUTT"
    };

    static inline std::vector<void*> controllerTexPtrs;
    static inline std::vector<std::vector<void*>> buttonTexPtrs;
    static inline void** gameButtonPtrs = nullptr;
    static inline void** gameControllerPtrs = nullptr;
    static void ButtonsCallback()
    {
        auto prefvalue = FusionFixSettings("PREF_BUTTONS");
        auto prefvalueindex = prefvalue - FusionFixSettings.ButtonsText.eXbox360;
        auto bNeedsReset = false;
        while (std::any_of(buttonTexPtrs[prefvalueindex].begin(), buttonTexPtrs[prefvalueindex].end(), [](auto i) { return i == nullptr; }))
        {
            prefvalue++;

            if (prefvalue > FusionFixSettings.ButtonsText.eSteamController)
                prefvalue = FusionFixSettings.ButtonsText.eXbox360;

            prefvalueindex = prefvalue - FusionFixSettings.ButtonsText.eXbox360;
            bNeedsReset = true;
        }

        if (bNeedsReset)
            FusionFixSettings.Set("PREF_BUTTONS", prefvalue);

        if (gameButtonPtrs)
        {
            for (auto b = buttons.begin(); b < buttons.end(); b++)
            {
                auto i = std::distance(std::begin(buttons), b);
                auto ptr = buttonTexPtrs[prefvalueindex][i];
                if (ptr)
                    gameButtonPtrs[i] = ptr;
            }
        }

        if (gameControllerPtrs && controllerTexPtrs[prefvalueindex])
            gameControllerPtrs[0] = controllerTexPtrs[prefvalueindex];
    }

    static inline injector::hook_back<void(__fastcall*)(void*, void*, const char*)> CTxdStore__LoadTexture;
    static void __fastcall LoadCustomButtons(void* dst, void* edx, const char* name)
    {
        CTxdStore__LoadTexture.fun(dst, edx, name);

        buttonTexPtrs.clear();
        buttonTexPtrs.resize(btnPrefix.size());
        controllerTexPtrs.clear();
        controllerTexPtrs.resize(btnPrefix.size());

        for (auto& v : buttonTexPtrs)
            v.resize(buttons.size());

        for (auto prefix = btnPrefix.begin(); prefix < btnPrefix.end(); prefix++)
        {
            for (auto b = buttons.begin(); b < buttons.end(); b++)
            {
                auto texName = *prefix + *b;
                CTxdStore__LoadTexture.fun(&buttonTexPtrs[std::distance(std::begin(btnPrefix), prefix)][std::distance(std::begin(buttons), b)], edx, texName.c_str());
            }

            auto texName = *prefix + "CONTROLLER";
            CTxdStore__LoadTexture.fun(&controllerTexPtrs[std::distance(std::begin(btnPrefix), prefix)], edx, texName.c_str());
        }

        ButtonsCallback();
    }

    static void __fastcall ControllerTextureCallback(void* dst, void* edx, const char* name)
    {
        CTxdStore__LoadTexture.fun(dst, edx, name);
        if (!controllerTexPtrs[0])
            controllerTexPtrs[0] = *(void**)dst;
        ButtonsCallback();
    }

    static inline injector::hook_back<decltype(&Natives::GetTexture)> hbNATIVE_GET_TEXTURE;
    static Texture __cdecl NATIVE_GET_TEXTURE(TextureDict dictionary, const char* textureName)
    {
        static std::vector<std::string> scriptButtons = {
            "A_BUTT", "B_BUTT", "X_BUTT", "LT_BUTT", "RT_BUTT"
        };

        auto texName = std::string(textureName);
        if (std::any_of(std::begin(scriptButtons), std::end(scriptButtons), [&](auto& i) { return iequals(texName, i); }))
        {
            auto prefvalue = FusionFixSettings("PREF_BUTTONS");
            auto prefvalueindex = prefvalue - FusionFixSettings.ButtonsText.eXbox360;
            texName = btnPrefix[prefvalueindex] + texName;
            auto result = hbNATIVE_GET_TEXTURE.fun(dictionary, texName.c_str());
            if (result)
                return result;
        }
        return hbNATIVE_GET_TEXTURE.fun(dictionary, textureName);
    }

    static inline injector::hook_back<decltype(&Natives::GetControlValue)> hbNATIVE_GET_CONTROL_VALUE;
    static int32_t __cdecl NATIVE_GET_CONTROL_VALUE(int padIndex, int32_t controlID)
    {
        if (padIndex == 99 && controlID == 99)
        {
            static auto prefvalue = FusionFixSettings.GetRef("PREF_BUTTONS");
            return prefvalue->get() - FusionFixSettings.ButtonsText.eXbox360;
        }
        else if (padIndex == 99 && controlID == 98)
        {
            return 1;
        }
        return hbNATIVE_GET_CONTROL_VALUE.fun(padIndex, controlID);
    }

public:
    Buttons()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = hook::pattern("83 C4 14 B9 ? ? ? ? 68 ? ? ? ? E8");
            if (!pattern.empty())
                gameButtonPtrs = *pattern.get_first<void**>(4);
            else
            {
                pattern = hook::pattern("83 C4 14 68 ? ? ? ? B9");
                gameButtonPtrs = *pattern.get_first<void**>(9);
            }

            pattern = hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? E8");
            gameControllerPtrs = *pattern.get_first<void**>(1);
            CTxdStore__LoadTexture.fun = injector::MakeCALL(pattern.get_first(5), ControllerTextureCallback).get();

            pattern = find_pattern("E8 ? ? ? ? 6A FF E8 ? ? ? ? C7 05", "E8 ? ? ? ? 55 E8 ? ? ? ? C7 05");
            CTxdStore__LoadTexture.fun = injector::MakeCALL(pattern.get_first(), LoadCustomButtons).get();

            FusionFixSettings.SetCallback("PREF_BUTTONS", [](int32_t value)
            {
                ButtonsCallback();
            });
        };

        FusionFix::onInitEvent() += []()
        {
            // Script
            {
                hbNATIVE_GET_CONTROL_VALUE.fun = NativeOverride::Register(Natives::NativeHashes::GET_CONTROL_VALUE, NATIVE_GET_CONTROL_VALUE, "E8 ? ? ? ? 8B 0E 83", 30);
                hbNATIVE_GET_TEXTURE.fun = NativeOverride::Register(Natives::NativeHashes::GET_TEXTURE, NATIVE_GET_TEXTURE, "E8 ? ? ? ? 8B 0E 83", 30);
            }
        };
    }
} Buttons;