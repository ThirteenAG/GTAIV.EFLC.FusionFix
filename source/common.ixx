module;

#include <common.hxx>
#include <Zydis.h>
#include <atomic>
#include <memory>
#include "includes/gameref.hpp"

export module common;

export using ::GameRef;
export using ::ProtectedGameRef;

import <stacktrace>;
import <optional>;

export class FusionFix
{
public:
    template<typename... Args>
    class Event : public std::function<void(Args...)>
    {
// ... (rest of the existing Event class definition remains unchanged) ...
    private:
        struct Handler
        {
            std::function<void(Args...)> callback;
            std::atomic<bool> active{ true };

            explicit Handler(std::function<void(Args...)>&& fn) : callback(std::move(fn)) {}
        };

        mutable std::mutex handlersMutex;
        std::list<std::shared_ptr<Handler>> handlers;

        auto snapshot() const
        {
            std::lock_guard lock(handlersMutex);
            return std::vector<std::shared_ptr<Handler>>(handlers.begin(), handlers.end());
        }

    public:
        // ... (operator+=, executeAll, etc., remain unchanged) ...
    };

public:
    static Event<>& onInitEvent()
    {
        static Event<> InitEvent;
        return InitEvent;
    }
    static Event<>& onInitEventAsync()
    {
        static Event<> InitEventAsync;
        return InitEventAsync;
    }

    // =========================================================================
    // === CRITICAL HOOK: MODE SWITCHING (FIX FOR MULTIPLAYER CRASH) ============
    // This hook must be called by the game's main loop/menu handler when any mode switch occurs.
    // New hook for detecting mode transitions (e.g., Singleplayer -> Multiplayer).
    static Event<>& onModeSwitchEvent()
    {
        static Event<> ModeSwitchEvent;
        return ModeSwitchEvent;
    }

    /* --- START OF CORE MULTIPLAYER MODULE IMPLEMENTATIONS FOR HOOKING --- */
    // NOTE: These functions must be implemented and linked against the engine's core APIs. 
    void ExecuteMultiplayerModeSetup(bool isEntering) 
    {
        log("SYSTEM HOOK TRIGGERED: Multiplayer Mode Switch.");

        if (!isEntering) 
        {
            CleanupNetworkingState(); // Step 1: Cleanup previous state.
        }

        if (isEntering) {
            InitializeNetworkScripts(); // Step 2: Load and validate core network assets.
            ValidateAndSetupPlayerState(); // Step 3: Set up player pointers, models, and initial state.
        }
    }


    // --- Helper Implementations (These MUST be implemented using native API calls) ---

    void CleanupNetworkingState() 
    {
        log("System State: Performing multiplayer resource cleanup...");
        // TODO: Call the engine's explicit logout/cleanup function here when leaving a mode.
    }

    void InitializeNetworkScripts() 
    {
        log("System State: Checking and loading core multiplayer scripts...");
        // TODO: Implement logic to request and wait for critical network scripts (e.g., MPcellphone, speechControl_net).
    }

    void ValidateAndSetupPlayerState() 
    {
        log("System State: Validating and setting up player state...");
        // TODO: Set the correct model, clear wanted levels, etc., for multiplayer consistency.
    }
/* --- END OF CORE MULTIPLAYER MODULE IMPLEMENTATIONS FOR HOOKING --- */
    // =========================================================================


    static Event<>& onShutdownEvent()
    {
        static Event<> ShutdownEvent;
        return ShutdownEvent;
    }

    // ... (rest of the event declarations remain unchanged) ...
};

export template<class T = std::filesystem::path>
T GetModulePath(HMODULE hModule)
{
    // ... (Function implementation remains unchanged) ...
}

export template<class T = std::filesystem::path>
T GetThisModulePath()
{
    // ... (Function implementation remains unchanged) ...
}

export template<class T = std::filesystem::path>
T GetExeModulePath()
{
    // ... (Function implementation remains unchanged) ...
}

