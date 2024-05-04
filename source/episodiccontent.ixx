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
            bool bExplosiveAnnihilator = iniReader.ReadInteger("EPISODICCONTENT", "ExplosiveAnnihilator", 0) != 0;
            bool bTBoGTHelicopterHeightLimit = iniReader.ReadInteger("EPISODICCONTENT", "TBoGTHelicopterHeightLimit", 0) != 0;
            bool bTBoGTWeaponRandomization = iniReader.ReadInteger("EPISODICCONTENT", "TBoGTWeaponRandomization", 0) != 0;
            bool bRemoveSCOSignatureCheck = iniReader.ReadInteger("EPISODICCONTENT", "RemoveSCOSignatureCheck", 0) != 0;
            bool bEpisodicCheats = iniReader.ReadInteger("EPISODICCONTENT", "EpisodicCheats", 0) != 0;
            bool bOtherEpisodicChecks = iniReader.ReadInteger("EPISODICCONTENT", "OtherEpisodicChecks", 0) != 0;

            if (bEpisodicVehicles)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 0F BF 46 2E"); // apc cannon
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 27"); // apc drive by lock
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 37"); // apc lights
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? F3 0F 10 0D ? ? ? ? F3 0F 11 4C 24 ? 75 14"); // apc cannon sounds
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(21), 2, true);

                pattern = hook::pattern("8B 1D ? ? ? ? 83 C4 04 83 FB 02"); // buzzard rocket particles
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(12), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 1E"); // buzzard minigun particles
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 10 FF 76 18 E8 ? ? ? ? 83 C4 04 83 38 27 74 04"); // buzzard minigun particles
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 89 44 24 38"); // buzzard rockets sound and minigun sounds
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(11), 6, true);
            }
            
            if (bEpisodicWeapons)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? FF 76 18");    // AA12 explosive shells
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 21 83 F8 23"); // DSR1 scope
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);


                pattern = hook::pattern("85 C0 74 0D 83 7E 18 1C");    //pipe bomb drop hud
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(2), 2, true);

                pattern = hook::pattern("83 F8 02 75 24 8B CE E8 ? ? ? ? 84 C0 74 14"); // sticky bomb drop hud
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(3), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 75"); // sticky bomb activation
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 83 7F 54 20"); // P90 scroll block
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 14 E8"); // parachute anims
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
            }
            
            if (bExplosiveAnnihilator)
                {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 7C 7F");  // annihilator explosive shots
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

            if (bOtherEpisodicChecks)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 8A 81");  // disco camera shake
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(18), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 05 E8 ? ? ? ? 8B 4E 04");  // disco camera shake
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 8B CE");  // phone switch
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 0E 80 3D"); // altimeter for parachute and helis
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
            }

            if (bTBoGTHelicopterHeightLimit)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 7C 0A F3 0F 10 05");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
            }

            if (bTBoGTWeaponRandomization)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 75 3D E8"); // P90s and assault shotguns for cops
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 0A 83 7F 0C 1E"); // swat in police helis
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
            }

            if (bRemoveSCOSignatureCheck)
            {
                auto pattern = hook::pattern("85 DB 74 1E A1 ? ? ? ? 85 C0 74 15 3B D8 74 11");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(2), 0xEB, true);
            }

            if (bEpisodicCheats)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 75 23 80 3D");  // parachute
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? DD D8");  // explosive sniper
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(9), 6, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 80 3D ? ? ? ? ? 0F 84 ? ? ? ? F3 0F 10 05");  // explosive fists
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
            }
        };
    }
} EpisodicContent;