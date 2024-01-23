#pragma once

#include "injector/injector.hpp"
#include <safetyhook/safetyhook.hpp>

namespace injector
{
    using reg_pack = SafetyHookContext;
    namespace injector_asm
    {
        // Wrapper functor, so the assembly can use some templating
        template<class T>
        struct wrapper
        {
            static void call(reg_pack* regs)
            {
                T fun; fun(*regs);
            }
        };

        // Constructs a reg_pack and calls the wrapper functor
        template<class W>   // where W is of type wrapper
        inline void make_reg_pack_and_call(memory_pointer_tr at)
        {
            static std::vector<SafetyHookMid> pack;
            auto m = safetyhook::create_mid(at.get<void>(), [](reg_pack& ctx)
            {
                W::call(&ctx);
            });
            pack.emplace_back(std::move(m));
        }
    };

    /*
     *  MakeInline
     *      Makes inline assembly (but not assembly, an actual functor of type FuncT) at address
     */
    template<class FuncT>
    void MakeInline(memory_pointer_tr at)
    {
        MakeNOP(at, 5);
        typedef injector_asm::wrapper<FuncT> functor;
        if(false) functor::call(nullptr);   // To instantiate the template, if not done _asm will fail
        injector_asm::make_reg_pack_and_call<functor>(at);
    }

    /*
     *  MakeInline
     *      Same as above, but it NOPs everything between at and end (exclusive), then performs MakeInline
     */
    template<class FuncT>
    void MakeInline(memory_pointer_tr at, memory_pointer_tr end)
    {
        MakeRangedNOP(at, end);
        MakeInline<FuncT>(at);
    }

    /*
     *  MakeInline
     *      Same as above, but (at,end) are template parameters.
     *      On this case the functor can be passed as argument since there will be one func instance for each at,end not just for each FuncT
     */
    template<uintptr_t at, uintptr_t end, class FuncT>
    void MakeInline(FuncT func)
    {
        static std::unique_ptr<FuncT> static_func;
        static_func.reset(new FuncT(std::move(func)));

        // Encapsulates the call to static_func
        struct Caps
        {
            void operator()(reg_pack& regs)
            { (*static_func)(regs); }
        };

        // Does the actual MakeInline
        return MakeInline<Caps>(lazy_pointer<at>::get(), lazy_pointer<end>::get());
    }

    /*
     *  MakeInline
     *      Same as above, but (end) is calculated by the length of a call instruction
     */
    template<uintptr_t at, class FuncT>
    void MakeInline(FuncT func)
    {
        MakeNOP(at, 5);
        return MakeInline<at, at+5, FuncT>(func);
    }
};

namespace injector
{
    static inline constexpr auto JMPSIZE = 14;
    static inline constexpr auto CALLSIZE = 16;

    inline injector::memory_pointer_raw MakeAbsCALL64(injector::memory_pointer_tr at, injector::memory_pointer_raw dest, bool vp = true)
    {
        injector::WriteMemory<uint16_t>(at, 0x15FF, vp);
        injector::WriteMemory<uint32_t>(at + sizeof(uint16_t), 2, vp);
        injector::WriteMemory<uint16_t>(at + sizeof(uint16_t) + sizeof(uint32_t), 0x08EB, vp);
        injector::WriteMemory<uint64_t>(at + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint16_t), dest.as_int(), vp);
        return at.as_int() + CALLSIZE;
    }

    inline injector::memory_pointer_raw MakeAbsJMP64(injector::memory_pointer_tr at, injector::memory_pointer_raw dest, bool vp = true)
    {
        injector::WriteMemory<uint16_t>(at, 0x25FF, vp);
        injector::WriteMemory<uint32_t>(at + sizeof(uint16_t), 0, vp);
        injector::WriteMemory<uint64_t>(at + sizeof(uint16_t) + sizeof(uint32_t), dest.as_int(), vp);
        return at.as_int() + JMPSIZE;
    }

    inline injector::memory_pointer_raw ReadRelativeAddress(memory_pointer_tr at, size_t sizeof_addr = 4, bool vp = true)
    {
        uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
        switch (sizeof_addr)
        {
        case 1: return (base + ReadMemory<int8_t>(at, vp));
        case 2: return (base + ReadMemory<int16_t>(at, vp));
        case 4: return (base + ReadMemory<int32_t>(at, vp));
        }
        return nullptr;
    }

    inline bool UnprotectMemory(memory_pointer_tr addr, size_t size)
    {
        DWORD out_oldprotect = 0;
        return VirtualProtect(addr.get(), size, PAGE_EXECUTE_READWRITE, &out_oldprotect) != 0;
    }
};

