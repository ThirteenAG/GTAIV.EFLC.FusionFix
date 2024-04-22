module;

#include <common.hxx>

export module episodiccontent;

import common;

class EpisodicContent
{
public:
    EpisodicContent()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");

            //[EPISODICCONTENT]
            bool bEpisodicVehicles = iniReader.ReadInteger("EPISODICCONTENT", "EpisodicVehicles", 0) != 0;
            bool bEpisodicWeapons = iniReader.ReadInteger("EPISODICCONTENT", "EpisodicWeapons", 0) != 0;
            bool bTBoGTHelicopterHeightLimit = iniReader.ReadInteger("EPISODICCONTENT", "TBoGTHelicopterHeightLimit", 0) != 0;
            bool bTBoGTWeaponRandomization = iniReader.ReadInteger("EPISODICCONTENT", "TBoGTWeaponRandomization", 0) != 0;
            bool bRemoveSCOSignatureCheck = iniReader.ReadInteger("EPISODICCONTENT", "RemoveSCOSignatureCheck", 0) != 0;

            if (bEpisodicVehicles)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 7C 52 0F BF 46 2E");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? A0");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(16), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 21 8B 87");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
 
                pattern = hook::pattern("83 3D ? ? ? ? ? 75 35 0F BF 46 2E");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 8B 0F");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(25), 2, true);         

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 18 0F BF 47 2E 3B 05 ? ? ? ? 74 08 3B 05 ? ? ? ? 75 04 B0 01");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
 
                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 18 0F BF 47 2E 3B 05 ? ? ? ? 74 08 3B 05 ? ? ? ? 75 04 32 C0");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 3A F3 0F 10 05");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 2D F6 85");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);      

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 0E F3 0F 10 05 ? ? ? ? F3 0F 11 44 24 ? 8B 46 04");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
 
                pattern = hook::pattern("83 3D ? ? ? ? ? 75 1F 8B 44 24 04");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 08 3B 05");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 18 66 0F 6E C6");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 17 F3 0F 10 44 24");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 21 0F 2F 05");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 0F BF 46 2E");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);      

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 27");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 37");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 21 83 F8 23");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
            }
            
            if (bEpisodicWeapons)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? FF 76 18");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? BA ? ? ? ? 50");   //...
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(13), 2, true);

                pattern = hook::pattern("83 F8 02 75 24 8B CE E8 ? ? ? ? 84 C0 74 14");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(3), 2, true);

                pattern = hook::pattern("85 C0 74 0D 83 7E 18 1C");    //pipe bomb
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(2), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 59 83 3D");    //scope hud
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 59 8B 07");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 24");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 75");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 8B 47 4C");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
            }

            if (bTBoGTHelicopterHeightLimit)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 7C 0A F3 0F 10 05");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
            }

            if (bTBoGTWeaponRandomization)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 75 3D E8");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
            }

            if (bRemoveSCOSignatureCheck)
            {
                auto pattern = hook::pattern("85 DB 74 1E A1 ? ? ? ? 85 C0 74 15 3B D8 74 11");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(2), 0xEB, true);
            }
        };
    }
} EpisodicContent;