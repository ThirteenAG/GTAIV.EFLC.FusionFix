module;

#include <common.hxx>
#include "menu_music/Live.hpp"

export module menumusic;

import common;
import comvars;

// Restore the console stem mixer using the PC menu sounds and volume curve.
// Native stream callbacks own sound references until the audio engine releases them.
class MenuMusicRestoration
{
public:
    MenuMusicRestoration()
    {
        FusionFix::onInitEvent() += []()
        {
            MenuMusic::InstallLiveHooks([]() { return _dwCurrentEpisode ? static_cast<int>(*_dwCurrentEpisode) : -1; });
        };
    }
} MenuMusicRestoration;
