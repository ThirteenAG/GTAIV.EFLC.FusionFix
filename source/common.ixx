module;

#include <common.hxx>
#include <Zydis.h>

export module common;

import <stacktrace>;
import <optional>;

export class FusionFix
{
public:
    template<typename... Args>
    class Event : public std::function<void(Args...)>
    {
    public:
        using std::function<void(Args...)>::function;

    private:
        std::list<std::function<void(Args...)>> handlers;

    public:
        auto operator+=(std::function<void(Args...)>&& handler) -> std::function<void()>
        {
            auto it = handlers.insert(handlers.end(), std::move(handler));
            return [this, it]() { handlers.erase(it); };
        }

        void executeAll(Args... args) const
        {
            if (!handlers.empty())
            {
                for (auto& handler : handlers)
                {
                    handler(args...);
                }
            }
        }

        std::reference_wrapper<std::vector<std::future<void>>> executeAllAsync(Args... args) const
        {
            static std::vector<std::future<void>> pendingFutures;
            if (!handlers.empty())
            {
                for (auto& handler : handlers)
                {
                    pendingFutures.emplace_back(std::async(std::launch::async, std::cref(handler), args...));
                }
            }
            return std::ref(pendingFutures);
        }
    };

public:
    static Event<>& onInitEvent() {
        static Event<> InitEvent;
        return InitEvent;
    }
    static Event<>& onInitEventAsync() {
        static Event<> InitEventAsync;
        return InitEventAsync;
    }
    static Event<>& onShutdownEvent() {
        static Event<> ShutdownEvent;
        return ShutdownEvent;
    }
    static Event<>& onGameInitEvent() {
        static Event<> GameInitEvent;
        return GameInitEvent;
    }
    static Event<>& onGameProcessEvent() {
        static Event<> GameProcessEvent;
        return GameProcessEvent;
    }
    static Event<>& onMenuDrawingEvent() {
        static Event<> MenuDrawingEvent;
        return MenuDrawingEvent;
    }
    static Event<>& onMenuEnterEvent() {
        static Event<> MenuEnterEvent;
        return MenuEnterEvent;
    }
    static Event<>& onMenuExitEvent() {
        static Event<> MenuExitEvent;
        return MenuExitEvent;
    }
    static Event<bool>& onActivateApp() {
        static Event<bool> ActivateApp;
        return ActivateApp;
    }
    static Event<>& onBeforeReset() {
        static Event<> BeforeReset;
        return BeforeReset;
    }
    static Event<>& onEndScene() {
        static Event<> EndScene;
        return EndScene;
    }
    static Event<>& onReadGameConfig()
    {
        static Event<> ReadGameConfig;
        return ReadGameConfig;
    }
    //static Event<>& onAfterReset() {
    //    static Event<> AfterReset;
    //    return AfterReset;
    //}
    //static Event<>& onBeforePostFX() {
    //    static Event<> BeforePostFX;
    //    return BeforePostFX;
    //}
    //static Event<>& onAfterPostFX() {
    //    static Event<> AfterPostFX;
    //    return AfterPostFX;
    //}

