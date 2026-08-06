module;

#include <common.hxx>

export module classext;

import common;

/*export class CEntityExt
{

};*/

export class CPedIntelligenceExt
{
public:
    float m_fTimeStepAccumulator = 0.0f;
};

export class CTimerExt
{
public:
    static uint32_t m_logicalFrameCounter;
    static uint32_t m_logicalFramesPassed;

public:
    static uint32_t GetLogicalFrameCounter()
    {
        return m_logicalFrameCounter;
    }

    static uint32_t GetLogicalFramesPassed() 
    { 
        return m_logicalFramesPassed;
    }
};

export class CCamFollowVehicleExt
{
public:
    float m_fTimeStepAccumulator = 0.0f;

    float m_fOldSpringOffsetX = 0.0f;
    float m_fOldSpringOffsetY = 0.0f;
    float m_fOldSpringOffsetZ = 0.0f;
};

class ClassExtender
{
private:
    //static inline std::unordered_map<uintptr_t, CEntityExt> entityExtensions;
    static inline std::unordered_map<uintptr_t, CPedIntelligenceExt> pedIntelligenceExtensions;
    static inline std::unordered_map<uintptr_t, CCamFollowVehicleExt> camFollowVehicleExtensions;

    static auto* Find(auto& map, uintptr_t key)
    {
        auto it = map.find(key);
        return (it != map.end()) ? &it->second : nullptr;
    }

public:
    //static CEntityExt* GetEntityExt(uintptr_t entity) { return Find(entityExtensions, entity); }
    static CPedIntelligenceExt* GetPedIntelligenceExt(uintptr_t pedintelligence) { return Find(pedIntelligenceExtensions, pedintelligence); }
    static CCamFollowVehicleExt* GetCamFollowVehicleExt(uintptr_t camfollowvehicle) { return Find(camFollowVehicleExtensions, camfollowvehicle); }

    ClassExtender()
    {
        FusionFix::onInitEvent() += []()
        {
            // Unused for now
            /*entityExtensions.reserve(2048);
             
            auto pattern = find_pattern("C7 06 ? ? ? ? C7 46 ? ? ? ? ? C7 46 ? ? ? ? ? C7 46 ? ? ? ? ? C7 46 ? ? ? ? ? C7 46 ? ? ? ? ? 8B 46",
                                        "C7 06 ? ? ? ? F3 0F 11 46 ? F3 0F 11 46 ? F3 0F 11 46 ? F3 0F 11 46 ? 89 5E");
            static auto CEntity__ctor_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                entityExtensions[regs.esi] = CEntityExt{};
            });
            
            pattern = hook::pattern("C7 06 ? ? ? ? 74 ? E8 ? ? ? ? 8B 46");
            static auto CEntity__dtor_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                entityExtensions.erase(regs.esi);
            });*/

            pedIntelligenceExtensions.reserve(256);

            auto pattern = hook::pattern("C7 03 ? ? ? ? E8 ? ? ? ? 8B 7C 24");
            if (!pattern.empty())
            {
                static auto CPedIntelligence__ctor_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    pedIntelligenceExtensions[regs.ebx] = CPedIntelligenceExt{};
                });
            }
            else
            {
                pattern = hook::pattern("C7 06 ? ? ? ? E8 ? ? ? ? 8B 7C 24");
                static auto CPedIntelligence__ctor_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    pedIntelligenceExtensions[regs.esi] = CPedIntelligenceExt{};
                });
            }

            pattern = hook::pattern("C7 07 ? ? ? ? 85 C9 74 ? 8B 01");
            if (!pattern.empty())
            {
                static auto CPedIntelligence__dtor_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    pedIntelligenceExtensions.erase(regs.edi);
                });
            }
            else
            {
                pattern = hook::pattern("C7 06 ? ? ? ? 74 ? 8B 01 8B 10 6A ? FF D2 C7 86 ? ? ? ? ? ? ? ? 8D 8E");
                static auto CPedIntelligence__dtor_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    pedIntelligenceExtensions.erase(regs.esi);
                });
            }

            camFollowVehicleExtensions.reserve(32);

            pattern = find_pattern("C7 06 ? ? ? ? 8B CE C7 86", "C7 06 ? ? ? ? 8B CE 89 86 ? ? ? ? 89 86");
            static auto CCamFollowVehicle__ctor_Hook = safetyhook::create_mid(pattern.count(2).get(0).get<void*>(0), [](SafetyHookContext& regs)
            {
                camFollowVehicleExtensions[regs.esi] = CCamFollowVehicleExt{};
            });

            pattern = find_pattern("C7 06 ? ? ? ? E8 ? ? ? ? 8B CE C7 05", "C7 06 ? ? ? ? E8 ? ? ? ? F3 0F 10 05 ? ? ? ? 8B CE");
            static auto CCamFollowVehicle__dtor_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                camFollowVehicleExtensions.erase(regs.esi);
            });
        };
    }
} ClassExtender;

/*export inline CEntityExt* GetEntityExt(uintptr_t entity)
{
    return ClassExtender::GetEntityExt(entity);
}*/

export inline CPedIntelligenceExt* GetPedIntelligenceExt(uintptr_t pedintelligence)
{
    return ClassExtender::GetPedIntelligenceExt(pedintelligence);
}

export inline CCamFollowVehicleExt* GetCamFollowVehicleExt(uintptr_t camfollowvehicle)
{
    return ClassExtender::GetCamFollowVehicleExt(camfollowvehicle);
}