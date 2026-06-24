module;

#include <common.hxx>

export module experimental;

import common;

bool bEnableHighDetailReflections = false;
bool bRemoveBBoxCulling = false;

namespace CScene
{
    injector::hook_back<void(__cdecl*)(int, int16_t, int)> hbAddToDrawList;
    void __cdecl AddToDrawList_CRenderPhaseReflection__AddToDrawList(int a1, int16_t a2, int a3)
    {
        if (bEnableHighDetailReflections)
        {
            for (int i = 0; i <= 11; ++i)
            {
                hbAddToDrawList.fun(i, 285, 0);
            }

            return;
        }

        return hbAddToDrawList.fun(a1, a2, a3);
    }
}

class Experimental
{
public:
    Experimental()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [EXPERIMENTAL]
            bEnableHighDetailReflections = iniReader.ReadInteger("EXPERIMENTAL", "EnableHighDetailReflections", 0) != 0;
            bRemoveBBoxCulling = iniReader.ReadInteger("EXPERIMENTAL", "RemoveBBoxCulling", 0) != 0;

            if (bEnableHighDetailReflections)
            {
                auto pattern = find_pattern("FF B6 ? ? ? ? E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 6A ? 6A", "68 ? ? ? ? 50 E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 6A");
                CScene::hbAddToDrawList.fun = injector::MakeCALL(pattern.get_first(6), CScene::AddToDrawList_CRenderPhaseReflection__AddToDrawList).get();

                if (bRemoveBBoxCulling)
                {
                    pattern = hook::pattern("0F 85 ? ? ? ? E8 ? ? ? ? 80 3D ? ? ? ? ? 8B 75");
                    injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true);
                }
            }
        };
    }
} Experimental;