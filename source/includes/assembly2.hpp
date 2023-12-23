#pragma once

#include "injector/injector.hpp"
#include <safetyhook/safetyhook.hpp>

namespace injector
{
    // Lowest level stuff (actual assembly) goes on the following namespace
    // PRIVATE! Skip this, not interesting for you.
    namespace injector_asm2
    {
        // Wrapper functor, so the assembly can use some templating
        template<class T>
        struct wrapper
        {
            static void call(SafetyHookContext* regs)
            {
                T fun; fun(*regs);
            }
        };

        // Constructs a SafetyHookContext and calls the wrapper functor
        template<class W>   // where W is of type wrapper
        inline void make_SafetyHookContext_and_call(memory_pointer_tr at)
        {
            static std::unique_ptr<SafetyHookMid> pack;
            auto m = safetyhook::create_mid(at.get<void>(), [](SafetyHookContext& ctx)
            {
                W::call(&ctx);
            });
            pack.reset(new SafetyHookMid(std::move(m)));
        }
    };

    /*
     *  MakeInline
     *      Makes inline assembly (but not assembly, an actual functor of type FuncT) at address
     */
    template<class FuncT>
    void MakeInline2(memory_pointer_tr at)
    {
        typedef injector_asm2::wrapper<FuncT> functor;
        if(false) functor::call(nullptr);   // To instantiate the template, if not done _asm will fail
        injector_asm2::make_SafetyHookContext_and_call<functor>(at);
    }

    /*
     *  MakeInline
     *      Same as above, but it NOPs everything between at and end (exclusive), then performs MakeInline
     */
    template<class FuncT>
    void MakeInline2(memory_pointer_tr at, memory_pointer_tr end)
    {
        MakeRangedNOP(at, end);
        MakeInline2<FuncT>(at);
    }

    /*
     *  MakeInline
     *      Same as above, but (at,end) are template parameters.
     *      On this case the functor can be passed as argument since there will be one func instance for each at,end not just for each FuncT
     */
    template<uintptr_t at, uintptr_t end, class FuncT>
    void MakeInline2(FuncT func)
    {
        static std::unique_ptr<FuncT> static_func;
        static_func.reset(new FuncT(std::move(func)));

        // Encapsulates the call to static_func
        struct Caps
        {
            void operator()(SafetyHookContext& regs)
            { (*static_func)(regs); }
        };

        // Does the actual MakeInline
        return MakeInline2<Caps>(lazy_pointer<at>::get(), lazy_pointer<end>::get());
    }

    /*
     *  MakeInline
     *      Same as above, but (end) is calculated by the length of a call instruction
     */
    template<uintptr_t at, class FuncT>
    void MakeInline2(FuncT func)
    {
        return MakeInline2<at, at+5, FuncT>(func);
    }
};
