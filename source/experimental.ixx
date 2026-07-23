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

            // Skybox Black Bottom Fix -- causes holes in the map to be more visible.
            /*{
                auto PatchVertices = [](float* ptr)
                {
                    for (auto i = 0; i < ((6156 / 4) / 3); i++)
                    {
                        auto pVertex = (ptr + i * 3);

                        if (i < 32)
                        {
                            if (pVertex[1] < 0.0f)
                            {
                                injector::WriteMemory<float>(pVertex, 0.0f, true);
                                injector::WriteMemory<float>(pVertex + 2, 0.0f, true);
                            }
                        }
                        else
                        {
                            injector::WriteMemory<float>(pVertex + 1, (pVertex[1] - 0.16037700f) * 1.78f - 1.0f, true);
                        }
                    }
                };
            
                auto pattern = hook::pattern("C7 44 24 ? ? ? ? ? C7 44 24 ? ? ? ? ? E8 ? ? ? ? 8B 44 24 44");
                if (!pattern.empty())
                {
                    auto addr = pattern.get_first<float*>(4);

                    PatchVertices(*addr);
                }
                else
                {
                    pattern = hook::pattern("C7 46 ? ? ? ? ? C7 46 ? ? ? ? ? 5E 59 C3");
                    if (!pattern.empty())
                    {
                        auto addr = pattern.get_first<float*>(3);

                        PatchVertices(*addr);
                    }
                }
            }*/
        };
    }
} Experimental;