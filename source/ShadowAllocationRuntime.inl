// Included after module imports and CShadows declarations. This experimental
// adapter is opt-in and has not been run in GTA IV. No install occurs from the
// offline build; the game integration activates only on a later chosen launch.
namespace PlayerShadowAllocation
{
    namespace allocation = fusionfix::shadows::ce::allocation;
    namespace budget = fusionfix::shadows::budget;
    using fusionfix::shadows::Vec3;
    using fusionfix::shadows::FloatingPointState;

    static SafetyHookInline selectionHook;
    static SafetyHookMid collectHook, finalizeHook;
    static std::atomic<bool> ready{false}, unsupportedThread{false};
    static std::atomic<DWORD> ownerThread{0};
    static uintptr_t gameBase = 0;
    static bool publicationEnabled = false; // Immutable after ready is published.
    static std::string installStatus = "not_requested"; // Init-only; diagnostics reads after ready publication.
    // Diagnostics are counters only: no per-frame logging/allocations or I/O.
    static std::atomic<uint32_t> appliedPasses{0}, observedPasses{0}, fallbackPasses{0};

    struct State
    {
        budget::ShadowAllocationPass pass;
        Vec3 player{};
        uintptr_t ped = 0, occupiedCar = 0, lastCar = 0;
        uint32_t frame = 0;
        uintptr_t stackAnchor = 0;
        unsigned depth = 0;
    };
    static thread_local State state;

    struct Invocation
    {
        Invocation() noexcept { ++state.depth; }
        ~Invocation() noexcept { state.pass.EndInvocation(); --state.depth; }
        Invocation(const Invocation&) = delete;
        Invocation& operator=(const Invocation&) = delete;
    };
    static void RejectPass() noexcept
    {
        if (state.pass.Active()) ++fallbackPasses;
        state.pass.Cancel();
    }

    static rage::CLightSource* CurrentLights() noexcept
    {
        return *reinterpret_cast<rage::CLightSource**>(gameBase + allocation::LightArrayPointerRva);
    }
    static uint32_t CurrentCount() noexcept
    {
        return *reinterpret_cast<uint32_t*>(gameBase + allocation::LightCountRva);
    }
    static bool Enabled() noexcept
    {
        return ready.load(std::memory_order_acquire) && !unsupportedThread.load(std::memory_order_relaxed) &&
            bExtraNightShadows && bHeadlightShadows && bVehicleNightShadows;
    }
    static bool Prepare() noexcept
    {
        if (!Enabled() || !CShadows::pFrameCounter || !CTimer::m_snTimeInMilliseconds ||
            !CPlayer::getLocalPlayerPed || !CPlayer::findPlayerCar)
            return false;
        const auto ped = CPlayer::getLocalPlayerPed();
        if (!ped) return false;
        const auto matrix = *reinterpret_cast<const float* const*>(ped + 0x20);
        if (!matrix) return false;
        const Vec3 position{matrix[12], matrix[13], matrix[14]};
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return false;
        if (ped != state.ped) state.lastCar = 0;
        state.ped = ped;
        state.player = position;
        state.occupiedCar = CPlayer::findPlayerCar();
        if (state.occupiedCar) state.lastCar = state.occupiedCar;
        state.frame = *CShadows::pFrameCounter;
        state.stackAnchor = 0;
        if (reinterpret_cast<uintptr_t>(CurrentLights()) < 0x10000) return false;
        state.pass.Begin({state.frame, static_cast<uint32_t>(*CTimer::m_snTimeInMilliseconds),
                          ped, state.occupiedCar != 0}, CurrentLights(), CurrentCount());
        return state.pass.Active();
    }

    static bool InfluencesPlayer(const rage::CLightSource& light) noexcept
    {
        return fusionfix::shadows::LightVolumeContains(state.player,
            {light.mPosition.x, light.mPosition.y, light.mPosition.z},
            {light.mDirection.x, light.mDirection.y, light.mDirection.z},
            light.mType, light.mRadius, light.mOuterConeAngle);
    }

    static uint64_t LampGeometry(const rage::CLightSource& light) noexcept
    {
        // A stationary lamp key reused at a new position must not inherit its
        // old slot's hold interval. Vehicle keys lack a verified generation
        // field; the adapter does not claim to detect every pool-pointer reuse.
        uint32_t words[6];
        std::memcpy(words, &light.mPosition, 12);
        words[3] = static_cast<uint32_t>(light.mType);
        words[4] = static_cast<uint32_t>(light.mInteriorIndex);
        words[5] = static_cast<uint32_t>(light.mRoomIndex);
        uint64_t hash = 14695981039346656037ull;
        for (auto word : words) { hash ^= word; hash *= 1099511628211ull; }
        return hash;
    }

