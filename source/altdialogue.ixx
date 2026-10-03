module;

#include <common.hxx>

export module altdialogue;

import common;
import comvars;
import settings;

// Mission counters that only pick which variant of a conversation plays. Missions bump them after a failed attempt,
// so a retry hears other lines; setting one to 1 when the mission starts plays the retry variant. Checked against
// the mission scripts for every value they can hold: counters that also drive difficulty, timers, spawns, player
// mood, texts or mission state are not listed, and neither are counters shared between missions.
std::vector<std::vector<std::tuple<std::string_view, uint32_t>>> gAltDialogueVars =
{
    {
        { "badman_1", 65020 },
        { "bell2", 64953 },
        { "bell2", 64954 },
        { "bell5", 64968 },
        { "bell7", 64939 },
        { "bernie1", 64936 },
        { "bernie2", 64937 },
        { "bernie2", 64938 },
        { "bernie3", 65005 },
        { "brian_2", 65023 },
        { "brian_3", 65025 },
        { "brucie4", 64727 },
        { "brucie4", 64728 },
        { "brucie4", 64729 },
        { "brucie4", 64730 },
        { "cherise", 64743 },
        { "derrick3", 64978 },
        { "derrick3", 64979 },
        { "eddie1", 64740 },
        { "elizabeta2", 64646 }, // g_U64645[0], 64645 is the array size
        { "elizabeta2", 64647 }, // g_U64645[1]
        { "faustin7", 64731 },
        { "finale1a", 64733 },
        { "finale1a", 64734 },
        { "finale1a", 64735 },
        { "finale2", 64969 },
        { "finale2", 64970 },
        { "finale2", 64971 },
        { "finale2", 64972 },
        { "francis6", 64932 },
        { "gambetti1", 64720 },
        { "gambetti1", 64721 },
        { "gerry3b", 64941 },
        { "hossan_1", 65027 },
        { "ivan_1", 65029 },
        { "jacob1", 64934 },
        { "jacob1", 64935 },
        { "jimmy1", 64572 },
        { "jimmy1", 64573 },
        { "jimmy1", 64574 },
        { "manny1", 64567 },
        { "manny1", 64568 },
        { "marnie1", 64738 },
        { "marnie2", 64739 },
        { "mel", 64737 },
        { "michelle1", 64564 },
        { "michelle1", 64565 },
        { "packie1", 64940 },
        { "packie2", 64718 },
        { "packie3", 64965 },
        { "packie3", 64966 },
        { "pathos2", 64742 },
        { "playboy3", 64962 },
        { "playboy3", 64963 },
        { "ray2", 64955 },
        { "ray2", 64956 },
        { "ray2", 64957 },
        { "roman12", 64964 },
        { "roman13", 64623 },
        { "roman14", 64624 },
        { "roman2", 64975 },
        { "roman2", 64976 },
        { "roman3", 64717 },
        { "roman4", 64960 },
        { "roman4", 64961 },
        { "roman5", 64716 },
        { "roman6", 64763 },
        { "roman6", 64762 },
        { "roman7", 64724 },
        { "roman7", 64725 },
        { "sara_1", 65031 },
    },
    {
        { "ashley1", 38744 },
        { "ashley2", 39237 },
        { "billy1", 39214 },
        { "billy1", 39216 },
        { "billy1", 39215 },
        { "billy1", 39217 },
        { "billy3", 38772 },
        { "billy3", 38770 },
        { "billy3", 38773 },
        { "billy3", 38771 },
        { "billy4", 39218 },
        { "billy6", 39203 },
        { "brian", 39241 },
        { "elizabeta3", 38710 },
        { "jim1", 39246 },
        { "jim1", 39243 },
        { "jim1", 39245 },
        { "jim5", 39182 },
        { "jim5", 39183 },
        { "malc1", 39242 },
        { "ray1", 39239 },
        { "stubbs2", 39233 },
    },
    {
        { "arnaud1", 43696 },
        { "arnaud2", 43697 },
        { "brother1", 43670 },
        { "brother1", 43669 },
        { "brother2", 42993 },
        { "brother3", 43587 },
        { "bulgarin1", 43800 },
        { "bulgarin2", 42939 },
        { "bulgarin2", 42940 },
        { "tony10", 43694 },
        { "tony10", 43695 },
        { "tony11", 43672 },
        { "tony2", 43004 },
        { "tony2", 43007 },
        { "tony2", 43005 },
        { "tony2", 43006 },
        { "tony2", 43008 },
        { "tony2", 43009 },
        { "tony2", 43010 },
        { "tony3", 42928 },
        { "tony3", 42929 },
        { "tony4a", 42937 },
        { "tony4b", 42938 },
        { "tony6", 43646 },
        { "tony6", 43647 },
        { "tony6", 43648 },
        { "tony7", 42994 },
        { "tony7", 42995 },
        { "tonym1", 43684 },
        { "tonym2", 43685 },
        { "tonym3", 43686 },
        { "tonym4", 43687 },
        { "tonym5", 43688 },
        { "tonym6", 43689 },
        { "tonym7", 43690 },
        { "yusuf2", 42910 },
        { "yusuf3", 43668 },
        { "yusuf4", 42912 }, // g_U42911[0], 42911 is the array size
    }
};

void SetAltDialogueVars(const char* name)
{
    static auto altdialogue = FusionFixSettings.GetRef("PREF_ALTDIALOGUE");
    auto episode = *_dwCurrentEpisode;
    if (name && altdialogue && altdialogue->get() && *rage::scrProgram::ms_pGlobals && episode >= 0 && episode < int32_t(gAltDialogueVars.size()))
    {
        auto pGlobals = *rage::scrProgram::ms_pGlobals;
        for (auto& [script, index] : gAltDialogueVars[episode])
        {
            // On every launch, so saves that already moved a counter on (retries, older versions) get it too
            if (iequals(script, name) && index < *rage::scrProgram::ms_pGlobalsSize)
                pGlobals[index] = 1;
        }
    }
}

injector::hook_back<int(__cdecl*)(const char*, void*, int32_t, int32_t)> hbStartNewScript;
int __cdecl StartNewScript(const char* name, void* args, int32_t argsSize, int32_t stackSize)
{
    SetAltDialogueVars(name);
    return hbStartNewScript.fun(name, args, argsSize, stackSize);
}

class AltDialogue
{
public:
    AltDialogue()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // START_NEW_SCRIPT and START_NEW_SCRIPT_WITH_ARGS start scripts through a launcher pointer, so the counters are set
            // before a mission runs its first instruction and are never touched while it runs. CGame::Init stores the
            // game's launcher in that pointer, so take over that store.
            auto pattern = hook::pattern("68 ? ? ? ? 68 B9 60 22 4E"); // push handler, push START_NEW_SCRIPT hash
            if (pattern.empty())
                return;

            auto handler = *pattern.get_first<uintptr_t>(1);
            auto call = hook::range_pattern(handler, handler + 0x40, "FF 15 ? ? ? ? 8B F8 83 C4 10");
            if (call.empty())
                return;

            auto launcher = *call.get_first<decltype(hbStartNewScript.fun)*>(2);
            pattern = hook::pattern("C7 05 " + pattern_str(to_bytes(launcher))); // mov launcher, offset GtaLauncher
            if (pattern.empty())
                return;

            auto store = pattern.get_first<decltype(hbStartNewScript.fun)>(6);
            hbStartNewScript.fun = *store;
            injector::WriteMemory(store, &StartNewScript, true);

            // In case CGame::Init has already run
            if (*launcher == hbStartNewScript.fun)
                *launcher = &StartNewScript;
        };
    }
} AltDialogue;