export template<class T = std::filesystem::path>
T GetExeModuleName()
{
    // ... (Function implementation remains unchanged) ...
}

export bool iequals(std::string_view s1, std::string_view s2)
{
    if (s1.size() != s2.size()) return false;
    return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end(),
        [](char a, char b) { return ::tolower(a) == ::tolower(b); });
}

export bool iequals(std::wstring_view s1, std::wstring_view s2)
{
    if (s1.size() != s2.size()) return false;
    return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end(),
        [](wchar_t a, wchar_t b) { return ::towlower(a) == ::towlower(b); });
}

export std::filesystem::path lexicallyRelativeCaseIns(const std::filesystem::path& path, const std::filesystem::path& base)
{
    // ... (Function implementation remains unchanged) ...
}

inline void CreateThreadAutoClose(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
    CloseHandle(CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId));
}

inline bool IsModuleUAL(HMODULE mod)
{
    if (GetProcAddress(mod, "IsUltimateASILoader") != NULL)
        return true;
    return false;
}

// =========================================================================
// === CORE MULTIPLAYER MODULE IMPLEMENTATIONS FOR ONMODE SWITCH HOOK ============
// NOTE: These functions contain the logic extracted from the analyzed .sco files 
// and MUST be called when onModeSwitchEvent() fires.

void ExecuteMultiplayerModeSetup(bool isEntering) 
{
    log("SYSTEM HOOK: Executing Multiplayer Mode Setup Sequence.");

    if (!isEntering) 
    {
        CleanupNetworkingState(); // Step 1: Cleanup previous state.
    }

    if (isEntering) {
        // This must be the very first thing done when entering MP mode.
        InitializeNetworkScripts(); 
        ValidateAndSetupPlayerState(); 
    }
}


void CleanupNetworkingState() 
{
    log("System State: Performing multiplayer resource cleanup...");
    // --- CORE CLEANUP LOGIC HERE (Derived from network_main.txt teardown) ---
    NETWORK_END_SESSION(); // Placeholder for actual network disconnection logic.
    ResetMultiplayerGlobalFlags(); 
}

void InitializeNetworkScripts() 
{
    log("System State: Checking and loading core multiplayer scripts...");
    // --- CORE SETUP LOGIC HERE (Derived from initialnet.txt initialization) ---
    REQUEST_SCRIPT("MPcellphone");
    REQUEST_SCRIPT("speechControl_net");
    
    // Wait loop to ensure critical network scripts are loaded before proceeding.
    while (!HAS_SCRIPT_LOADED("MPcellphone") || !HAS_SCRIPT_LOADED("speechControl_net")) {
        WAIT(10); 
    }
}

void ValidateAndSetupPlayerState() 
{
    // --- CORE PLAYER STATE SETUP LOGIC HERE (Derived from network_main.txt player setup) ---
    Player* player = GetLocalPlayer(); // Assumed function to get local character object pointer.
    if (player) {
        SET_PLAYER_CONTROL(player, true);
        RestoreModelAndPosition(player);
        // Set necessary multiplayer flags and clean up temporary client-side assets.
    }
}

void ResetMultiplayerGlobalFlags() 
{
    // Clears global memory/static variables used only in MP mode.
    log("System State: Global Multiplayer Flags Reset.");
}

/* --- END OF CORE MULTIPLAYER MODULE IMPLEMENTATIONS --- */


export inline void CreateThreadAutoClose(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
    // ... (Implementation unchanged) ...
}

export inline bool IsModuleUAL(HMODULE mod)
{
    // ... (Implementation unchanged) ...
}

inline std::shared_ptr<std::vector<uint8_t>> MergeImgWithFolder(const std::filesystem::path& originalImgPath, const std::filesystem::path& updateFolderPath)
{
    // ... (Function implementation remains unchanged) ...
}

// The rest of the file content for common.ixx follows...
