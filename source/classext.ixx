module;

#include <common.hxx>

export module classext;

import common;

export class CEntityExt
{

};

export class CPedIntelligenceExt
{
public:
    float m_fTimeStepAccumulator = 0.0f;
};

class ClassExtender
{
private:
    static inline std::unordered_map<uintptr_t, CEntityExt> entityExtensions;
    static inline std::unordered_map<uintptr_t, CPedIntelligenceExt> pedIntelligenceExtensions;

    static auto* Find(auto& map, uintptr_t key)
    {
        auto it = map.find(key);
        return (it != map.end()) ? &it->second : nullptr;
    }

public:
    static CEntityExt* GetEntityExt(uintptr_t entity) { return Find(entityExtensions, entity); }
    static CPedIntelligenceExt* GetPedIntelligenceExt(uintptr_t entity) { return Find(pedIntelligenceExtensions, entity); }

    ClassExtender()
    {
        FusionFix::onInitEvent() += []()
        {
            //unused for now
            //entityExtensions.reserve(2048);
            // 
            //auto pattern = find_pattern("8B 46 ? 25 ? ? ? ? 0D ? ? ? ? 89 46 ? B8 ? ? ? ? 66 21 46", "8B 46 ? 66 81 66 ? ? ? 66 81 66");
            //static auto CEntityCtor = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            //{
            //    entityExtensions[regs.esi] = CEntityExt{};
            //});
            //
            //pattern = find_pattern("C7 06 ? ? ? ? 74 ? E8 ? ? ? ? 8B 46");
            //static auto CEntityDtor = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            //{
            //    entityExtensions.erase(regs.esi);
            //});

            pedIntelligenceExtensions.reserve(256);

            auto pattern = find_pattern("66 C7 83 ? ? ? ? ? ? E8 ? ? ? ? 8D 8B");
            if (!pattern.empty())
            {
                static auto CPedIntelligenceCtor = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    pedIntelligenceExtensions[regs.ebx] = CPedIntelligenceExt{};
                });
            }
            else
            {
                pattern = find_pattern("F3 0F 11 86 ? ? ? ? F3 0F 11 86 ? ? ? ? F3 0F 10 05 ? ? ? ? 8D 8E");
                static auto CPedIntelligenceCtor = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    pedIntelligenceExtensions[regs.esi] = CPedIntelligenceExt{};
                });
            }

            pattern = find_pattern("8D 8F ? ? ? ? E8 ? ? ? ? 8D B7 ? ? ? ? 56");
            if (!pattern.empty())
            {
                static auto CPedIntelligenceDtor = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    pedIntelligenceExtensions.erase(regs.edi);
                });
            }
            else
            {
                pattern = find_pattern("8D 8E ? ? ? ? E8 ? ? ? ? 8D BE ? ? ? ? 57");
                static auto CPedIntelligenceDtor = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    pedIntelligenceExtensions.erase(regs.esi);
                });
            }
        };
    }
} ClassExtender;

export inline CEntityExt* GetEntityExt(uintptr_t entity)
{
    return ClassExtender::GetEntityExt(entity);
}

export inline CPedIntelligenceExt* GetPedIntelligenceExt(uintptr_t entity)
{
    return ClassExtender::GetPedIntelligenceExt(entity);
}