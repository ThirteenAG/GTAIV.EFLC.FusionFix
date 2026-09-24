// Candidate 14: narrowly scoped immediate-render experiment. AE3310 may queue
// AE0690 instead: that path does not visit the entity hook and is NOT fixed here.
namespace OwnHeadlightCaster
{
    namespace policy = fusionfix::shadows::caster;
    namespace guard = fusionfix::shadows::ce::casterguard;
    static thread_local policy::Context context;
    static std::atomic<bool> enabled{false};
    static uintptr_t base = 0;
    static std::atomic<uint32_t> passes{0}, deferredPasses{0}, ownPasses{0};
    static std::atomic<uint32_t> casterVisits{0}, carExcluded{0}, occupantsExcluded{0};

    static policy::Context Capture(void* renderPass) noexcept
    {
        policy::Context result{};
        if (!enabled.load(std::memory_order_acquire) || !renderPass ||
            !bHeadlightShadows || !bVehicleNightShadows || !CPlayer::findPlayerCar)
            return result;
        ++passes;
        // TLS identity cannot be carried into the deferred renderer. Observe it
        // explicitly and leave that path alone rather than applying stale state.
        if (*reinterpret_cast<const uint8_t*>(base + guard::DeferredFlagRva))
        {
            ++deferredPasses;
            return result;
        }
        const auto pass = reinterpret_cast<const uint8_t*>(renderPass);
        if (*reinterpret_cast<const int32_t*>(pass + 0x938) == -1) return result;
        const auto slot = *reinterpret_cast<const uint32_t*>(pass + 0x940);
        if (slot < 1 || slot > 7) return result;
        const auto offset = slot * 0x110;
        const auto key = *reinterpret_cast<const uint32_t*>(base + guard::SlotKeyRva + offset);
        const auto kind = *reinterpret_cast<const uint32_t*>(base + guard::SlotKindRva + offset);
        const bool active = *reinterpret_cast<const uint8_t*>(base + guard::SlotActiveRva + offset) == 1;
        const auto car = CPlayer::findPlayerCar();
        if (!policy::OwnBeam(slot, kind, active, key, car)) return result;
        result.car = car;
        result.ownBeam = true;
        // Audited CE/official FusionFix: driver followed by eight passengers.
        std::memcpy(result.occupants.data(), reinterpret_cast<const void*>(car + 0xF50),
                    sizeof(result.occupants));
        ++ownPasses;
        return result;
    }

    static bool Exclude(uintptr_t entity, uint32_t type, bool artificial) noexcept
    {
        if (!enabled.load(std::memory_order_acquire)) return false;
        ++casterVisits;
        if (!bHeadlightShadows || !bVehicleNightShadows ||
            !policy::Exclude(context, entity, type, artificial)) return false;
        if (type == 2) ++carExcluded;
        else ++occupantsExcluded;
        return true;
    }
}

namespace ShadowDiagnostics
{
    static std::atomic<bool> ready{false};
    static std::filesystem::path path;
    static uint64_t lastWrite = 0;
    static bool guardPassed = false;
    static int casterMode = 0, allocationMode = 0;
    static std::string startupGuardDetails;
    static bool startupWritten = false;
    static bool admissionInstalled = false;

    static void Write() noexcept
    {
        if (!ready.load(std::memory_order_acquire)) return;
        const auto now = GetTickCount64();
        if (now - lastWrite < 5000) return;
        lastWrite = now;
        // Low-frequency game-event I/O, never inside submission/caster hooks.
        // Failure to write diagnostics must not escape into game code.
        try
        {
            std::ofstream log(path, std::ios::app);
            if (!startupWritten)
            {
                log << "candidate=18 startup_guard " << startupGuardDetails << '\n';
                log << "candidate=18 allocator_startup " << PlayerShadowAllocation::installStatus << '\n';
                if (log.good()) startupWritten = true;
            }
            log << "candidate=18 tick=" << now << " admission_installed=" << admissionInstalled
                << " caster_guard=" << guardPassed
                << " caster_requested=" << casterMode << " caster_enabled=" << OwnHeadlightCaster::enabled.load()
                << " allocation_mode=" << allocationMode << " allocation_ready=" << PlayerShadowAllocation::ready.load()
                << " allocation_thread_block=" << PlayerShadowAllocation::unsupportedThread.load()
                << " applied=" << PlayerShadowAllocation::appliedPasses.load()
                << " observed=" << PlayerShadowAllocation::observedPasses.load()
                << " fallback=" << PlayerShadowAllocation::fallbackPasses.load()
                << " passes=" << OwnHeadlightCaster::passes.load()
                << " deferred=" << OwnHeadlightCaster::deferredPasses.load()
                << " own_beam_passes=" << OwnHeadlightCaster::ownPasses.load()
                << " caster_visits=" << OwnHeadlightCaster::casterVisits.load()
                << " car_excluded=" << OwnHeadlightCaster::carExcluded.load()
                << " occupants_excluded=" << OwnHeadlightCaster::occupantsExcluded.load()
                << " driving_beam_yes=" << CShadows::drivingBeamAccepted.load()
                << " driving_beam_no=" << CShadows::drivingBeamRejected.load() << '\n';
        }
        catch (...) {}
    }
}
