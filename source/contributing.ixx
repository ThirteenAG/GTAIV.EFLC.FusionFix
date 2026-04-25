module;

#include <common.hxx>

export module contributing;

import common;
import comvars;

namespace CGameLogic {
    GameRef<uint32_t> GameState;
}

bool AreWeDeadOrGettingArrested()
{
    if (!_dwCurrentEpisode || *_dwCurrentEpisode != 0)
        return false;
    return CGameLogic::GameState == 1;
}

class Contributing
{
public:
    Contributing()
    {
        FusionFix::onInitEvent() += []()
        {
                // restores check for is dead / getting arrested, restoring this makes the death music play (clippy95)
                static auto pattern = find_pattern("83 3D ? ? ? ? 00 75 ? E8 ? ? ? ? 39 05");

                if (!pattern.empty()) {

                    CGameLogic::GameState.SetAddress(*pattern.get_first<uint32_t*>(2));

                    pattern = find_pattern("E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? C7 86 ? ? ? ? 00 00 00 00 C7 86 ? ? ? ? 00 00 00 00");
                    auto call1 = pattern.get_first();
                    pattern = find_pattern("E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? C7 86 ? ? ? ? ? ? ? ? E9");
                    auto call2 = pattern.get_first();
                    pattern = find_pattern("E8 ? ? ? ? 84 C0 74 ? 8B BE ? ? ? ? 85 FF");
                    auto call3 = pattern.get_first();

                    if (call1 && call2 && call3) {
                        injector::MakeCALL(call1, AreWeDeadOrGettingArrested);
                        injector::MakeCALL(call2, AreWeDeadOrGettingArrested);
                        injector::MakeCALL(call3, AreWeDeadOrGettingArrested);
                    }

                    //pattern = find_pattern("6A ? 68 ? ? ? ? 8D 44 24 ? 6A ? 50 E8 ? ? ? ? 83 C4");
                    //if (!pattern.empty()) {
                    //    injector::WriteMemory<uint8_t>(pattern.get_first(1), 2, true);
                    //}

                }

            // Add your code here
        };
    }
} Contributing;