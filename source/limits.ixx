module;

#include <common.hxx>
#include <array>

export module limits;

import common;
import settings;

class LimitAdjuster
{
private:
    size_t    m_increaseby = 2;
    bool      m_patchedXrefs = false;
    bool      m_patchedNumRefs = false;
    uintptr_t m_startAddress = 0;
    size_t    m_elementSize = 0;
    size_t    m_elementsCount = 0;
    size_t    m_xrefCount = 0;
    uint8_t*  m_array = nullptr;
public:
    LimitAdjuster(auto startAddress, auto elementSize, auto elementsCount, auto xrefCount) :
        m_startAddress(startAddress), m_elementSize(elementSize), m_elementsCount(elementsCount), m_xrefCount(xrefCount){}

    LimitAdjuster& IncreaseBy(auto num)
    {
        if (num > 1)
            m_increaseby = num;
        return *this;
    }

    LimitAdjuster& InsertNewArrayPointer(uint8_t* ptr)
    {
        m_array = ptr;
        return *this;
    }

    LimitAdjuster& ReplaceXrefs(std::convertible_to<ptrdiff_t> auto&& ...offsets)
    {
        if (!m_patchedXrefs)
        {
            auto list = std::initializer_list<ptrdiff_t>{ offsets... };
            ptrdiff_t max_array_offset = m_elementSize * m_elementsCount;
            auto max_needed_offset = *std::max_element(list.begin(), list.end());
            auto i = 0;

            if (m_array == nullptr) {
                auto vec = new std::vector<uint8_t>(m_increaseby * max_array_offset + ((max_needed_offset > max_array_offset) ? max_needed_offset - max_array_offset : 0), 0);
                m_array = vec->data();
            }

            for (auto v : list)
            {
                auto ptr = m_startAddress + v;
                auto pattern = hook::pattern(pattern_str(to_bytes(ptr)));
                pattern.for_each_result([&](hook::pattern_match match)
                {
                    auto xref = match.get<uint32_t>();
                    if (*xref == ptr)
                    {
                        if (v < max_array_offset)
                            injector::AdjustPointer(xref, m_array, m_startAddress, ptr);
                        else
                            injector::WriteMemory(xref, m_array + ((m_elementSize * m_elementsCount * m_increaseby) + (v - max_array_offset)), true);
                    }
                    i++;
                });
            }

            assert(i == m_xrefCount);
            m_patchedXrefs = true;
        }
        return *this;
    }

    LimitAdjuster& ReplaceNumericRefs(std::convertible_to<intptr_t> auto&& ...pointers)
    {
        if (!m_patchedNumRefs && m_patchedXrefs)
        {
            auto TryPatchAddr = [&]<typename T>(intptr_t ptr) {
                auto val = injector::ReadMemory<T>(ptr, true);
                if (val == m_elementsCount) {
                    injector::WriteMemory<T>(ptr, T(m_elementsCount * m_increaseby), true);
                    return true;
                }
                if (val == m_elementsCount + 1) {
                    injector::WriteMemory<T>(ptr, T(m_elementsCount * m_increaseby + 1), true);
                    return true;
                }
                if (val == m_elementsCount - 1) {
                    injector::WriteMemory<T>(ptr, T(m_elementsCount * m_increaseby - 1), true);
                    return true;
                }
                return false;
            };

            auto list = std::initializer_list<intptr_t>{ pointers... };
            for (auto v : list)
            {
                if (TryPatchAddr.template operator()<uint64_t>(v))
                    continue;
                if (TryPatchAddr.template operator()<uint32_t>(v))
                    continue;
                if (TryPatchAddr.template operator()<uint16_t>(v))
                    continue;
                if (TryPatchAddr.template operator()<uint8_t>(v))
                    continue;
            }
            m_patchedNumRefs = true;
        }
        return *this;
    }
};

class Limits
{
public:
    Limits()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // Increasing all pools is not safe, leads to crashing
            static std::vector<std::string> poolNames = { "Task" };

            //[BudgetedIV]
            uint32_t nVehicleBudget = iniReader.ReadInteger("BudgetedIV", "VehicleBudget", 0);
            uint32_t nPedBudget = iniReader.ReadInteger("BudgetedIV", "PedBudget", 0);
            auto bExtendedLimits = iniReader.ReadInteger("BudgetedIV", "ExtendedLimits", 0);
            auto nLiveriesLimit = std::clamp(iniReader.ReadInteger("BudgetedIV", "LiveriesLimit", CHAR_MAX), 0, CHAR_MAX);

