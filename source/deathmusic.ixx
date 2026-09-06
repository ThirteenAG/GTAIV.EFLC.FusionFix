module;

#include <common.hxx>

export module deathmusic;

import common;
import comvars;
import settings;

bool bEpisodicDeathMusic = false;

bool IsDeathArrest()
{
    if (CText::hasViceCityStrings() || !bEpisodicDeathMusic && (!_dwCurrentEpisode || *_dwCurrentEpisode != 0))
        return false;

    return *CGameLogic::GameState == 1 || *CGameLogic::GameState == 2;
}

class DeathMusic
{
public:
    static void DeathMusic_sprintf_s(char* Buffer, size_t BufferCount, const char* Format)
    {
        int g_DeathMusicIndex = CRandom::GetRandomNumberInRange(1, 7); // Since a2 is exclusive when using this function, a2 needs to be 7 to cover all 6 audio files

        sprintf_s(Buffer, BufferCount, Format, g_DeathMusicIndex);
    }

    DeathMusic()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [MISC]
            auto bDeathMusic = iniReader.ReadInteger("MISC", "DeathMusic", 0) != 0;

            // [EPISODICCONTENT]
            bEpisodicDeathMusic = iniReader.ReadInteger("EPISODICCONTENT", "EpisodicDeathMusic", 0) != 0;

            // By Clippy95
            if (bDeathMusic)
            {
                // Replace stub functions a long time ago responsible for making the death music play with a new function that checks if the player is dead or arrested, restoring the death music
                auto pattern = find_pattern("E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? C7 86 ? ? ? ? ? ? ? ? C7 86", "E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? 0F 57 C0 F3 0F 11 86");
                injector::MakeCALL(pattern.get_first(0), IsDeathArrest);

                pattern = find_pattern("E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? C7 86 ? ? ? ? ? ? ? ? E9");
                injector::MakeCALL(pattern.get_first(0), IsDeathArrest);

                pattern = find_pattern("E8 ? ? ? ? 84 C0 74 ? 8B BE ? ? ? ? 85 FF", "E8 ? ? ? ? 84 C0 74 ? 8B BE ? ? ? ? 3B FB");
                injector::MakeCALL(pattern.get_first(0), IsDeathArrest);

                // Edit the death music sprintf_s call to randomize the playing of death music audio files instead of always playing DEATH_MUSIC_06
                pattern = find_pattern("E8 ? ? ? ? 83 C4 ? 8D 4C 24 ? E8 ? ? ? ? FF 35 ? ? ? ? 8D 44 24", "E8 ? ? ? ? 83 C4 ? 8D 8C 24 ? ? ? ? E8 ? ? ? ? 8B 15 ? ? ? ? 52");
                injector::MakeCALL(pattern.get_first(0), DeathMusic_sprintf_s);
            }
        };
    }
} DeathMusic;