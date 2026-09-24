// FusionFix's existing light-admission workaround moved from fixes.ixx.
// It edits CE 927BD4 inside the allocator's guarded selection function. Keep
// ONE installer, after both startup guards and allocator hook preparation.
namespace NightShadowAdmission
{
    static SafetyHookMid admissionHook;
    static uintptr_t skip = 0;

    static bool Install()
    {
        auto pattern = hook::pattern("A8 ? 0F 84 ? ? ? ? 8B C8");
        if (pattern.empty()) return false;
        skip = resolve_next_displacement(pattern.get_first(0)).value();
        injector::MakeNOP(pattern.get_first(2), 6);
        admissionHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
        {
            static auto extra = FusionFixSettings.GetRef("PREF_EXTRANIGHTSHADOWS");
            if (extra->get())
            {
                if ((regs.eax & 6) != 0) return;
            }
            else if ((regs.eax & 6) != 0 && Natives::IsInteriorScene()) return;
            return_to(skip);
        });
        return static_cast<bool>(admissionHook);
    }
}