            if (bExtendedLimits)
            {
                poolNames.push_back("TxdStore");
                poolNames.push_back("Anim Manager");
                poolNames.push_back("PhysicsStore");
                poolNames.push_back("BoundsStore");
                poolNames.push_back("DrawblDictStore");
                poolNames.push_back("IplStore");
            }

            if (nVehicleBudget)
            {
                auto pattern = find_pattern("F7 2D ? ? ? ? 8B CA C1 E9 1F 03 CA B8 ? ? ? ? F7 2D ? ? ? ? 8B C2 C1 E8 1F 03 C2 89 0D ? ? ? ? A3 ? ? ? ? 83 C4 10 C3", "8B 0D ? ? ? ? B8 ? ? ? ? F7 E9 8B 0D");
                injector::WriteMemory(*pattern.get_first<void*>(2), nVehicleBudget, true);

                // Increase VehicleStruct pool size, to avoid certain crash with high budget
                poolNames.push_back("VehicleStruct");
            }

            if (nPedBudget)
            {
                auto pattern = find_pattern("F7 2D ? ? ? ? 8B C2 C1 E8 1F 03 C2 89 0D ? ? ? ? A3 ? ? ? ? 83 C4 10 C3", "8B 0D ? ? ? ? 8B C2 C1 E8 1F");
                injector::WriteMemory(*pattern.get_first<void*>(2), nPedBudget, true);
            }