    struct D3D9 {
        [[deprecated]] static Event<LPDIRECT3D9&, UINT&, D3DDEVTYPE&, HWND&, DWORD&, D3DPRESENT_PARAMETERS*&, IDirect3DDevice9**&>& onBeforeCreateDevice() {
            static Event<LPDIRECT3D9&, UINT&, D3DDEVTYPE&, HWND&, DWORD&, D3DPRESENT_PARAMETERS*&, IDirect3DDevice9**&> BeforeCreateDevice;
            return BeforeCreateDevice;
        }
        [[deprecated]] static Event<LPDIRECT3D9&, UINT&, D3DDEVTYPE&, HWND&, DWORD&, D3DPRESENT_PARAMETERS*&, IDirect3DDevice9**&>& onAfterCreateDevice() {
            static Event<LPDIRECT3D9&, UINT&, D3DDEVTYPE&, HWND&, DWORD&, D3DPRESENT_PARAMETERS*&, IDirect3DDevice9**&> AfterCreateDevice;
            return AfterCreateDevice;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&>& onBeginScene() {
            static Event<LPDIRECT3DDEVICE9&> BeginScene;
            return BeginScene;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&>& onEndScene() {
            static Event<LPDIRECT3DDEVICE9&> EndScene;
            return EndScene;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, D3DPRESENT_PARAMETERS*&>& onBeforeReset() {
            static Event<LPDIRECT3DDEVICE9&, D3DPRESENT_PARAMETERS*&> BeforeReset;
            return BeforeReset;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, D3DPRESENT_PARAMETERS*&>& onAfterReset() {
            static Event<LPDIRECT3DDEVICE9&, D3DPRESENT_PARAMETERS*&> AfterReset;
            return AfterReset;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, IDirect3DPixelShader9*&>& onBeforeSetPixelShader() {
            static Event<LPDIRECT3DDEVICE9&, IDirect3DPixelShader9*&> BeforeSetPixelShader;
            return BeforeSetPixelShader;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, IDirect3DPixelShader9*&>& onAfterSetPixelShader() {
            static Event<LPDIRECT3DDEVICE9&, IDirect3DPixelShader9*&> AfterSetPixelShader;
            return AfterSetPixelShader;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, IDirect3DVertexShader9*&>& onBeforeSetVertexShader() {
            static Event<LPDIRECT3DDEVICE9&, IDirect3DVertexShader9*&> BeforeSetVertexShader;
            return BeforeSetVertexShader;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, IDirect3DVertexShader9*&>& onAfterSetVertexShader() {
            static Event<LPDIRECT3DDEVICE9&, IDirect3DVertexShader9*&> AfterSetVertexShader;
            return AfterSetVertexShader;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, UINT&, float*&, UINT&>& onSetVertexShaderConstantF() {
            static Event<LPDIRECT3DDEVICE9&, UINT&, float*&, UINT&> SetVertexShaderConstantF;
            return SetVertexShaderConstantF;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, UINT&, float*&, UINT&>& onSetPixelShaderConstantF() {
            static Event<LPDIRECT3DDEVICE9&, UINT&, float*&, UINT&> SetPixelShaderConstantF;
            return SetPixelShaderConstantF;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, DWORD*&, IDirect3DPixelShader9**&>& onBeforeCreatePixelShader() {
            static Event<LPDIRECT3DDEVICE9&, DWORD*&, IDirect3DPixelShader9**&> BeforeCreatePixelShader;
            return BeforeCreatePixelShader;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, DWORD*&, IDirect3DPixelShader9**&>& onAfterCreatePixelShader() {
            static Event<LPDIRECT3DDEVICE9&, DWORD*&, IDirect3DPixelShader9**&> AfterCreatePixelShader;
            return AfterCreatePixelShader;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, DWORD*&, IDirect3DVertexShader9**&>& onBeforeCreateVertexShader() {
            static Event<LPDIRECT3DDEVICE9&, DWORD*&, IDirect3DVertexShader9**&> BeforeCreateVertexShader;
            return BeforeCreateVertexShader;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, DWORD*&, IDirect3DVertexShader9**&>& onAfterCreateVertexShader() {
            static Event<LPDIRECT3DDEVICE9&, DWORD*&, IDirect3DVertexShader9**&> AfterCreateVertexShader;
            return AfterCreateVertexShader;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, UINT&, UINT&, UINT&, DWORD&, D3DFORMAT&, D3DPOOL&, IDirect3DTexture9**&, HANDLE*&>& onBeforeCreateTexture() {
            static Event<LPDIRECT3DDEVICE9&, UINT&, UINT&, UINT&, DWORD&, D3DFORMAT&, D3DPOOL&, IDirect3DTexture9**&, HANDLE*&> BeforeCreateTexture;
            return BeforeCreateTexture;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, UINT&, UINT&, UINT&, DWORD&, D3DFORMAT&, D3DPOOL&, IDirect3DTexture9**&, HANDLE*&>& onAfterCreateTexture() {
            static Event<LPDIRECT3DDEVICE9&, UINT&, UINT&, UINT&, DWORD&, D3DFORMAT&, D3DPOOL&, IDirect3DTexture9**&, HANDLE*&> AfterCreateTexture;
            return AfterCreateTexture;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, DWORD&, IDirect3DBaseTexture9*&>& onSetTexture() {
            static Event<LPDIRECT3DDEVICE9&, DWORD&, IDirect3DBaseTexture9*&> SetTexture;
            return SetTexture;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, D3DPRIMITIVETYPE&, UINT&, UINT&>& onBeforeDrawPrimitive() {
            static Event<LPDIRECT3DDEVICE9&, D3DPRIMITIVETYPE&, UINT&, UINT&> BeforeDrawPrimitive;
            return BeforeDrawPrimitive;
        }
        [[deprecated]] static bool& isInsteadDrawPrimitive() {
            static bool InsteadDrawPrimitive = false;
            return InsteadDrawPrimitive;
        }
        [[deprecated]] static void setInsteadDrawPrimitive(bool set) {
            isInsteadDrawPrimitive() = set;
        }
        [[deprecated]] static Event<LPDIRECT3DDEVICE9&, D3DPRIMITIVETYPE&, UINT&, UINT&>& onAfterDrawPrimitive() {
            static Event<LPDIRECT3DDEVICE9&, D3DPRIMITIVETYPE&, UINT&, UINT&> AfterDrawPrimitive;
            return AfterDrawPrimitive;
        }
    };
};

export template<class T = std::filesystem::path>
T GetModulePath(HMODULE hModule)
{
    static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
    static constexpr auto MAX_ITERATIONS = 7;

    if constexpr (std::is_same_v<T, std::filesystem::path>)
    {
        std::u16string ret;
        std::filesystem::path pathret;
        auto bufferSize = INITIAL_BUFFER_SIZE;
        for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
        {
            ret.resize(bufferSize);
            size_t charsReturned = 0;
            charsReturned = GetModuleFileNameW(hModule, (LPWSTR)&ret[0], bufferSize);
            if (charsReturned < ret.length())
            {
                ret.resize(charsReturned);
                pathret = ret;
                return pathret;
            }
            else
            {
                bufferSize *= 2;
            }
        }
    }
    else
    {
        T ret;
        auto bufferSize = INITIAL_BUFFER_SIZE;
        for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
        {
            ret.resize(bufferSize);
            size_t charsReturned = 0;
            if constexpr (std::is_same_v<T, std::string>)
                charsReturned = GetModuleFileNameA(hModule, &ret[0], bufferSize);
            else
                charsReturned = GetModuleFileNameW(hModule, &ret[0], bufferSize);
            if (charsReturned < ret.length())
            {
                ret.resize(charsReturned);
                return ret;
            }
            else
            {
                bufferSize *= 2;
            }
        }
    }
    return T();
}

export template<class T = std::filesystem::path>
T GetThisModulePath()
{
    HMODULE hm = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&FusionFix::onInitEvent, &hm);
    T r = GetModulePath<T>(hm);
    if constexpr (std::is_same_v<T, std::filesystem::path>)
        return r.parent_path();
    else if constexpr (std::is_same_v<T, std::string>)
        r = r.substr(0, r.find_last_of("/\\") + 1);
    else
        r = r.substr(0, r.find_last_of(L"/\\") + 1);
    return r;
}

export template<class T = std::filesystem::path>
T GetThisModuleName()
{
    HMODULE hm = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&FusionFix::onInitEvent, &hm);
    const T moduleFileName = GetModulePath<T>(hm);

    if constexpr (std::is_same_v<T, std::filesystem::path>)
        return moduleFileName.filename();
    else if constexpr (std::is_same_v<T, std::string>)
        return moduleFileName.substr(moduleFileName.find_last_of("/\\") + 1);
    else
        return moduleFileName.substr(moduleFileName.find_last_of(L"/\\") + 1);
}

export template<class T = std::filesystem::path>
T GetExeModulePath()
{
    T r = GetModulePath<T>(NULL);

    if constexpr (std::is_same_v<T, std::filesystem::path>)
        return r.parent_path();
    else if constexpr (std::is_same_v<T, std::string>)
        r = r.substr(0, r.find_last_of("/\\") + 1);
    else
        r = r.substr(0, r.find_last_of(L"/\\") + 1);
    return r;
}

export template<class T = std::filesystem::path>
T GetExeModuleName()
{
    const T moduleFileName = GetModulePath<T>(NULL);
    if constexpr (std::is_same_v<T, std::filesystem::path>)
        return moduleFileName.filename();
    else if constexpr (std::is_same_v<T, std::string>)
        return moduleFileName.substr(moduleFileName.find_last_of("/\\") + 1);
    else
        return moduleFileName.substr(moduleFileName.find_last_of(L"/\\") + 1);
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
    class input_iterator_range
    {
    public:
        input_iterator_range(const std::filesystem::path::const_iterator& first, const std::filesystem::path::const_iterator& last)
            : _first(first)
            , _last(last)
        {
        }
        std::filesystem::path::const_iterator begin() const
        {
            return _first;
        }
        std::filesystem::path::const_iterator end() const
        {
            return _last;
        }
    private:
        std::filesystem::path::const_iterator _first;
        std::filesystem::path::const_iterator _last;
    };

    if (!iequals(path.root_name().wstring(), base.root_name().wstring()) || path.is_absolute() != base.is_absolute() || (!path.has_root_directory() && base.has_root_directory()))
    {
        return std::filesystem::path();
    }

    std::filesystem::path::const_iterator a = path.begin(), b = base.begin();

    while (a != path.end() && b != base.end() && iequals(a->wstring(), b->wstring()))
    {
        ++a;
        ++b;
    }

    if (a == path.end() && b == base.end())
    {
        return std::filesystem::path(".");
    }

    int count = 0;

    for (const auto& element : input_iterator_range(b, base.end()))
    {
        if (element != "." && element != "" && element != "..")
        {
            ++count;
        }
        else if (element == "..")
        {
            --count;
        }
    }

    if (count < 0)
    {
        return std::filesystem::path();
    }

    std::filesystem::path result;
    for (int i = 0; i < count; ++i)
    {
        result /= "..";
    }

    for (const auto& element : input_iterator_range(a, path.end()))
    {
        result /= element;
    }

    return result;
};

export inline void CreateThreadAutoClose(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
    CloseHandle(CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId));
}

export inline bool IsModuleUAL(HMODULE mod)
{
    if (GetProcAddress(mod, "IsUltimateASILoader") != NULL)
        return true;
    return false;
}

export bool IsUALPresent() {
    for (const auto& entry : std::stacktrace::current()) {
        HMODULE hModule = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)entry.native_handle(), &hModule)) {
            if (IsModuleUAL(hModule))
                return true;
        }
    }
    return false;
}

export class CallbackHandler
{
public:
    static inline void RegisterCallback(std::function<void()>&& fn)
    {
        fn();
    }

    static inline void RegisterCallback(std::wstring_view module_name, std::function<void()>&& fn, bool bOnUnload = false)
    {
        if (!bOnUnload && (module_name.empty() || GetModuleHandleW(module_name.data()) != NULL))
        {
            fn();
        }
        else
        {
            RegisterDllNotification();
            if (!bOnUnload)
                GetOnModuleLoadCallbackList().emplace(module_name, std::forward<std::function<void()>>(fn));
            else
                GetOnModuleUnloadCallbackList().emplace(module_name, std::forward<std::function<void()>>(fn));
        }
    }

    static inline void RegisterCallback(std::function<void(HMODULE)>&& fn)
    {
        RegisterDllNotification();
        GetOnAnyModuleLoadCallbackList().emplace_back(std::forward<std::function<void(HMODULE)>>(fn));
    }

    static inline void RegisterCallback(std::function<void()>&& fn, bool bPatternNotFound, ptrdiff_t offset = 0x1100, uint32_t* ptr = nullptr)
    {
        if (!bPatternNotFound)
        {
            fn();
        }
        else
        {
            auto mh = GetModuleHandle(NULL);
            IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((DWORD)mh + ((IMAGE_DOS_HEADER*)mh)->e_lfanew);
            if (ptr == nullptr)
                ptr = (uint32_t*)((DWORD)mh + ntHeader->OptionalHeader.BaseOfCode + ntHeader->OptionalHeader.SizeOfCode - offset);
            std::thread([](std::function<void()>&& fn, uint32_t* ptr, uint32_t val)
                {
                    while (*ptr == val)
                        std::this_thread::yield();

                    fn();
                }, fn, ptr, *ptr).detach();
        }
    }

    static inline void RegisterCallback(std::function<void()>&& fn, hook::pattern pattern)
    {
        if (!pattern.empty())
        {
            fn();
        }
        else
        {
            auto* ptr = new ThreadParams{ fn, pattern };
            CreateThreadAutoClose(0, 0, (LPTHREAD_START_ROUTINE)&ThreadProc, (LPVOID)ptr, 0, NULL);
        }
    }

private:
    static inline void invokeOnModuleLoad(std::wstring_view module_name)
    {
        if (GetOnModuleLoadCallbackList().count(module_name.data()))
        {
            GetOnModuleLoadCallbackList().at(module_name.data())();
        }
    }

    static inline void invokeOnUnload(std::wstring_view module_name)
    {
        if (GetOnModuleUnloadCallbackList().count(module_name.data()))
        {
            GetOnModuleUnloadCallbackList().at(module_name.data())();
        }
    }

    static inline void invokeOnAnyModuleLoad(HMODULE mod)
    {
        if (!GetOnAnyModuleLoadCallbackList().empty())
        {
            for (auto& f : GetOnAnyModuleLoadCallbackList())
            {
                f(mod);
            }
        }
    }

    static inline void invokeOnAnyModuleUnload(HMODULE mod)
    {
        if (!GetOnAnyModuleUnloadCallbackList().empty())
        {
            for (auto& f : GetOnAnyModuleUnloadCallbackList())
            {
                f(mod);
            }
        }
    }

    static inline void InvokeAll()
    {
        for (auto&& fn : GetOnModuleLoadCallbackList())
            fn.second();
    }

private:
    struct Comparator
    {
        bool operator() (const std::wstring& s1, const std::wstring& s2) const
        {
            std::wstring str1(s1.length(), ' ');
            std::wstring str2(s2.length(), ' ');
            std::transform(s1.begin(), s1.end(), str1.begin(), tolower);
            std::transform(s2.begin(), s2.end(), str2.begin(), tolower);
            return  str1 < str2;
        }
    };

    static inline std::map<std::wstring, std::function<void()>, Comparator>& GetOnModuleLoadCallbackList()
    {
        static std::map<std::wstring, std::function<void()>, Comparator> onModuleLoad;
        return onModuleLoad;
    }

    static inline std::map<std::wstring, std::function<void()>, Comparator>& GetOnModuleUnloadCallbackList()
    {
        static std::map<std::wstring, std::function<void()>, Comparator> onModuleUnload;
        return onModuleUnload;
    }

    static inline std::vector<std::function<void(HMODULE)>>& GetOnAnyModuleLoadCallbackList()
    {
        return onAnyModuleLoad;
    }

    static inline std::vector<std::function<void(HMODULE)>>& GetOnAnyModuleUnloadCallbackList()
    {
        return onAnyModuleUnload;
    }

    struct ThreadParams
    {
        std::function<void()> fn;
        hook::pattern pattern;
    };

    typedef NTSTATUS(NTAPI* _LdrRegisterDllNotification) (ULONG, PVOID, PVOID, PVOID);
    typedef NTSTATUS(NTAPI* _LdrUnregisterDllNotification) (PVOID);

    typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA
    {
        ULONG Flags;                    //Reserved.
        PUNICODE_STRING FullDllName;    //The full path name of the DLL module.
        PUNICODE_STRING BaseDllName;    //The base file name of the DLL module.
        PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
        ULONG SizeOfImage;              //The size of the DLL image, in bytes.
    } LDR_DLL_LOADED_NOTIFICATION_DATA, LDR_DLL_UNLOADED_NOTIFICATION_DATA, * PLDR_DLL_LOADED_NOTIFICATION_DATA, * PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

    typedef union _LDR_DLL_NOTIFICATION_DATA
    {
        LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
        LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
    } LDR_DLL_NOTIFICATION_DATA, * PLDR_DLL_NOTIFICATION_DATA;

    typedef NTSTATUS(NTAPI* PLDR_MANIFEST_PROBER_ROUTINE)
        (
            IN HMODULE DllBase,
            IN PCWSTR FullDllPath,
            OUT PHANDLE ActivationContext
            );

    typedef NTSTATUS(NTAPI* PLDR_ACTX_LANGUAGE_ROURINE)
        (
            IN HANDLE Unk,
            IN USHORT LangID,
            OUT PHANDLE ActivationContext
            );

    typedef void(NTAPI* PLDR_RELEASE_ACT_ROUTINE)
        (
            IN HANDLE ActivationContext
            );

    typedef VOID(NTAPI* fnLdrSetDllManifestProber)
        (
            IN PLDR_MANIFEST_PROBER_ROUTINE ManifestProberRoutine,
            IN PLDR_ACTX_LANGUAGE_ROURINE CreateActCtxLanguageRoutine,
            IN PLDR_RELEASE_ACT_ROUTINE ReleaseActCtxRoutine
            );

private:
    static inline void CALLBACK LdrDllNotification(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
    {
        static constexpr auto LDR_DLL_NOTIFICATION_REASON_LOADED = 1;
        static constexpr auto LDR_DLL_NOTIFICATION_REASON_UNLOADED = 2;
        if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED)
        {
            invokeOnModuleLoad(NotificationData->Loaded.BaseDllName->Buffer);
            invokeOnAnyModuleLoad((HMODULE)NotificationData->Loaded.DllBase);
        }
        else if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_UNLOADED)
        {
            invokeOnUnload(NotificationData->Loaded.BaseDllName->Buffer);
            invokeOnAnyModuleUnload((HMODULE)NotificationData->Loaded.DllBase);
        }
    }

    static inline NTSTATUS NTAPI ProbeCallback(IN HMODULE DllBase, IN PCWSTR FullDllPath, OUT PHANDLE ActivationContext)
    {
        //wprintf(L"ProbeCallback: Base %p, path '%ls', context %p\r\n", DllBase, FullDllPath, *ActivationContext);

        std::wstring str(FullDllPath);
        invokeOnModuleLoad(str.substr(str.find_last_of(L"/\\") + 1));
        invokeOnAnyModuleLoad(DllBase);

        //if (!*ActivationContext)
        //    return STATUS_INVALID_PARAMETER; // breaks on xp

        HANDLE actx = NULL;
        ACTCTXW act = { 0 };

        act.cbSize = sizeof(act);
        act.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
        act.lpSource = FullDllPath;
        act.hModule = DllBase;
        act.lpResourceName = ISOLATIONAWARE_MANIFEST_RESOURCE_ID;

        // Reset pointer, crucial for x64 version
        *ActivationContext = 0;

        actx = CreateActCtxW(&act);

        // Report no manifest is present
        if (actx == INVALID_HANDLE_VALUE)
            return 0xC000008B; //STATUS_RESOURCE_NAME_NOT_FOUND;

        *ActivationContext = actx;

        return STATUS_SUCCESS;
    }

    static inline void RegisterDllNotification()
    {
        LdrRegisterDllNotification = (_LdrRegisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification");
        if (LdrRegisterDllNotification)
        {
            if (!cookie)
                LdrRegisterDllNotification(0, LdrDllNotification, 0, &cookie);
        }
        else
        {
            LdrSetDllManifestProber = (fnLdrSetDllManifestProber)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrSetDllManifestProber");
            if (LdrSetDllManifestProber)
            {
                LdrSetDllManifestProber(&ProbeCallback, NULL, &ReleaseActCtx);
            }
        }
    }

    static inline void UnRegisterDllNotification()
    {
        LdrUnregisterDllNotification = (_LdrUnregisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrUnregisterDllNotification");
        if (LdrUnregisterDllNotification && cookie)
            LdrUnregisterDllNotification(cookie);
    }

private:
    static inline DWORD WINAPI ThreadProc(LPVOID ptr)
    {
        auto paramsPtr = static_cast<CallbackHandler::ThreadParams*>(ptr);
        auto params = *paramsPtr;
        delete paramsPtr;

        HANDLE hTimer = NULL;
        LARGE_INTEGER liDueTime;
        liDueTime.QuadPart = -30 * 10000000LL;
        hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
        SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, 0);

        while (params.pattern.clear().empty())
        {
            Sleep(0);

            if (WaitForSingleObject(hTimer, 0) == WAIT_OBJECT_0)
            {
                CloseHandle(hTimer);
                return 0;
            }
        };

        params.fn();

        return 0;
    }
private:
    static inline _LdrRegisterDllNotification   LdrRegisterDllNotification;
    static inline _LdrUnregisterDllNotification LdrUnregisterDllNotification;
    static inline void* cookie;
    static inline fnLdrSetDllManifestProber     LdrSetDllManifestProber;
public:
    static inline std::once_flag flag;
    static inline std::vector<std::function<void(HMODULE)>> onAnyModuleLoad;
    static inline std::vector<std::function<void(HMODULE)>> onAnyModuleUnload;
};

export template <size_t count = 1, typename... Args>
hook::pattern find_pattern(Args... args)
{
    hook::pattern pattern;
    ((pattern = hook::pattern(args), !pattern.count_hint(count).empty()) || ...);
    return pattern;
}

std::string format(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::vector<char> v(1024);
    while (true)
    {
        va_list args2;
        va_copy(args2, args);
        int res = vsnprintf(v.data(), v.size(), fmt, args2);
        if ((res >= 0) && (res < static_cast<int>(v.size())))
        {
            va_end(args);
            va_end(args2);
            return std::string(v.data());
        }
        size_t size;
        if (res < 0)
            size = v.size() * 2;
        else
            size = static_cast<size_t>(res) + 1;
        v.clear();
        v.resize(size);
        va_end(args2);
    }
}

export template<typename T>
std::array<uint8_t, sizeof(T)> to_bytes(const T& object)
{
    std::array<uint8_t, sizeof(T)> bytes;
    const uint8_t* begin = reinterpret_cast<const uint8_t*>(std::addressof(object));
    const uint8_t* end = begin + sizeof(T);
    std::copy(begin, end, std::begin(bytes));
    return bytes;
}

export template<typename T>
T& from_bytes(const std::array<uint8_t, sizeof(T)>& bytes, T& object)
{
    static_assert(std::is_trivially_copyable<T>::value, "not a TriviallyCopyable type");
    uint8_t* begin_object = reinterpret_cast<uint8_t*>(std::addressof(object));
    std::copy(std::begin(bytes), std::end(bytes), begin_object);
    return object;
}

export template<class T, class T1>
T from_bytes(const T1& bytes)
{
    static_assert(std::is_trivially_copyable<T>::value, "not a TriviallyCopyable type");
    T object;
    uint8_t* begin_object = reinterpret_cast<uint8_t*>(std::addressof(object));
    std::copy(std::begin(bytes), std::end(bytes) - (sizeof(T1) - sizeof(T)), begin_object);
    return object;
}

export template <size_t n>
std::string pattern_str(const std::array<uint8_t, n> bytes)
{
    std::string result;
    for (size_t i = 0; i < n; i++)
    {
        result += format("%02X ", bytes[i]);
    }
    return result;
}

export template <typename T>
std::string pattern_str(T t)
{
    return std::string((std::is_same<T, char>::value ? format("%c ", t) : format("%02X ", t)));
}

export template <typename T, typename... Rest>
std::string pattern_str(T t, Rest... rest)
{
    return std::string((std::is_same<T, char>::value ? format("%c ", t) : format("%02X ", t)) + pattern_str(rest...));
}

export std::string pattern_str(std::string_view str) {
    std::stringstream str_stream;
    for (const auto& item : str) {
        str_stream << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << +uint8_t(item) << " ";
    }
    return str_stream.str();
}

export class IATHook
{
public:
    template <class... Ts>
    static auto Replace(HMODULE target_module, std::string_view dll_name, Ts&& ... inputs)
    {
        std::map<std::string, std::future<void*>> originalPtrs;

        const DWORD_PTR instance = reinterpret_cast<DWORD_PTR>(target_module);
        const PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(instance + reinterpret_cast<PIMAGE_DOS_HEADER>(instance)->e_lfanew);
        PIMAGE_IMPORT_DESCRIPTOR pImports = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(instance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        DWORD dwProtect[2];

        for (; pImports->Name != 0; pImports++)
        {
            if (_stricmp(reinterpret_cast<const char*>(instance + pImports->Name), dll_name.data()) == 0)
            {
                if (pImports->OriginalFirstThunk != 0)
                {
                    const PIMAGE_THUNK_DATA pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(instance + pImports->OriginalFirstThunk);

                    for (ptrdiff_t j = 0; pThunk[j].u1.AddressOfData != 0; j++)
                    {
                        auto pAddress = reinterpret_cast<void**>(instance + pImports->FirstThunk) + j;
                        if (!pAddress) continue;
                        VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                        ([&]
                        {
                            auto name = std::string_view(std::get<0>(inputs));
                            auto num = std::string("-1");
                            if (name.contains("@")) {
                                num = name.substr(name.find_last_of("@") + 1);
                                name = name.substr(0, name.find_last_of("@"));
                            }

                            if (pThunk[j].u1.Ordinal & IMAGE_ORDINAL_FLAG)
                            {
                                try
                                {
                                    if (IMAGE_ORDINAL(pThunk[j].u1.Ordinal) == std::stoi(num.data()))
                                    {
                                        originalPtrs[std::get<0>(inputs)] = std::async(std::launch::deferred, [&]() -> void* { return *pAddress; });
                                        originalPtrs[std::get<0>(inputs)].wait();
                                        *pAddress = std::get<1>(inputs);
                                    }
                                }
                                catch (...) {}
                            }
                            else if ((*pAddress && *pAddress == (void*)GetProcAddress(GetModuleHandleA(dll_name.data()), name.data())) ||
                            (strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, name.data()) == 0))
                            {
                                originalPtrs[std::get<0>(inputs)] = std::async(std::launch::deferred, [&]() -> void* { return *pAddress; });
                                originalPtrs[std::get<0>(inputs)].wait();
                                *pAddress = std::get<1>(inputs);
                            }
                        } (), ...);
                        VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                    }
                }
                else
                {
                    auto pFunctions = reinterpret_cast<void**>(instance + pImports->FirstThunk);

                    for (ptrdiff_t j = 0; pFunctions[j] != nullptr; j++)
                    {
                        auto pAddress = reinterpret_cast<void**>(pFunctions[j]);
                        VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                        ([&]
                        {
                            if (*pAddress && *pAddress == (void*)GetProcAddress(GetModuleHandleA(dll_name.data()), std::get<0>(inputs)))
                            {
                                originalPtrs[std::get<0>(inputs)] = std::async(std::launch::deferred, [&]() -> void* { return *pAddress; });
                                originalPtrs[std::get<0>(inputs)].wait();
                                *pAddress = std::get<1>(inputs);
                            }
                        } (), ...);
                        VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                    }
                }
            }
        }

        if (originalPtrs.empty())
        {
            PIMAGE_DELAYLOAD_DESCRIPTOR pDelayed = reinterpret_cast<PIMAGE_DELAYLOAD_DESCRIPTOR>(instance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].VirtualAddress);
            if (pDelayed)
            {
                for (; pDelayed->DllNameRVA != 0; pDelayed++)
                {
                    if (_stricmp(reinterpret_cast<const char*>(instance + pDelayed->DllNameRVA), dll_name.data()) == 0)
                    {
                        if (pDelayed->ImportAddressTableRVA != 0)
                        {
                            const PIMAGE_THUNK_DATA pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(instance + pDelayed->ImportNameTableRVA);
                            const PIMAGE_THUNK_DATA pFThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(instance + pDelayed->ImportAddressTableRVA);

                            for (ptrdiff_t j = 0; pThunk[j].u1.AddressOfData != 0; j++)
                            {
                                auto pAddress = reinterpret_cast<void**>(pFThunk[j].u1.Function);
                                if (!pAddress) continue;
                                if (pThunk[j].u1.Ordinal & IMAGE_ORDINAL_FLAG)
                                    pAddress = *reinterpret_cast<void***>(pFThunk[j].u1.Function + 1); // mov     eax, offset *

                                VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                ([&]
                                {
                                    auto name = std::string_view(std::get<0>(inputs));
                                    auto num = std::string("-1");
                                    if (name.contains("@")) {
                                        num = name.substr(name.find_last_of("@") + 1);
                                        name = name.substr(0, name.find_last_of("@"));
                                    }

                                    if (pThunk[j].u1.Ordinal & IMAGE_ORDINAL_FLAG)
                                    {
                                        try
                                        {
                                            if (IMAGE_ORDINAL(pThunk[j].u1.Ordinal) == std::stoi(num.data()))
                                            {
                                                originalPtrs[std::get<0>(inputs)] = std::async(std::launch::async,
                                                [](void** pAddress, void* value, PVOID instance) -> void*
                                                {
                                                    DWORD dwProtect[2];
                                                    VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                                    MEMORY_BASIC_INFORMATION mbi;
                                                    mbi.AllocationBase = instance;
                                                    do
                                                    {
                                                        VirtualQuery(*pAddress, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
                                                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                                    } while (mbi.AllocationBase == instance);
                                                    auto r = *pAddress;
                                                    *pAddress = value;
                                                    VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                                                    return r;
                                                }, pAddress, std::get<1>(inputs), (PVOID)instance);
                                            }
                                        }
                                        catch (...) {}
                                    }
                                    else if ((*pAddress && *pAddress == (void*)GetProcAddress(GetModuleHandleA(dll_name.data()), name.data())) ||
                                    (strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, name.data()) == 0))
                                    {
                                        originalPtrs[std::get<0>(inputs)] = std::async(std::launch::async,
                                        [](void** pAddress, void* value, PVOID instance) -> void*
                                        {
                                            DWORD dwProtect[2];
                                            VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                            MEMORY_BASIC_INFORMATION mbi;
                                            mbi.AllocationBase = instance;
                                            do
                                            {
                                                VirtualQuery(*pAddress, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
                                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                            } while (mbi.AllocationBase == instance);
                                            auto r = *pAddress;
                                            *pAddress = value;
                                            VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                                            return r;
                                        }, pAddress, std::get<1>(inputs), (PVOID)instance);
                                    }
                                } (), ...);
                                VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                            }
                        }
                    }
                }
            }
        }

        if (originalPtrs.empty()) // e.g. re5dx9.exe steam
        {
            static auto getSection = [](const PIMAGE_NT_HEADERS nt_headers, unsigned section) -> PIMAGE_SECTION_HEADER
            {
                return reinterpret_cast<PIMAGE_SECTION_HEADER>(
                    (UCHAR*)nt_headers->OptionalHeader.DataDirectory +
                    nt_headers->OptionalHeader.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY) +
                    section * sizeof(IMAGE_SECTION_HEADER));
            };

            for (auto i = 0; i < ntHeader->FileHeader.NumberOfSections; i++)
            {
                auto sec = getSection(ntHeader, i);
                auto pFunctions = reinterpret_cast<void**>(instance + std::max(sec->PointerToRawData, sec->VirtualAddress));

                for (ptrdiff_t j = 0; j < 300; j++)
                {
                    auto pAddress = reinterpret_cast<void**>(&pFunctions[j]);
                    VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                    ([&]
                    {
                        auto name = std::string_view(std::get<0>(inputs));
                        auto num = std::string("-1");
                        if (name.contains("@")) {
                            num = name.substr(name.find_last_of("@") + 1);
                            name = name.substr(0, name.find_last_of("@"));
                        }

                        if (*pAddress && *pAddress == (void*)GetProcAddress(GetModuleHandleA(dll_name.data()), name.data()))
                        {
                            originalPtrs[std::get<0>(inputs)] = std::async(std::launch::deferred, [&]() -> void* { return *pAddress; });
                            originalPtrs[std::get<0>(inputs)].wait();
                            *pAddress = std::get<1>(inputs);
                        }
                    } (), ...);
                    VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                }

                if (!originalPtrs.empty())
                    return originalPtrs;
            }
        }

        return originalPtrs;
    }
};

export class raw_mem
{
public:
    raw_mem(injector::memory_pointer_tr addr, std::initializer_list<uint8_t> bytes, bool offset_back = false)
    {
        ptr = addr.as_int() - (offset_back ? bytes.size() : 0);
        new_code.assign(std::move(bytes));
        old_code.resize(new_code.size());
        ReadMemoryRaw(ptr, old_code.data(), old_code.size(), true);
    }

    void Write()
    {
        WriteMemoryRaw(ptr, new_code.data(), new_code.size(), true);
    }

    void Restore()
    {
        WriteMemoryRaw(ptr, old_code.data(), old_code.size(), true);
    }

    size_t Size()
    {
        return old_code.size();
    }

private:
    injector::memory_pointer ptr;
    std::vector<uint8_t> old_code;
    std::vector<uint8_t> new_code;
};

export std::optional<uintptr_t> resolve_displacement(auto ip)
{
    ZydisDecoder decoder;
    #if defined(_M_X64) || defined(__x86_64__)
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    #else
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);
    #endif

    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    ZyanStatus status = ZydisDecoderDecodeFull(
        &decoder,
        (void*)ip,
        ZYDIS_MAX_INSTRUCTION_LENGTH,
        &instruction,
        operands
    );

    if (!ZYAN_SUCCESS(status))
    {
        return std::nullopt;
    }

    for (uint32_t i = 0; i < instruction.operand_count_visible; ++i)
    {
        const auto& operand = operands[i];

        if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY)
        {
            if (operand.mem.disp.has_displacement)
            {
                #if defined(_M_X64) || defined(__x86_64__)
                if (operand.mem.is_rip_relative)
                {
                    return (uintptr_t)ip + instruction.length + operand.mem.disp.value;
                }
                #else
                return static_cast<uintptr_t>(operand.mem.disp.value);
                #endif
            }
        }
        else if (operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
        {
            if (operand.imm.is_relative)
            {
                return (uintptr_t)ip + instruction.length + ZyanISize(operand.imm.value.s);
            }
        }
    }

    if (instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE && instruction.raw.disp.size > 0)
    {
        return (uintptr_t)ip + instruction.length + ZyanISize(instruction.raw.disp.value);
    }

    return std::nullopt;
}

export std::optional<uintptr_t> resolve_next_displacement(auto ip)
{
    ZydisDecoder decoder;
    #if defined(_M_X64) || defined(__x86_64__)
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    #else
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);
    #endif

    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    uintptr_t current_ip = (uintptr_t)ip;
    size_t instruction_count = 0;

    while (true)
    {
        ZyanStatus status = ZydisDecoderDecodeFull(
            &decoder,
            (void*)current_ip,
            ZYDIS_MAX_INSTRUCTION_LENGTH,
            &instruction,
            operands
        );

        if (!ZYAN_SUCCESS(status))
        {
            return std::nullopt;
        }

        if (instruction.meta.category == ZYDIS_CATEGORY_COND_BR)
        {
            for (uint32_t i = 0; i < instruction.operand_count_visible; ++i)
            {
                const auto& operand = operands[i];

                if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY)
                {
                    if (operand.mem.disp.has_displacement)
                    {
                        #if defined(_M_X64) || defined(__x86_64__)
                        if (operand.mem.is_rip_relative)
                        {
                            return current_ip + instruction.length + operand.mem.disp.value;
                        }
                        #else
                        return static_cast<uintptr_t>(operand.mem.disp.value);
                        #endif
                    }
                }
                else if (operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
                {
                    if (operand.imm.is_relative)
                    {
                        return current_ip + instruction.length + ZyanISize(operand.imm.value.s);
                    }
                }
            }

            if (instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE && instruction.raw.disp.size > 0)
            {
                return current_ip + instruction.length + ZyanISize(instruction.raw.disp.value);
            }

            return std::nullopt;
        }

        current_ip += instruction.length;

        constexpr size_t MAX_INSTRUCTIONS = 20;
        if (++instruction_count >= MAX_INSTRUCTIONS)
        {
            return std::nullopt;
        }
    }

    return std::nullopt;
}