    static void Collect(SafetyHookContext& regs) noexcept
    {
        const FloatingPointState fp;
        if (!Enabled() || state.depth != 1 || !state.pass.Active()) return;
        if (!state.stackAnchor) state.stackAnchor = regs.esp;
        if (state.stackAnchor != regs.esp) { RejectPass(); return; }
        const auto index = static_cast<uint32_t>(regs.edx);
        auto* lights = CurrentLights();
        const auto count = CurrentCount();
        const auto address = reinterpret_cast<uintptr_t>(lights);
        if (address < 0x10000 || count > 4096 ||
            address > UINTPTR_MAX - static_cast<uintptr_t>(count) * sizeof(rage::CLightSource) ||
            index >= count || regs.edi != address ||
            regs.esi != index * sizeof(rage::CLightSource) ||
            *reinterpret_cast<const uint32_t*>(regs.esp + 0x14) != index ||
            *reinterpret_cast<const uint32_t*>(regs.esp + 0x18) != regs.esi)
        {
            RejectPass();
            return;
        }
        const auto& light = lights[index];
        const auto flags = *reinterpret_cast<const uint32_t*>(regs.esp + 0x1C);
        if (flags != light.mFlags || !(flags & 4u) || ((flags & 2u) && light.mShadowCacheIndex < 0))
        {
            RejectPass();
            return;
        }
        const auto key = static_cast<uint32_t>(light.mCastShadows); // Audited opaque +60 key.
        const auto geometry = fusionfix::shadows::EvaluateGeometry(state.player,
            {light.mPosition.x, light.mPosition.y, light.mPosition.z});
        auto kind = budget::Kind::Lamp;
        if (flags & 0x100u)
            kind = fusionfix::shadows::ce::IsVehicleBeam(key, state.occupiedCar ? state.occupiedCar : state.lastCar)
                ? budget::Kind::PlayerBeam : budget::Kind::OtherBeam;
        state.pass.Observe({key, index, kind, geometry.distanceSquared,
                           InfluencesPlayer(light), true,
                           kind == budget::Kind::Lamp ? LampGeometry(light) : 0}, &light);
        if (!state.pass.Active()) ++fallbackPasses;
    }

    static void Finalize(SafetyHookContext& regs) noexcept
    {
        const FloatingPointState fp;
        if (!Enabled() || state.depth != 1 || !state.pass.Active()) return;
        if (!state.stackAnchor || state.stackAnchor != regs.esp ||
            !CShadows::pFrameCounter || *CShadows::pFrameCounter != state.frame ||
            CPlayer::getLocalPlayerPed() != state.ped || CPlayer::findPlayerCar() != state.occupiedCar)
        {
            RejectPass();
            return;
        }
        auto* indices = reinterpret_cast<int32_t*>(regs.esp + allocation::SelectedIndicesStackOffset);
        // Observe mode tests the whole transaction against a private copy.
        // Switching to publication requires a new explicitly selected launch.
        std::array<int32_t, 7> observation{};
        if (!publicationEnabled) std::memcpy(observation.data(), indices, sizeof(observation));
        if (state.pass.Commit(CurrentLights(), CurrentCount(), publicationEnabled ? indices : observation.data()))
        {
            if (publicationEnabled) ++appliedPasses;
            else ++observedPasses;
        }
        else ++fallbackPasses;
    }

    static void __cdecl Select()
    {
        const Invocation invocation;
        {
            const FloatingPointState fp;
            DWORD expected = 0;
            const DWORD current = GetCurrentThreadId();
            ownerThread.compare_exchange_strong(expected, current, std::memory_order_relaxed);
            if (ownerThread.load(std::memory_order_relaxed) != current)
                unsupportedThread.store(true, std::memory_order_relaxed);
            if (state.depth != 1 || !Prepare()) RejectPass();
        }
        // Hooks are immutable once published. The unsafe call avoids the
        // hook-object mutex around native execution (including recursion).
        selectionHook.unsafe_ccall<void>();
    }

    static bool Install(bool publish)
    {
        gameBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        publicationEnabled = publish;
        if (!allocation::ValidateMappedImage(reinterpret_cast<const uint8_t*>(gameBase),
                fusionfix::shadows::ce::ImageSize, gameBase))
        {
            installStatus = "guard_failed " + fusionfix::shadows::ce::diagnostics::Describe(
                reinterpret_cast<const uint8_t*>(gameBase), fusionfix::shadows::ce::ImageSize, gameBase);
            return false;
        }
        // Prepare all trampolines before any write. Mid hooks are inert until
        // ready is published AND execution is inside our selection wrapper.
        auto begin = safetyhook::InlineHook::create(gameBase + allocation::SelectionRva,
            Select, safetyhook::InlineHook::StartDisabled);
        auto collect = safetyhook::MidHook::create(gameBase + allocation::CollectRva,
            Collect, safetyhook::MidHook::StartDisabled);
        auto finish = safetyhook::MidHook::create(gameBase + allocation::FinalizeRva,
            Finalize, safetyhook::MidHook::StartDisabled);
        if (!begin || !collect || !finish)
        {
            installStatus = std::string("hook_creation_failed begin=") + (begin ? "1" : "0") +
                " collect=" + (collect ? "1" : "0") + " finish=" + (finish ? "1" : "0");
            return false;
        }
        selectionHook = std::move(*begin);
        collectHook = std::move(*collect);
        finalizeHook = std::move(*finish);
        if (!collectHook.enable() || !finalizeHook.enable() || !selectionHook.enable())
        {
            installStatus = "hook_enable_failed";
            // Leave any enabled trampolines owned and inert; freeing them while
            // another thread is executing a stub would be unsafe.
            return false;
        }
        ready.store(true, std::memory_order_release);
        installStatus = "enabled";
        OutputDebugStringW(publish ? L"FusionFix experimental shadow allocator: publication enabled.\n" :
            L"FusionFix experimental shadow allocator: observing only; native output unchanged.\n");
        return true;
    }
}