            auto pattern = hook::pattern("8B C7 0F AF C2 8B F1");
            if (!pattern.empty())
            {
                struct atPool
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        auto name = std::string_view(*(const char**)(regs.esp + 16));
                        if (std::any_of(poolNames.begin(), poolNames.end(), [&name](auto& i) {return i == name; }) || (name == "VehicleStruct" && CText::hasViceCityStrings()))
                            regs.edi *= 2;
                        regs.eax = regs.edi * regs.edx;
                    }
                }; injector::MakeInline<atPool>(pattern.get_first(0));
            }
            else
            {
                auto pattern = hook::pattern("8B CF 0F AF C8 51");
                struct atPool
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        auto name = std::string_view(*(const char**)(regs.esp + 16));
                        if (std::any_of(poolNames.begin(), poolNames.end(), [&name](auto& i) {return i == name; }) || (name == "VehicleStruct" && CText::hasViceCityStrings()))
                            regs.edi *= 2;
                        regs.ecx = regs.edi * regs.eax;
                    }
                }; injector::MakeInline<atPool>(pattern.get_first(0));
            }

            // Drawable reference list limit, fixes In the Crosshairs mission crash with maxed out draw distance
            {
                auto pattern = hook::pattern("68 C8 32 00 00 E8 ? FF FF FF C3");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(1), 20000, true);
            }

            // Liveries
            if (nLiveriesLimit)
            {
                static std::unordered_map<uint32_t, std::array<uint32_t, CHAR_MAX + 1>> liveries;

                auto pattern = find_pattern("89 86 ? ? ? ? 89 86 ? ? ? ? 89 86 ? ? ? ? 89 86 ? ? ? ? C7 86 ? ? ? ? ? ? ? ? C7 86 ? ? ? ? ? ? ? ? C7 86 ? ? ? ? ? ? ? ? C7 86 ? ? ? ? ? ? ? ? C7 86",
                                            "89 86 ? ? ? ? 89 86 ? ? ? ? 89 86 ? ? ? ? 89 86 ? ? ? ? F3 0F 11 86 ? ? ? ? F3 0F 11 86 ? ? ? ? F3 0F 11 86 ? ? ? ? F3 0F 11 86 ? ? ? ? F3 0F 11 86 ? ? ? ? F3 0F 11 86 ? ? ? ? 80 A6 ? ? ? ? ? 88 8E ? ? ? ? 66 89 86 ? ? ? ? F3 0F 11 86 ? ? ? ? 89 86 ? ? ? ? 5E");
                if (!pattern.empty())
                {
                    struct CVehicleModelInfoInitializeHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            *(uint32_t**)(regs.esi + 0x13C) = &liveries[regs.esi][0];
                        }
                    }; injector::MakeInline<CVehicleModelInfoInitializeHook>(pattern.get_first(0), pattern.get_first(6));

                    pattern = hook::pattern("66 89 46 48 66 89 46 52 5E C3");
                    struct CBaseModelInfoTerminateHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            *(uint16_t*)(regs.esi + 0x48) = -1;
                            *(uint16_t*)(regs.esi + 0x52) = -1;

                            auto it = liveries.find(regs.esi);
                            if (it != liveries.end())
                                liveries.erase(it);
                        }
                    }; injector::MakeInline<CBaseModelInfoTerminateHook>(pattern.get_first(0), pattern.get_first(8));

                    pattern = hook::pattern("8D B3 ? ? ? ? 33 DB 8D 9B");
                    if (!pattern.empty())
                    {
                        struct CVehicleModelInfoSetVehicleDrawableHook
                        {
                            void operator()(injector::reg_pack& regs)
                            {
                                regs.esi = *(uint32_t*)(regs.ebx + 0x13C);
                            }
                        }; injector::MakeInline<CVehicleModelInfoSetVehicleDrawableHook>(pattern.get_first(0), pattern.get_first(6));
                    }
                    else
                    {
                        pattern = hook::pattern("8D B5 ? ? ? ? 83 FB 04");
                        struct CVehicleModelInfoSetVehicleDrawableHook
                        {
                            void operator()(injector::reg_pack& regs)
                            {
                                regs.esi = *(uint32_t*)(regs.ebp + 0x13C);
                            }
                        }; injector::MakeInline<CVehicleModelInfoSetVehicleDrawableHook>(pattern.get_first(0), pattern.get_first(6));
                    }

                    pattern = hook::pattern("FF B4 81 ? ? ? ? 0F BF 41 48");
                    if (!pattern.empty())
                    {
                        struct LiveryAccessHook
                        {
                            void operator()(injector::reg_pack& regs)
                            {
                                auto arr = (uint32_t**)(regs.ecx + 0x13C);
                                if (arr)
                                {
                                    auto ptr = *arr;
                                    if (ptr && (int)ptr != -1)
                                    {
                                        regs.eax = ptr[regs.eax];
                                        return;
                                    }
                                }
                                regs.eax = 0;
                            }
                        }; injector::MakeInline<LiveryAccessHook>(pattern.get_first(0), pattern.get_first(7));

                        injector::WriteMemory<uint8_t>(pattern.get_first(6), 0x50, true); // push eax
                    }
                    else
                    {
                        pattern = hook::pattern("8B 94 8B ? ? ? ? 0F BF 43");
                        struct LiveryAccessHook
                        {
                            void operator()(injector::reg_pack& regs)
                            {
                                auto arr = (uint32_t**)(regs.ebx + 0x13C);
                                if (arr)
                                {
                                    auto ptr = *arr;
                                    if (ptr && (int)ptr != -1)
                                    {
                                        regs.edx = ptr[regs.ecx];
                                        return;
                                    }
                                }
                                regs.edx = 0;
                            }
                        }; injector::MakeInline<LiveryAccessHook>(pattern.get_first(0), pattern.get_first(7));

                        pattern = hook::pattern("8B 94 83 ? ? ? ? 0F BF 43");
                        struct LiveryAccessHook2
                        {
                            void operator()(injector::reg_pack& regs)
                            {
                                auto arr = (uint32_t**)(regs.ebx + 0x13C);
                                if (arr)
                                {
                                    auto ptr = *arr;
                                    if (ptr && (int)ptr != -1)
                                    {
                                        regs.edx = ptr[regs.eax];
                                        return;
                                    }
                                }
                                regs.edx = 0;
                            }
                        }; injector::MakeInline<LiveryAccessHook2>(pattern.get_first(0), pattern.get_first(7));

                        pattern = hook::pattern("8B 8C 83 ? ? ? ? 0F BF 53");
                        struct LiveryAccessHook3
                        {
                            void operator()(injector::reg_pack& regs)
                            {
                                auto arr = (uint32_t**)(regs.ebx + 0x13C);
                                if (arr)
                                {
                                    auto ptr = *arr;
                                    if (ptr && (int)ptr != -1)
                                    {
                                        regs.ecx = ptr[regs.eax];
                                        return;
                                    }
                                }
                                regs.ecx = 0;
                            }
                        }; injector::MakeInline<LiveryAccessHook3>(pattern.get_first(0), pattern.get_first(7));
                    }
                }

                pattern = find_pattern("83 FB 04 7D 45 8D 44 24 24 6A 00", "83 FB 04 7D 47 8D 54 24 20 6A 00");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(2), nLiveriesLimit, true);
            }
        };
    }
} Limits;

