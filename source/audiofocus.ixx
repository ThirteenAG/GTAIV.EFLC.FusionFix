module;

#include <common.hxx>

export module audiofocus;

import common;
import comvars;

// The game pauses its audio timers while the window is minimized or, with "block on lost focus" off, in the background.
// The timers of the cutscene and script sounds are paused and resumed only by its focus handler, which runs on
// WM_ACTIVATEAPP and WM_CLOSE. A fullscreen game minimizes itself when it loses focus and gets WM_ACTIVATEAPP back
// before it is restored, so the handler finds it still minimized and keeps them paused, and nothing resumes them once
// the window is restored. The next cutscene's audio never gets prepared and the mission waits for it on a black screen
// (the infinite loading after alt-tabbing; closing the window resumes them and the cutscene starts as the game quits).
// Here the handler runs again whenever the window's state no longer matches what it last applied.
namespace AudioFocus
{
    void (**pOnFocusChange)() = nullptr;
    void (*OnFocusChange)() = nullptr;
    bool bPaused = false;

    bool ShouldPause()
    {
        return (gWnd && IsIconic(gWnd)) || (*rage::grcDevice::ms_bNoBlockOnLostFocus && *rage::grcWindow::ms_bFocusLost);
    }

    void OnFocusChangeHook()
    {
        OnFocusChange();
        bPaused = ShouldPause();
    }

    void Resync()
    {
        auto handler = *pOnFocusChange;
        if (!handler)
            return;

        // The handler is set before the first frame; what it applied until then isn't known, so it runs once
        if (handler != &OnFocusChangeHook)
        {
            OnFocusChange = handler;
            *pOnFocusChange = &OnFocusChangeHook;
            OnFocusChangeHook();
            return;
        }

        if (bPaused != ShouldPause())
            OnFocusChangeHook();
    }
}

// Between cutscene sections the next section's audio is prepared in a blocking loop that updates the audio but doesn't
// pump the window's messages. The game resumes one audio timer for it, but not the cutscene sounds' one, which stays as
// the focus handler left it, so the loop never ends when the window was minimized or the handler's state was stale.
// It is resumed for the wait too.
namespace audCutsceneAudioEntity
{
    uint8_t* pCutsceneTimerPaused = nullptr;
    SafetyHookInline shPreload = {};

    int __stdcall Preload(const char* name, int startOffset)
    {
        AudioFocus::Resync();
        auto paused = std::exchange(*pCutsceneTimerPaused, uint8_t(0));
        auto result = shPreload.unsafe_stdcall<int>(name, startOffset);
        *pCutsceneTimerPaused = paused;
        return result;
    }
}

class AudioFocusFix
{
public:
    AudioFocusFix()
    {
        FusionFix::onInitEvent() += []()
        {
            // CApp::Init sets the handler, the window procedure calls it on WM_CLOSE: mov eax, [handler]; ...; call eax
            auto pattern = find_pattern("A1 ? ? ? ? C6 05 ? ? ? ? 00 85 C0 74 02 FF D0 C6 05", "A1 ? ? ? ? 85 C0 C6 05 ? ? ? ? 00 74 02 FF D0 C6 05");
            AudioFocus::pOnFocusChange = *pattern.get_first<void(**)()>(1);

            // audCutsceneAudioEntity::Preload starts by reading the pause flag of the timer it resumes; the audio timers
            // are 12 bytes apart in audSoundManager, the cutscene sounds' one right before it
            pattern = find_pattern("51 8A 0D ? ? ? ? 53 8B 5C 24 0C 57 8B 7C 24 14", "53 8A 1D ? ? ? ? 84 DB 55 57 74 07 C6 05");
            audCutsceneAudioEntity::pCutsceneTimerPaused = *pattern.get_first<uint8_t*>(3) - 12;
            audCutsceneAudioEntity::shPreload = safetyhook::create_inline(pattern.get_first(0), audCutsceneAudioEntity::Preload);

            FusionFix::onGameProcessEvent() += []()
            {
                AudioFocus::Resync();
            };
        };
    }
} AudioFocusFix;
