module;

#include <common.hxx>
#include <array>

export module limits;

import common;

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
            auto max_array_offset = m_elementSize * m_elementsCount;
            auto max_needed_offset = *std::max_element(list.begin(), list.end());
            auto i = 0;

            if (m_array == nullptr) {
                auto vec = new std::vector<uint8_t>(m_increaseby * max_array_offset + ((max_needed_offset > max_array_offset) ? max_needed_offset - max_array_offset : 0), 0);
                m_array = vec->data();
            }

            for (auto v : list)
            {
                auto ptr = m_startAddress + v;
                auto x = pattern_str(to_bytes(ptr));
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
                    injector::WriteMemory<T>(ptr, m_elementsCount * m_increaseby, true);
                    return true;
                }
                if (val == m_elementsCount + 1) {
                    injector::WriteMemory<T>(ptr, m_elementsCount * m_increaseby + 1, true);
                    return true;
                }
                if (val == m_elementsCount - 1) {
                    injector::WriteMemory<T>(ptr, m_elementsCount * m_increaseby - 1, true);
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

            //[BudgetedIV]
            uint32_t nVehicleBudget = iniReader.ReadInteger("BudgetedIV", "VehicleBudget", 0);
            uint32_t nPedBudget = iniReader.ReadInteger("BudgetedIV", "PedBudget", 0);

            if (nVehicleBudget)
            {
                auto pattern = find_pattern("F7 2D ? ? ? ? 8B CA C1 E9 1F 03 CA B8 ? ? ? ? F7 2D ? ? ? ? 8B C2 C1 E8 1F 03 C2 89 0D ? ? ? ? A3 ? ? ? ? 83 C4 10 C3", "8B 0D ? ? ? ? B8 ? ? ? ? F7 E9 8B 0D");
                injector::WriteMemory(*pattern.get_first<void*>(2), nVehicleBudget, true);

                // Increase VehicleStruct pool size, to avoid certain crash with high budget
                pattern = hook::pattern("6A 32 8B C8 E8 ? ? ? ? A3");
                injector::WriteMemory<uint8_t>(pattern.get_first(1), 50 * 2, true);
            }

            if (nPedBudget)
            {
                auto pattern = find_pattern("F7 2D ? ? ? ? 8B C2 C1 E8 1F 03 C2 89 0D ? ? ? ? A3 ? ? ? ? 83 C4 10 C3", "8B 0D ? ? ? ? 8B C2 C1 E8 1F");
                injector::WriteMemory(*pattern.get_first<void*>(2), nPedBudget, true);
            }
        };
    }
} Limits;