std::vector<uint32_t> WeaponInfoHashes;
SafetyHookInline shGetWeaponInfoIdByHash = {};
int __cdecl getWeaponInfoIdByHash(int hash, int defaultId)
{
    constexpr int UNKNOWN_WEAPON_ID = 55;
    constexpr int CUSTOM_WEAPON_START_ID = 58;
    constexpr int WEAPON_NOT_FOUND = -1;

    // Check if this is a vanilla weapon
    int vanillaId = shGetWeaponInfoIdByHash.unsafe_ccall<int>(hash, WEAPON_NOT_FOUND);
    if (vanillaId != WEAPON_NOT_FOUND)
        return vanillaId;

    // Don't register unknown weapons
    if (defaultId == UNKNOWN_WEAPON_ID)
        return UNKNOWN_WEAPON_ID;

    // Check if this custom weapon was already registered
    auto it = std::find(WeaponInfoHashes.begin(), WeaponInfoHashes.end(), hash);
    if (it != WeaponInfoHashes.end())
        return static_cast<int>(std::distance(WeaponInfoHashes.begin(), it)) + CUSTOM_WEAPON_START_ID;

    // Register new custom weapon
    int newWeaponId = static_cast<int>(WeaponInfoHashes.size()) + CUSTOM_WEAPON_START_ID;
    WeaponInfoHashes.push_back(hash);

    return newWeaponId;
}

class ExtendedLimits
{
public:
    ExtendedLimits()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            auto bExtendedLimits = iniReader.ReadInteger("BudgetedIV", "ExtendedLimits", 0);

