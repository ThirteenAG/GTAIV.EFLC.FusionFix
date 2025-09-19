module;

#include <common.hxx>

export module episodiccontent;

import common;
import settings;

namespace CEpisodes
{
    injector::hook_back<char(__fastcall*)(void*, void*, int)> hbisAvailable;
    char __fastcall isAvailable(void* _this, void* edx, int a2)
    {
        if (CText::hasViceCityStrings())
            return 0;
        return hbisAvailable.fun(_this, edx, a2);
    }
}

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
            bool bTBoGTPoliceWeapons = iniReader.ReadInteger("EPISODICCONTENT", "TBoGTPoliceWeapons", 0) != 0;
            bool bRemoveSCOSignatureCheck = iniReader.ReadInteger("EPISODICCONTENT", "RemoveSCOSignatureCheck", 0) != 0;
            bool bOtherEpisodicChecks = iniReader.ReadInteger("EPISODICCONTENT", "OtherEpisodicChecks", 0) != 0;

            if (bEpisodicVehicles)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 0F BF 46 2E 3B 05 ? ? ? ? 74 0C 3B 05 ? ? ? ? 0F 85 ? ? ? ? F7 46 ? ? ? ? ? 0F 85 ? ? ? ? F6 86 ? ? ? ? ? 0F 84"); // apc cannon shooting
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 0F BF 46 2E 3B 05 ? ? ? ? 74 0C 3B 05 ? ? ? ? 0F 85 ? ? ? ? 85 7E 28");
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 27"); // apc drive by lock
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 26 8B 85");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 37"); // apc lights
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 35");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 22"); // apc cannon sounds
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 57 7C 22");
                    injector::MakeNOP(pattern.get_first(8), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? F3 0F 10 0D ? ? ? ? F3 0F 11 4C 24 ? 75 14"); // apc cannon on impact sounds
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(21), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 11 44 24 ? 75 4C");
                    injector::MakeNOP(pattern.get_first(21), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 08 3B 05"); // APC respray
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 59 83 F8 02"); // CExplosions__addExplosion
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 57 83 F8 02");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 1B 8B 46 40"); // CExplosions__addExplosion disable ped rolling on any explosive from aa12/apc shots
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 1B 8B 56 40");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 21 8B 87"); // APC rotation sound
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 21 8B 96");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("8B 1D ? ? ? ? 83 C4 04 83 FB 02"); // buzzard rocket particles
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(12), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 43");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 1E"); // buzzard minigun particles
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 1F");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 10 FF 76 18 E8 ? ? ? ? 83 C4 04 83 38 27 74 04"); // buzzard minigun particles
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 11 8B 4E 18");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 10 FF 76 18 E8 ? ? ? ? 83 C4 04 83 38 27 74 07"); // BUZZARD rubble effects
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 11 8B 4F 18");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 89 44 24 38"); // buzzard rockets sound and minigun sounds
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(11), 6, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 64"); // EpisodicVehicleSupport (BUZZARD & SWIFT) smoke effects
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 10 FF 76 18 E8 ? ? ? ? 83 C4 04 83 38 27 74 0A"); // BUZZARD BULLET_IMPACT_WATER
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 11 8B 46 18");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 48 3B 05"); // smuggler, floater, blade support
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 5A 3B 05");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 54 3B 05 ? ? ? ? 75 19 8B CF"); // smuggler, floater, blade support
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 2C 3B 05");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 3B 3B 05 ? ? ? ? 75 19"); // smuggler, floater, blade support
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 28 3B 05 ? ? ? ? 74 C3");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 3B 3B 05 ? ? ? ? 74 08"); // smuggler, floater, blade support
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 30 3B 05");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 54 3B 05 ? ? ? ? 75 19 8B CE"); // smuggler, floater, blade support
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 28 3B 05 ? ? ? ? 74 82");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 2E 3B 05"); // smuggler
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 32 3B 05");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 3A 3B 05 ? ? ? ? 75 0C C7 86 ? ? ? ? ? ? ? ? EB 26 3B 05 ? ? ? ? 75 0C C7 86 ? ? ? ? ? ? ? ? EB 12 3B 05 ? ? ? ? 75 0A C7 86 ? ? ? ? ? ? ? ? 8B CE E8 ? ? ? ? 83 3D ? ? ? ? ? 8B 45 1C"); // smuggler
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 3C 3B 05 ? ? ? ? 75 0A F3 0F 10 05 ? ? ? ? EB 22 3B 05 ? ? ? ? 75 0A F3 0F 10 05 ? ? ? ? EB 10 3B 05 ? ? ? ? 75 10 F3 0F 10 05 ? ? ? ? F3 0F 11 86 ? ? ? ? 8B CE E8 ? ? ? ? 83 3D ? ? ? ? ? D9 44 24 10");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 3A 3B 05 ? ? ? ? 75 0C C7 86 ? ? ? ? ? ? ? ? EB 26 3B 05 ? ? ? ? 75 0C C7 86 ? ? ? ? ? ? ? ? EB 12 3B 05 ? ? ? ? 75 0A C7 86 ? ? ? ? ? ? ? ? 8B CE E8 ? ? ? ? 83 3D ? ? ? ? ? 8B 45 10"); // smuggler
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 3C 3B 05 ? ? ? ? 75 0A F3 0F 10 05 ? ? ? ? EB 22 3B 05 ? ? ? ? 75 0A F3 0F 10 05 ? ? ? ? EB 10 3B 05 ? ? ? ? 75 10 F3 0F 10 05 ? ? ? ? F3 0F 11 86 ? ? ? ? 8B CE E8 ? ? ? ? 83 3D ? ? ? ? ? D9 EE");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 20 0F BF 46 2E"); // floater camera height
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 20 0F BF 56 2E");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }
            }
            
            if (bEpisodicWeapons)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? FF 76 18");  // AA12 explosive shells
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 8B 46 18");
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 24");  // AA12 self damage
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 21 8B 44 24 0C");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 21 83 F8 23"); // DSR1 scope
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 32");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 5F 5E 5D");    // DSR1 scope hud
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(11), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 5B 83 3D");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("85 C0 74 0D 83 7E 18 1C");    //pipe bomb drop hud
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(2), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 74 1A 83 7D 18 1C");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 F8 02 75 24 8B CE E8 ? ? ? ? 84 C0 74 14"); // sticky bomb drop hud
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(3), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 20 8B CD");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 75"); // sticky bomb activation
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 7D");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 59 8B 07"); // sticky bomb planting animation freeze
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 50 8B 06");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 8B 47 4C"); // sticky bomb planting camera freeze
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 8B 46 4C");
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 3C 8B 41 20"); // Sticky bomb faster throw in vehicle ??
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 8D 8E"); // Sticky bomb faster throw in vehicle
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 7C 69");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("0F 8C ? ? ? ? 8B 44 24 14 83 78 18 24"); // Sticky bomb faster throw in vehicle
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                else {
                    pattern = hook::pattern("0F 8C ? ? ? ? 83 7B 18 24");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 26 8B 4C 24 04"); // Sticky bomb something?
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 29 8B 44 24 04");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 89 44 24 1C"); // Sticky bomb ???
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(15), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 88 44 24 1C");
                    injector::MakeNOP(pattern.get_first(11), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 1F 8B 44 24 0C"); // CTaskSimplePlayerAimProjectile::process
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 1C 8B 47 18");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 07 68 ? ? ? ? EB 05 68 ? ? ? ? E8"); // E2 Bullet traces
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 2D 83 7E 14 19"); // Grenade launcher explode on impact from E2
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 30 83 7E 14 19");
                    injector::MakeNOP(pattern.get_first(27), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 8B 3D ? ? ? ? F3 0F 10 05"); // Weapon sounds slow motion? 
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(31), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? F3 0F 10 15");
                    injector::MakeNOP(pattern.get_first(27), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 8B 3D ? ? ? ? F3 0F 10 05"); // SHOTGUN_EXPLOSION
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 7C");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 8B 3D ? ? ? ? F3 0F 11 4C 24"); // GRENADE_EXPLOSION 
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 84 C0");
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 22 83 F8 02"); // weap checks? nearby explosive weapon checks
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 83 7F 54 20"); // P90 scroll block
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 83 7F 6C 20");
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 7C 05"); // P90 get in car block
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("39 35 ? ? ? ? 7C 05");
                    injector::MakeNOP(pattern.get_first(6), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 14 E8 ? ? ? ? 85 C0 74 0B"); // parachute anims
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 14 E8 ? ? ? ? 3B C3");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 56 8B 74 24 0C"); // parachute extended camera
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 8B 54 24 0C");
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 57 C9"); // parachute wind sounds
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(10), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 0A F3 0F 10 05 ? ? ? ? EB 03");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }
                    
                pattern = hook::pattern("83 3D ? ? ? ? ? 75 0C 80 3D ? ? ? ? ? 74 03"); // Check if player had a parachute
                    if (!pattern.empty())
                        injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 20 E8 ? ? ? ? 85 C0"); // Check for parachute during savegame
                    if (!pattern.empty())
                        injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 18 0F B6 05"); // Check for parachute during savegame
                    if (!pattern.empty())
                        injector::MakeNOP(pattern.get_first(7), 2, true);
                    else {
                        pattern = hook::pattern("83 3D ? ? ? ? ? 75 0D 80 7E 7D 00");
                        injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 5D 75 1E"); // Give parachute during load save
                    if (!pattern.empty())
                        injector::MakeNOP(pattern.get_first(8), 2, true);
                    else {
                        pattern = hook::pattern("83 3D ? ? ? ? ? 75 1E E8");
                        injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 53 57 8B 7C 24 20"); // TBoGT counter anims fix
                    if (!pattern.empty())
                        injector::MakeNOP(pattern.get_first(15), 2, true);
                    else {
                        pattern = hook::pattern("83 3D ? ? ? ? ? 53 55 8B 6C 24 20");
                        injector::MakeNOP(pattern.get_first(21), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 36 80 7B 28 00"); // TBoGT counter anims fix
                    if (!pattern.empty())
                        injector::MakeNOP(pattern.get_first(7), 2, true);
                    else {
                        pattern = hook::pattern("39 1D ? ? ? ? 75 2A 80 7F 28 00");
                        injector::MakeNOP(pattern.get_first(7), 2, true);
                }
            }
            
            if (bExplosiveAnnihilator)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 7C 7F");  // annihilator explosive shots
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 85 FF");
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                }
            }

            if (bOtherEpisodicChecks)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 8A 81");  // disco camera shake
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(18), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 05 E8 ? ? ? ? 8B 4E 04");  // disco camera shake
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 23 80 3D");  // disco camera shake
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 8B CE");  // phone switch
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? 8B CF");
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 75 0E 80 3D ? ? ? ? ? 74 05 E8"); // altimeter for parachute and helis
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("39 35 ? ? ? ? 75 0E 80 3D");
                    injector::MakeNOP(pattern.get_first(6), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? DD D8");  // explosive sniper cheat
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(9), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 39 9C 24");
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 80 3D ? ? ? ? ? 0F 84 ? ? ? ? F3 0F 10 05");  // explosive fists cheat
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 80 3D ? ? ? ? ? 0F 84 ? ? ? ? 6A FF");
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                }

                pattern = hook::pattern("83 F9 02 75 2E");  // E2_landing marker
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(3), 2, true);
                else {
                    pattern = hook::pattern("88 8C 24 ? ? ? ? 75 2F");
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                }

                pattern = hook::pattern("B8 ? ? ? ? 0F 44 E8 57");  // E2_landing marker
                if (!pattern.empty()) {
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xBD, true);
                    injector::MakeNOP(pattern.get_first(5), 3, true);
                }
                else {
                    pattern = hook::pattern("BD ? ? ? ? 75 05");
                    injector::MakeNOP(pattern.get_first(5), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? BB ? ? ? ? B8");  // E2_landing marker
                if (!pattern.empty()) {
                    injector::WriteMemory<uint8_t>(pattern.get_first(12), 0xBB, true);
                    injector::MakeNOP(pattern.get_first(17), 3, true);
                }
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? BB ? ? ? ? 75 05");
                    injector::MakeNOP(pattern.get_first(12), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 66 83 3E 09");  // E2_landing marker
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else {
                    pattern = hook::pattern("39 3D ? ? ? ? 0F 85 ? ? ? ? 66 83 7E");
                    injector::MakeNOP(pattern.get_first(6), 6, true);
                }

                pattern = find_pattern("7E ? 8D 7C 24 ? 57", "7E ? 8D 7C 24 ? 8B FF");
                static auto loc_9BB987 = resolve_displacement(pattern.get_first(0)).value();

                pattern = find_pattern("8B 48 ? 85 C9 74 ? 8B 81 ? ? ? ? EB ? 8B 40 ? 85 C0 74 ? 8B 00 89 04 B5", "8B 48 ? 83 C4 ? 85 C9 74 ? 8B 81");
                static auto NoAssetsCrashWorkaround = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    if (!regs.eax)
                        return_to(loc_9BB987);
                });
            }

            if (bTBoGTHelicopterHeightLimit)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 7C 0A F3 0F 10 05");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
            }

            if (bTBoGTPoliceWeapons)
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 75 3D E8"); // P90s and assault shotguns for cops
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 2, true);
                else {
                    pattern = hook::pattern("39 2D ? ? ? ? 75 3E");
                    injector::MakeNOP(pattern.get_first(6), 2, true);
                }

                pattern = hook::pattern("83 3D ? ? ? ? ? 6A 00 6A 00 6A 01 8D 8E ? ? ? ? 68 ? ? ? ? 75 04"); // m249 for swat in annihilators and helicopters
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(24), 2, true);
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 6A 00 6A 00 6A 01 8B CF 68 ? ? ? ? 75 04");
                    injector::MakeNOP(pattern.get_first(20), 2, true);
                }
            }

            if (bRemoveSCOSignatureCheck)
            {
                auto pattern = hook::pattern("85 DB 74 1E A1 ? ? ? ? 85 C0 74 15 3B D8 74 11");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(2), 0xEB, true);
            }

            {
                auto pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 ? 8B 0D ? ? ? ? 6A ? E8 ? ? ? ? 84 C0 75 ? A2");
                if (!pattern.empty())
                    CEpisodes::hbisAvailable.fun = injector::MakeCALL(pattern.get_first(), CEpisodes::isAvailable).get();

                pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 ? A2 ? ? ? ? C6 05 ? ? ? ? ? A2 ? ? ? ? E8 ? ? ? ? C7 05");
                if (!pattern.empty())
                    CEpisodes::hbisAvailable.fun = injector::MakeCALL(pattern.get_first(), CEpisodes::isAvailable).get();

                pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 ? 46 83 FE ? 7C ? 83 C8");
                if (!pattern.empty())
                    CEpisodes::hbisAvailable.fun = injector::MakeCALL(pattern.get_first(), CEpisodes::isAvailable).get();
            }
        };
    }
} EpisodicContent;