            if (bExtendedLimits)
            {
                {
                    enum CModelInfoStore
                    {
                        ms_baseModels,
                        ms_instanceModels,
                        ms_timeModels,
                        ms_weaponModels,
                        ms_vehicleModels,
                        ms_pedModels,
                        ms_mloModels,
                        ms_mlo,
                        stru_F27FC4,
                        ms_amat,
                        ms_2dfxRefs1,
                        ms_2dfxRefs2,
                        ms_particleAttrs,
                        ms_explosionAttrs,
                        ms_procObjsAttrs,
                        ms_ladderInfo,
                        ms_spawnPointAttrs,
                        ms_lightShaftAttrs,
                        ms_scrollBars,
                        ms_swayableAttrs,
                        ms_bouyancyAttrs,
                        ms_audioAttrs,
                        ms_worldPointAttrs,
                        ms_walkDontWalkAttrs,

                        amount
                    };

                    struct CDataStore
                    {
                        uint32_t nSize;
                        uint32_t nAllocated;
                        uint32_t pData;
                    };

                    auto pattern = hook::pattern("8B C8 E8 ? ? ? ? B9 ? ? ? ? A3");
                    auto CModelInfoStore__ms_baseModels = *pattern.get_first<CDataStore*>(8);

                    pattern = find_pattern("B9 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? FF 35", "B9 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 33 F6");
                    if (pattern.empty())
                        return;

                    for (size_t i = CModelInfoStore::ms_baseModels; i < CModelInfoStore::amount; i++)
                    {
                        CModelInfoStore__ms_baseModels[i].nSize *= 2;
                    }

                    auto ms_mloPortalStore = *pattern.get_first<CDataStore*>(1);
                    pattern = find_pattern("B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? FF 35 ? ? ? ? C7 05", "B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 33 F6");
                    auto ms_mloRoomStore = *pattern.get_first<CDataStore*>(1);

                    ms_mloPortalStore->nSize *= 2;
                    ms_mloRoomStore->nSize *= 2;
                }

                // Handling
                {
                    constexpr auto increaseby = 2;
                    auto ms_iStandardLines = 160;
                    auto ms_iBikeLines = 40;
                    auto ms_iFlyingLines = 40;
                    auto ms_iBoatLines = 40;

                    auto ms_iStandardLinesLimit = ms_iStandardLines * increaseby;
                    auto ms_iBikeLinesLimit = ms_iBikeLines * increaseby;
                    auto ms_iFlyingLinesLimit = ms_iFlyingLines * increaseby;
                    auto ms_iBoatLinesLimit = ms_iBoatLines * increaseby;

                    static std::vector<uint8_t> handling((0x110 * ms_iStandardLinesLimit) + (0x40 * ms_iBikeLinesLimit) + (0x60 * ms_iFlyingLinesLimit) + (0xE0 * ms_iBoatLinesLimit), 0);
                    auto aHandlingLines = *find_pattern("8D B0 ? ? ? ? 57 8B CE E8 ? ? ? ? 8B CE E8", "8D B0 ? ? ? ? 53 8B CE E8 ? ? ? ? 8B CE E8 ? ? ? ? 6A 01 55").get_first<uintptr_t>(2);
                    auto aBikeHandlingLines = aHandlingLines + (0x110 * ms_iStandardLines);
                    auto aFlyingHandlingLines = aHandlingLines + (0x110 * ms_iStandardLines) + (0x40 * ms_iBikeLines);
                    auto aBoatHandlingLines = aHandlingLines + (0x110 * ms_iStandardLines) + (0x40 * ms_iBikeLines) + (0x60 * ms_iFlyingLines);

                    auto HandlingLines = LimitAdjuster(aHandlingLines, 0x110, ms_iStandardLines, 26).IncreaseBy(increaseby).InsertNewArrayPointer(handling.data()).ReplaceXrefs(0x0, 0xF8, 0xFC, 0x100, 0x5F60, 0xAA00, 0xAAFC);
                    //auto BikeHandlingLines = LimitAdjuster(aBikeHandlingLines, 0x40, ms_iBikeLines, 7).IncreaseBy(increaseby).InsertNewArrayPointer(handling.data() + (0x110 * ms_iStandardLinesLimit)).ReplaceXrefs(0);
                    auto FlyingHandlingLines = LimitAdjuster(aFlyingHandlingLines, 0x40, ms_iFlyingLines, 5).IncreaseBy(increaseby).InsertNewArrayPointer(handling.data() + (0x110 * ms_iStandardLinesLimit) + (0x40 * ms_iBikeLinesLimit)).ReplaceXrefs(0);
                    auto BoatHandlingLines = LimitAdjuster(aBoatHandlingLines, 0x40, ms_iBoatLines, 5).IncreaseBy(increaseby).InsertNewArrayPointer(handling.data() + (0x110 * ms_iStandardLinesLimit) + (0x40 * ms_iBikeLinesLimit) + (0x60 * ms_iFlyingLinesLimit)).ReplaceXrefs(0);

                    auto pattern = find_pattern("BF 9F 00 00 00 8D 64 24 ? 8B CE E8 ? ? ? ? 81 C6 ? ? ? ? 4F 79 ? 5F 5E C3", "BE 9F ? ? ? EB 03 8D 49 00 8B CA E8 ? ? ? ? 81 C2 ? ? ? ? 83 EE 01 79 EE 5E C3");
                    injector::WriteMemory(pattern.get_first(1), ms_iStandardLinesLimit - 1, true);
                    pattern = find_pattern("BF ? ? ? ? 8D 64 24 00 8B CE E8 ? ? ? ? 83 C6 40 4F 79 F3 5F 5E C3 56 57 BE ? ? ? ? BF ? ? ? ? 8D 64 24 00 8B CE E8 ? ? ? ? 81 C6 ? ? ? ? 4F 79 F0 5F", "BA ? ? ? ? 8D 9B ? ? ? ? E8 ? ? ? ? 83 C1 40 83 EA 01 79 F3 C3");
                    injector::WriteMemory(pattern.get_first(1), ms_iBikeLinesLimit - 1, true);
                    pattern = find_pattern("BF ? ? ? ? 8D 64 24 00 8B CE E8 ? ? ? ? 83 C6 60 4F 79 F3 5F 5E C3 56", "BA ? ? ? ? 8D 9B ? ? ? ? E8 ? ? ? ? 83 C1 60 83 EA 01 79 F3 C3");
                    injector::WriteMemory(pattern.get_first(1), ms_iFlyingLinesLimit - 1, true);
                    pattern = find_pattern("BF 27 00 00 00 8D 64 24 ? 8B CE E8 ? ? ? ? 81 C6 ? ? ? ? 4F 79 ? 5F 5E C3", "BA 27 ? ? ? 8D 9B ? ? ? ? E8 ? ? ? ? 81 C1 ? ? ? ? 83 EA 01 79 F0 C3");
                    injector::WriteMemory(pattern.get_first(1), ms_iBoatLinesLimit - 1, true);

                    pattern = find_pattern("7D 1B 8B C2", "7D 19 56 8B F2");        // bikeHandlingCount
                    injector::MakeNOP(pattern.get_first(), 2);

                    pattern = find_pattern("7D 1C 8D 04 52", "7D 1A 56 8D 34 49");  // flyingHandlingCount
                    injector::MakeNOP(pattern.get_first(), 2);

                    pattern = find_pattern("7D 1E 8B C2", "7D 1C 56 8B F2");        // boatHandlingCount
                    injector::MakeNOP(pattern.get_first(), 2);
                }

                // Carcols
                {
                    auto pattern = find_pattern("8B 87 ? ? ? ? 25 ? ? ? ? 0B C8 89 8F", "8B 94 36 ? ? ? ? 03 F6 33 C0 8A A4 24 ? ? ? ? 81 E2 ? ? ? ? C1 E1 10 0B CA");
                    auto ref1 = (intptr_t)find_pattern("81 3D ? ? ? ? ? ? ? ? 0F 8D ? ? ? ? 8D 84 24", "81 3D ? ? ? ? ? ? ? ? 0F 8D ? ? ? ? 8D 8C 24 ? ? ? ? 51 8D 94 24 ? ? ? ? 52 8D 84 24 ? ? ? ? 50").get_first(6);
                    auto ref2 = (intptr_t)find_pattern("81 FA ? ? ? ? 0F 8D ? ? ? ? 42", "81 3D ? ? ? ? ? ? ? ? 0F 8D ? ? ? ? 83 05 ? ? ? ? ? E9 ? ? ? ? 83 FD 02").get_first(2);
                    auto adwCarColors = LimitAdjuster(*pattern.get_first<uintptr_t>(2), 4, 196, 23).ReplaceXrefs(0).ReplaceNumericRefs(ref1, ref2);
                    pattern = find_pattern("8B 04 8D ? ? ? ? 89 06", "8B 0C 85 ? ? ? ? 89 0E");
                    auto adwPoliceScannerCarColorPrefixes = LimitAdjuster(*pattern.get_first<uintptr_t>(3), 4, 197, 3).ReplaceXrefs(0);
                    pattern = find_pattern("8B 04 8D ? ? ? ? 89 46 04", "8B 14 85 ? ? ? ? 89 56 04");
                    auto adwPoliceScannerCarColors = LimitAdjuster(*pattern.get_first<uintptr_t>(3), 4, 196, 3).ReplaceXrefs(0);
                }

                // WeaponInfo
                {
                    auto pattern = find_pattern("81 C3 ? ? ? ? 89 03", "81 C7 ? ? ? ? 89 07");
                    auto ref1 = (intptr_t)find_pattern("BF ? ? ? ? 8D 64 24 00 8B CE E8 ? ? ? ? 81 C6 ? ? ? ? 4F 79 F0 68 ? ? ? ? E8 ? ? ? ? 83 C4 04 5F 5E C3 56", "BE ? ? ? ? EB 03 8D 49 00 E8 ? ? ? ? 81 C1 ? ? ? ? 83 EE 01 79 F0 68 ? ? ? ? E8 ? ? ? ? 83 C4 04 5E C3").get_first(1);
                    auto ref2 = (intptr_t)find_pattern("83 F8 3C 7C F1", "83 F8 3C 7C EF").get_first(2);
                    auto WeaponInfo = LimitAdjuster(*pattern.get_first<uintptr_t>(2), 0x110, 60, 16).ReplaceXrefs(0, 0x24, 0x1A98, 0x1A9C, 0x1AB4, 0x1B30, 0x3430, 0x3540, 0x363C, 0x3870, 0x3980, 0x3DC0).ReplaceNumericRefs(ref1, ref2);
                        
                    pattern = hook::pattern("7D 0C 69 C0");
                    injector::MakeNOP(pattern.get_first(), 2);

                    pattern = find_pattern("C0 3B 0C 85 ? ? ? ? 74 0A 40", "C0 3B 0C 85 ? ? ? ? 74 0C");
                    if (!pattern.empty())
                    {
                        shGetWeaponInfoIdByHash = safetyhook::create_inline(pattern.get_first(-5), getWeaponInfoIdByHash);
                    }
                }

                // VehOff
                {
                    auto pattern = find_pattern("81 C7 ? ? ? ? 83 BB", "81 C7 ? ? ? ? 83 BE");
                    auto VehOff = LimitAdjuster(*pattern.get_first<uintptr_t>(2), 640, 205, 6).ReplaceXrefs(0, 0x1C0, 0x1E0);
                }
            }
        };
    }
} ExtendedLimits;