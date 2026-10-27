#pragma once

// The graphics API of a Direct3D 9 game (or a Direct3D 8 game through d3d8to9), chosen by the plugin's "Graphics API"
// setting: 0 Direct3D 9 (the system's runtime), 1 Vulkan (DXVK as vulkan.dll next to the exe: the build FusionFixHub
// installs or the player's own, also a symbolic or hard link to it), 2 DirectX 12 (Fusion Fix's D3D9On12 driver,
// d3d9on12.dll and dxbcSigner.dll next to the exe). Install() points every module's d3d9.dll imports of
// Direct3DCreate9(Ex)/Direct3DCreate9On12(Ex) at this header's functions; a third-party d3d9.dll (ReShade, dxwrapper)
// stays on top, and the system's CreateDevice code is hooked so that it wraps a device of the chosen API. A crash guard
// (a marker file deleted on the second frame presented) falls back to Direct3D 9, sets the setting back and shows a
// notice. Needs safetyhook on the include path.
//
//   Install(options)       apply the API; from the plugin's init (outside the loader lock, before the game creates its
//                          Direct3D object), and again after loading modules that import d3d9.dll
//   Available(api)         what menus offer
//   State(), Running()     what was asked, what runs, and why
//   SetWindowedOnDriver, SetVulkanHdr, AnyDisplayHdr, HdrOutput, QueuedInterop: driver and HDR helpers

#include <windows.h>
#include <bcrypt.h>
#include <d3d9.h>
#include <d3d9on12.h>
#include <dxgi1_6.h>
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <safetyhook.hpp>
#include "D3D9On12Extensions.h"

namespace FusionFixGraphicsApi
{
    enum class Api : int
    {
        Direct3D9 = 0,
        Vulkan = 1,
        D3D9On12 = 2,       // DirectX 12
    };

    constexpr const wchar_t* DxvkFileName = L"vulkan.dll";
    constexpr const wchar_t* DriverFileName = L"d3d9on12.dll";
    constexpr const wchar_t* SignerFileName = L"dxbcSigner.dll";
    // The exports that tell Fusion Fix's own d3d9.dll files: the pass-through placeholder, and the old graphics-API
    // proxies of GTA IV's Fusion Fix (up to 5.1.2)
    constexpr const char* PassThroughExport = "FusionFixPassThroughD3D9";
    constexpr const char* OldProxyExport = "FusionFixGraphicsApiSwitch";

    // SHA-256 of the DXVK builds of upscalers.json (component "dxvk"); keep both in step. Only tells FusionFixHub's
    // build from the player's own: any Direct3D 9 implementation as vulkan.dll can be used (DxvkFile).
    constexpr const char* KnownDxvk[] =
    {
        "265888c31ca78dffa290c39cb7e50bfb02762590e41927906e46fb32f01497fa",     // DXVK 3.1.1, x32\d3d9.dll
    };

    inline const char* Name(Api api)
    {
        return api == Api::Vulkan ? "Vulkan" : api == Api::D3D9On12 ? "DirectX 12" : "DirectX 9";
    }

    // What a d3d9.dll the imports resolve to is
    enum class D3D9Module
    {
        System,         // the system's (System32/SysWOW64)
        PassThrough,    // Fusion Fix's pass-through placeholder
        OldProxy,       // an old Fusion Fix graphics-API proxy: bypassed
        Foreign,        // a third-party wrapper: stays on top
        Own,            // already this header's function
    };

    // What vulkan.dll next to the exe is
    enum class DxvkFile
    {
        Missing,        // not there (or a link to nothing)
        Known,          // a DXVK build FusionFixHub installs (KnownDxvk)
        Custom,         // another Direct3D 9 implementation for this process: the player's own DXVK build
        Unusable,       // not a Direct3D 9 implementation this process can load (DxvkInfo::why)
    };

    struct DxvkInfo
    {
        DxvkFile file = DxvkFile::Missing;
        std::string why;                                // Unusable: what the file is
        bool Usable() const { return file == DxvkFile::Known || file == DxvkFile::Custom; }
    };

    // Shown once, before the device is created: the API that failed, what happened, the text of the default notice
    struct Notice
    {
        Api failed = Api::Direct3D9;
        std::string reason;
        std::wstring text;
    };

    struct Callbacks
    {
        std::function<void(const Notice&)> notice;      // default: a message box with notice.text
        std::function<void(Api)> setApiSetting;         // stores the setting (ASI Settings Set, which saves); called with Direct3D9
        std::function<void(const char*)> log;           // one line, no newline
    };

    struct Options
    {
        Api api = Api::Direct3D9;                       // the player's choice (the stored value, not the one in effect)
        std::filesystem::path markerFolder;             // the folder of the settings file; empty: no crash guard
        std::wstring markerName;                        // e.g. L"GTAIV.EFLC.FusionFix"
        std::wstring product = L"Fusion Fix";           // the notice's title
        Callbacks callbacks;
    };

    struct Status
    {
        bool installed = false;
        Api requested = Api::Direct3D9;
        Api effective = Api::Direct3D9;                 // what this start runs on (changes when the API fails while starting)
        std::string why;                                // why effective isn't requested; empty when it is
        bool fellBack = false;                          // the crash guard set the setting back to Direct3D 9
        bool oldProxy = false;                          // an old Fusion Fix d3d9.dll proxy is loaded (bypassed)
        std::wstring wrapper;                           // a third-party d3d9.dll kept on top of the API
        DxvkFile dxvk = DxvkFile::Missing;              // vulkan.dll as the start found it (when Vulkan was asked for)
        uint32_t driverCaps = 0;                        // of the loaded driver (D3D9ON12_CAP_*), 0 before it's loaded
        bool driverLoaded = false;
    };

    inline std::filesystem::path GameRoot()
    {
        wchar_t path[MAX_PATH * 4] = {};
        GetModuleFileNameW(nullptr, path, DWORD(std::size(path)));
        return std::filesystem::path(path).parent_path();
    }

    inline std::filesystem::path DxvkPath()
    {
        return GameRoot() / DxvkFileName;
    }

    namespace detail
    {
        // bcrypt.dll from System32 by full path: game folders can hold a proxy of that name too
        inline std::string Sha256(const void* bytes, size_t size)
        {
            auto bcrypt = LoadLibraryExW(L"bcrypt.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!bcrypt)
                return {};
            using Open = NTSTATUS(WINAPI*)(BCRYPT_ALG_HANDLE*, LPCWSTR, LPCWSTR, ULONG);
            using Hash = NTSTATUS(WINAPI*)(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, PUCHAR, ULONG, PUCHAR, ULONG);
            using Close = NTSTATUS(WINAPI*)(BCRYPT_ALG_HANDLE, ULONG);
            auto open = reinterpret_cast<Open>(GetProcAddress(bcrypt, "BCryptOpenAlgorithmProvider"));
            auto hash = reinterpret_cast<Hash>(GetProcAddress(bcrypt, "BCryptHash"));
            auto close = reinterpret_cast<Close>(GetProcAddress(bcrypt, "BCryptCloseAlgorithmProvider"));
            std::string result;
            BCRYPT_ALG_HANDLE algorithm = nullptr;
            unsigned char digest[32] = {};
            if (open && hash && close && open(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0)
            {
                if (hash(algorithm, nullptr, 0, static_cast<PUCHAR>(const_cast<void*>(bytes)), ULONG(size), digest, sizeof(digest)) >= 0)
                {
                    static const char* hex = "0123456789abcdef";
                    for (auto b : digest)
                    {
                        result += hex[b >> 4];
                        result += hex[b & 15];
                    }
                }
                close(algorithm, 0);
            }
            FreeLibrary(bcrypt);
            return result;
        }

        inline std::string Sha256(const std::filesystem::path& file)
        {
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return {};
            std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            return Sha256(data.data(), data.size());
        }

        inline bool KnownDxvkHash(const std::string& hash)
        {
            for (auto known : KnownDxvk)
                if (hash == known)
                    return true;
            return false;
        }

        // Whether an image mapped as an image (a loaded module, or LOAD_LIBRARY_AS_IMAGE_RESOURCE) exports a name.
        // 32- and 64-bit images; everything is checked against SizeOfImage (the file may be anything).
        inline bool ImageExports(const void* image, const char* name)
        {
            auto base = static_cast<const uint8_t*>(image);
            if (!base)
                return false;
            auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096)
                return false;
            auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE)
                return false;
            DWORD sizeOfImage = 0;
            IMAGE_DATA_DIRECTORY directory{};
            if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
            {
                if (nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT)
                    return false;
                sizeOfImage = nt->OptionalHeader.SizeOfImage;
                directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            }
            else if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            {
                auto nt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt);
                if (nt64->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT)
                    return false;
                sizeOfImage = nt64->OptionalHeader.SizeOfImage;
                directory = nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            }
            else
                return false;
            auto inImage = [&](DWORD rva, size_t size) { return rva < sizeOfImage && size <= sizeOfImage - rva; };
            if (!directory.VirtualAddress || !inImage(directory.VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY)))
                return false;
            auto exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + directory.VirtualAddress);
            if (!inImage(exports->AddressOfNames, size_t(exports->NumberOfNames) * sizeof(DWORD)))
                return false;
            auto names = reinterpret_cast<const DWORD*>(base + exports->AddressOfNames);
            auto length = strlen(name) + 1;
            for (DWORD i = 0; i < exports->NumberOfNames; ++i)
            {
                if (inImage(names[i], length) && memcmp(base + names[i], name, length) == 0)
                    return true;
            }
            return false;
        }

        // The same for a file, without running it
        inline bool FileExports(const std::filesystem::path& file, const char* name)
        {
            auto module = LoadLibraryExW(file.c_str(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
            if (!module)
                return false;
            // The handle of a resource mapping has its low bits set
            auto base = reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(module) & ~uintptr_t(3));
            bool found = ImageExports(base, name);
            FreeLibrary(module);
            return found;
        }

        inline std::wstring ModulePath(HMODULE module)
        {
            wchar_t path[MAX_PATH * 4] = {};
            GetModuleFileNameW(module, path, DWORD(std::size(path)));
            return path;
        }

        // System32, or SysWOW64 for a 32-bit process on 64-bit Windows (where the system's DLLs are loaded from)
        inline bool InSystemDirectory(const std::wstring& path)
        {
            wchar_t folder[MAX_PATH] = {};
            for (int i = 0; i < 2; ++i)
            {
                UINT length = i == 0 ? GetSystemDirectoryW(folder, MAX_PATH) : GetSystemWow64DirectoryW(folder, MAX_PATH);
                if (length && length < MAX_PATH && path.size() > length && _wcsnicmp(path.c_str(), folder, length) == 0 &&
                    (path[length] == L'\\' || path[length] == L'/'))
                    return true;
            }
            return false;
        }

        // What a d3d9.dll is, from where it is and what it exports
        inline D3D9Module Classify(bool inSystemDirectory, bool passThrough, bool oldProxy)
        {
            if (inSystemDirectory)
                return D3D9Module::System;
            if (passThrough)
                return D3D9Module::PassThrough;
            if (oldProxy)
                return D3D9Module::OldProxy;
            return D3D9Module::Foreign;
        }

        inline HMODULE OwnModule()
        {
            HMODULE module = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&OwnModule), &module);
            return module;
        }

        inline D3D9Module Classify(HMODULE module)
        {
            if (!module)
                return D3D9Module::Foreign;
            if (module == OwnModule())
                return D3D9Module::Own;
            return Classify(InSystemDirectory(ModulePath(module)), ImageExports(module, PassThroughExport), ImageExports(module, OldProxyExport));
        }

        inline HMODULE ModuleOf(const void* address)
        {
            HMODULE module = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                static_cast<LPCWSTR>(address), &module);
            return module;
        }

        // A file's identity for the caches below: they look again when it changes (the hub installs while the game runs)
        struct FileStamp
        {
            bool exists = false;
            uintmax_t size = 0;
            std::filesystem::file_time_type time{};
            bool operator==(const FileStamp&) const = default;
        };

        inline FileStamp Stamp(const std::filesystem::path& file)
        {
            std::error_code ec;
            FileStamp stamp;
            if (!std::filesystem::is_regular_file(file, ec))
                return stamp;
            stamp.size = std::filesystem::file_size(file, ec);
            stamp.time = std::filesystem::last_write_time(file, ec);
            stamp.exists = !ec;
            return stamp;
        }

        // Whether an image mapped as an image is for this process's architecture
        inline bool ImageForThisProcess(const void* image)
        {
            auto base = static_cast<const uint8_t*>(image);
            auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096)
                return false;
            auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE)
                return false;
            return nt->OptionalHeader.Magic == (sizeof(void*) == 8 ? IMAGE_NT_OPTIONAL_HDR64_MAGIC : IMAGE_NT_OPTIONAL_HDR32_MAGIC);
        }

        // Whether a module's version resource names a company (CompanyName), read as data
        inline bool VersionNames(HMODULE module, const wchar_t* company)
        {
            auto resource = FindResourceW(module, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(16));    // RT_VERSION
            auto data = resource ? static_cast<const wchar_t*>(LockResource(LoadResource(module, resource))) : nullptr;
            if (!data)
                return false;
            std::wstring_view block(data, SizeofResource(module, resource) / sizeof(wchar_t));
            // The value follows its key (padded to 32 bits)
            auto key = block.find(L"CompanyName");
            auto value = key == std::wstring_view::npos ? key : block.find(company, key);
            return value != std::wstring_view::npos && value < key + 16;
        }

        // What a file meant as vulkan.dll is, read as data (none of its code runs). Links are followed: the link stays
        // where it is, its target is read.
        inline DxvkInfo InspectDxvk(const std::filesystem::path& file)
        {
            DxvkInfo info;
            if (!Stamp(file).exists)
                return info;
            info.file = DxvkFile::Unusable;
            auto module = LoadLibraryExW(file.c_str(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
            if (!module)
            {
                info.why = "vulkan.dll can't be read as a DLL (error " + std::to_string(GetLastError()) + ")";
                return info;
            }
            // The handle of a resource mapping has its low bits set
            auto base = reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(module) & ~uintptr_t(3));
            if (!ImageForThisProcess(base))
                info.why = sizeof(void*) == 8 ? "vulkan.dll isn't a 64-bit DLL (DXVK's x64 d3d9.dll)" : "vulkan.dll isn't a 32-bit DLL (DXVK's x32 d3d9.dll)";
            else if (ImageExports(base, PassThroughExport) || ImageExports(base, OldProxyExport))
                info.why = "vulkan.dll is a Fusion Fix d3d9.dll, not DXVK";
            else if (ImageExports(base, D3D9ON12_CAPABILITIES_EXPORT))
                info.why = "vulkan.dll is the D3D9On12 driver, not DXVK";
            else if (!ImageExports(base, "Direct3DCreate9"))
                info.why = "vulkan.dll has no Direct3DCreate9 (not a Direct3D 9 implementation)";
            else if (VersionNames(module, L"Microsoft"))
                info.why = "vulkan.dll is a copy of Windows' d3d9.dll, not DXVK";
            FreeLibrary(module);
            if (info.why.empty())
                info.file = KnownDxvkHash(Sha256(file)) ? DxvkFile::Known : DxvkFile::Custom;
            return info;
        }

        // -------------------------------------------------------------------------------------------------------------
        // Crash guard: the marker and the decision at the start (no side effects but the marker file and the callbacks)

        struct Marker
        {
            bool found = false;
            Api api = Api::Direct3D9;
            std::string reason;
        };

        inline std::filesystem::path MarkerPath(const std::filesystem::path& folder, const std::wstring& name)
        {
            if (folder.empty())
                return {};
            return folder / ((name.empty() ? std::wstring(L"FusionFix") : name) + L".graphics-api-start");
        }

        // "<api>\n<what happened>"
        inline void WriteMarker(const std::filesystem::path& marker, Api api, const std::string& reason)
        {
            if (marker.empty())
                return;
            std::ofstream out(marker, std::ios::binary | std::ios::trunc);
            out << static_cast<int>(api) << "\n" << reason;
        }

        inline Marker ReadMarker(const std::filesystem::path& marker)
        {
            Marker result;
            std::error_code ec;
            if (marker.empty() || !std::filesystem::exists(marker, ec))
                return result;
            std::ifstream in(marker, std::ios::binary);
            std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            result.found = true;
            auto newline = text.find('\n');
            auto first = text.substr(0, newline);
            result.api = first == "1" ? Api::Vulkan : first == "2" ? Api::D3D9On12 : Api::Direct3D9;
            result.reason = newline == std::string::npos ? std::string() : text.substr(newline + 1);
            if (result.reason.size() > 300)
                result.reason.resize(300);
            return result;
        }

        inline void RemoveMarker(const std::filesystem::path& marker)
        {
            std::error_code ec;
            if (!marker.empty())
                std::filesystem::remove(marker, ec);
        }

        struct Availability
        {
            bool vulkan = false;            // vulkan.dll is DXVK (FusionFixHub's build or the player's own)
            bool driver = false;            // Fusion Fix's d3d9on12.dll and dxbcSigner.dll are next to the exe
            bool on12Runtime = false;       // the system's d3d9.dll has Direct3DCreate9On12 (Windows 10 and later)
            std::string vulkanWhy;          // why vulkan.dll can't be used, when it's there
        };

        struct Decision
        {
            Api effective = Api::Direct3D9;
            std::string why;
            bool setBack = false;           // set the setting back to Direct3D 9 (the crash guard)
            bool notice = false;
            bool removeMarker = false;      // a marker was found: it's used up, or stale (another API was chosen since)
            std::string failure;            // what the marker said
        };

        inline Decision Decide(Api requested, const Marker& marker, const Availability& available)
        {
            Decision d;
            d.effective = requested;
            if (marker.found)
            {
                d.removeMarker = true;
                if (requested != Api::Direct3D9 && marker.api == requested)
                {
                    d.effective = Api::Direct3D9;
                    d.setBack = true;
                    d.notice = true;
                    d.failure = marker.reason;
                    d.why = std::string(Name(requested)) + " didn't start last time" + (marker.reason.empty() ? "" : " (" + marker.reason + ")");
                    return d;
                }
            }
            if (requested == Api::Vulkan && !available.vulkan)
            {
                d.effective = Api::Direct3D9;
                d.why = available.vulkanWhy.empty() ? "vulkan.dll (DXVK) next to the game is missing" : available.vulkanWhy;
            }
            else if (requested == Api::D3D9On12 && !available.on12Runtime)
            {
                d.effective = Api::Direct3D9;
                d.why = "this Windows has no Direct3D 9 on 12 (Windows 10 or later)";
            }
            else if (requested == Api::D3D9On12 && !available.driver)
            {
                d.effective = Api::Direct3D9;
                d.why = "D3D9On12 (d3d9on12.dll and dxbcSigner.dll next to the game) is missing";
            }
            return d;
        }

        inline std::wstring Widen(const std::string& text)
        {
            if (text.empty())
                return {};
            int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
            std::wstring out(size_t(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), length);
            return out;
        }

        inline std::string Narrow(const std::wstring& text)
        {
            if (text.empty())
                return {};
            int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
            std::string out(size_t(length), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), length, nullptr, nullptr);
            return out;
        }

        inline std::wstring NoticeText(Api failed, const std::string& reason, const std::wstring& wrapper, bool customDxvk = false)
        {
            std::wstring text = failed == Api::D3D9On12 ? L"DirectX 12 couldn't start" : L"Vulkan (DXVK) couldn't start";
            if (!reason.empty())
                text += L" (" + Widen(reason) + L")";
            text += L".\n\nThe game runs on DirectX 9 now, and Graphics API is set back to DirectX 9. ";
            if (failed == Api::D3D9On12)
                text += L"You can choose DirectX 12 again in the game's options or in FusionFixHub, for example after installing or updating D3D9On12 on the hub's Graphics enhancements page.";
            else if (customDxvk)
                text += L"vulkan.dll in the game folder is your own DXVK build. You can choose Vulkan again in the game's options or in FusionFixHub, for example after updating the graphics driver or that build.";
            else
                text += L"You can choose Vulkan again in the game's options or in FusionFixHub, for example after updating the graphics driver, or DXVK on the hub's Graphics enhancements page.";
            if (!wrapper.empty())
                text += L"\n\nThe game folder has a third-party d3d9.dll (" + wrapper + L"). If it doesn't work with " +
                    (failed == Api::D3D9On12 ? L"DirectX 12" : L"Vulkan") + L", remove it or keep DirectX 9.";
            return text;
        }

        // Third-party d3d9.dll files that can't stay on top of an API (tested): the reason, or null. driverCaps: of
        // the loaded driver (DirectX 12).
        inline const char* IncompatibleWrapper(HMODULE wrapper, Api api, uint32_t driverCaps)
        {
            // FUSIONFIX_WRAPPER_CHECK=0 in the environment: no check (testing)
            wchar_t env[4]{};
            if (GetEnvironmentVariableW(L"FUSIONFIX_WRAPPER_CHECK", env, DWORD(std::size(env))) && env[0] == L'0')
                return nullptr;
            // None known: ReShade 6.8 and dxwrapper work on all three APIs (on DirectX 12 ReShade's reset/teardown waits
            // for an event query, which drivers without EVENT_QUERY_FLUSH could leave pending).
            UNREFERENCED_PARAMETER(wrapper);
            UNREFERENCED_PARAMETER(api);
            UNREFERENCED_PARAMETER(driverCaps);
            return nullptr;
        }

        // -------------------------------------------------------------------------------------------------------------
        // The running state

        using Create9 = IDirect3D9* (WINAPI*)(UINT);
        using Create9Ex = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
        using Create9On12 = IDirect3D9* (WINAPI*)(UINT, D3D9ON12_ARGS*, UINT);
        using Create9On12Ex = HRESULT(WINAPI*)(UINT, D3D9ON12_ARGS*, UINT, IDirect3D9Ex**);
        using CreateDeviceFn = HRESULT(WINAPI*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
        using CreateDeviceExFn = HRESULT(WINAPI*)(IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**);
        using PresentFn = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
        using PresentExFn = HRESULT(WINAPI*)(IDirect3DDevice9Ex*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
        using ResetFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
        using ResetExFn = HRESULT(WINAPI*)(IDirect3DDevice9Ex*, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*);
        using SwapPresentFn = HRESULT(WINAPI*)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);

        inline bool InImage(const void* address, HMODULE module)
        {
            auto base = reinterpret_cast<const uint8_t*>(module);
            auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
            auto p = static_cast<const uint8_t*>(address);
            return p >= base && p < base + nt->OptionalHeader.SizeOfImage;
        }

        // One vtable slot hooked in up to four vtables (the system's runtime, DXVK), the original of each
        struct SlotHook
        {
            void** vtables[4]{};
            void* originals[4]{};

            void* Original(const void* object) const
            {
                auto vtable = *static_cast<void** const*>(object);
                for (size_t i = 0; i < std::size(vtables); ++i)
                    if (vtables[i] == vtable)
                        return originals[i];
                return nullptr;
            }

            void Install(void** vtable, size_t index, void* replacement)
            {
                if (!vtable || vtable[index] == replacement)
                    return;
                for (size_t i = 0; i < std::size(vtables); ++i)
                {
                    if (vtables[i] == vtable)
                        return;
                    if (vtables[i])
                        continue;
                    DWORD protection = 0;
                    if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_READWRITE, &protection))
                        return;
                    originals[i] = vtable[index];
                    vtables[i] = vtable;
                    vtable[index] = replacement;
                    VirtualProtect(&vtable[index], sizeof(void*), protection, &protection);
                    return;
                }
            }

            void Install(void* object, size_t index, void* replacement)
            {
                if (object)
                    Install(*static_cast<void***>(object), index, replacement);
            }
        };

        struct CodeHook { void* target = nullptr; safetyhook::InlineHook hook; };

        // A method of the devices and their swap chains (Present, Reset): DXVK's objects have constant vtables in its
        // module, hooked there. The system runtime's objects (also the driver's devices) get a heap vtable that the
        // runtime may rewrite (e.g. after a device's first Reset): their method is hooked once in the runtime's code
        // the entry points to. A wrapper's own vtable entry (not the runtime's code) is hooked in the vtable.
        struct MethodHook
        {
            SlotHook vtables;
            safetyhook::InlineHook code;

            void* Original(const void* object) const
            {
                if (auto original = vtables.Original(object))
                    return original;
                return code ? code.original<void*>() : nullptr;
            }

            void Install(void* object, size_t index, void* replacement, HMODULE system)
            {
                if (!object)
                    return;
                auto vtable = *static_cast<void***>(object);
                auto entry = vtable[index];
                if (system && entry != replacement && InImage(entry, system))
                {
                    if (!code)
                        code = safetyhook::create_inline(entry, replacement);
                    if (code)
                        return;
                }
                vtables.Install(vtable, index, replacement);
            }
        };
        struct SystemSlot { void** vtable = nullptr; size_t slot = 0; void* original = nullptr; };

        struct State
        {
            std::recursive_mutex mutex;
            Options options;
            Status status;
            HMODULE system = nullptr;           // System32's d3d9.dll
            HMODULE dxvk = nullptr;             // vulkan.dll
            HMODULE driver = nullptr;           // Fusion Fix's d3d9on12.dll, once loaded
            bool driverTried = false;
            Create9 systemCreate9 = nullptr;
            Create9Ex systemCreate9Ex = nullptr;
            Create9On12 systemCreate9On12 = nullptr;
            Create9On12Ex systemCreate9On12Ex = nullptr;
            std::filesystem::path marker;
            std::atomic<bool> guarding{ false };
            std::atomic<int> presents{ 0 };
            bool noticePending = false;
            Notice notice;
            std::atomic<bool> noticeShown{ false };
            bool systemPatched = false;
            IDirect3D9* dxvkObject = nullptr;   // DXVK's and the driver's objects for the system's, made on first use, kept
            IDirect3D9Ex* dxvkObjectEx = nullptr;
            IDirect3D9* on12Object = nullptr;
            IDirect3D9Ex* on12ObjectEx = nullptr;
            std::vector<const void*> on12Objects;   // every D3D9On12 object made here (they share the system's vtable)
            std::vector<HMODULE> loggedModules;     // Fusion Fix and third-party d3d9.dll files, logged once
            // The system's CreateDevice and CreateDeviceEx, hooked in their code (not in the vtable, where a wrapper's
            // hook may be): per vtable and slot the original code's trampoline
            std::vector<std::unique_ptr<CodeHook>> codeHooks;
            std::vector<SystemSlot> systemSlots;
            SlotHook dxvkCreateDevice, dxvkCreateDeviceEx;
            MethodHook present, presentEx, reset, resetEx, swapPresent;
            std::vector<const void*> devices;   // the game's devices made on the chosen API (AfterCreateDevice)
            std::function<bool()> windowedOnDriver;     // SetWindowedOnDriver
            std::function<bool()> vulkanHdr;            // SetVulkanHdr
            bool vulkanHdrInstance = false;             // DXVK's instance was created for HDR output
        };

        inline State& Get()
        {
            static State state;
            return state;
        }

        inline void Log(const char* format, ...)
        {
            auto& s = Get();
            if (!s.options.callbacks.log)
                return;
            char line[1024];
            va_list args;
            va_start(args, format);
            vsnprintf(line, sizeof(line), format, args);
            va_end(args);
            s.options.callbacks.log(line);
        }

        // By its full path: by name, the loader returns any module called d3d9.dll that is loaded already (the
        // d3d9.dll next to the exe: an old proxy, a wrapper)
        inline HMODULE SystemD3D9()
        {
            wchar_t path[MAX_PATH] = {};
            UINT length = GetSystemDirectoryW(path, MAX_PATH);
            if (!length || length + 10 >= MAX_PATH)
                return nullptr;
            wcscat_s(path, L"\\d3d9.dll");
            return LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        }

        inline void ShowNotice()
        {
            auto& s = Get();
            if (!s.noticePending || s.noticeShown.exchange(true))
                return;
            Log("Notice: %s", Narrow(s.notice.text).c_str());
            if (s.options.callbacks.notice)
                s.options.callbacks.notice(s.notice);
            else
                MessageBoxW(nullptr, s.notice.text.c_str(), s.options.product.c_str(), MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
        }

        inline void QueueNotice(Api failed, const std::string& reason)
        {
            auto& s = Get();
            s.noticePending = true;
            s.notice = { failed, reason, NoticeText(failed, reason, s.status.wrapper, s.status.dxvk == DxvkFile::Custom) };
        }

        inline void StopGuard()
        {
            auto& s = Get();
            s.guarding = false;
            RemoveMarker(s.marker);
        }

        // The API failed while starting: Direct3D 9 now and from now on, the notice before the device is created
        inline void FallBack(const std::string& why)
        {
            auto& s = Get();
            {
                std::lock_guard lock(s.mutex);
                if (s.status.effective == Api::Direct3D9)
                    return;
                auto failed = s.status.effective;
                Log("%s failed: %s; DirectX 9 from now on", Name(failed), why.c_str());
                StopGuard();
                s.status.effective = Api::Direct3D9;
                s.status.why = std::string(Name(failed)) + " failed while starting (" + why + ")";
                s.status.fellBack = true;
                if (s.options.callbacks.setApiSetting)
                    s.options.callbacks.setApiSetting(Api::Direct3D9);
                QueueNotice(failed, why);
            }
            ShowNotice();
        }

        // -------------------------------------------------------------------------------------------------------------
        // The driver

        // The driver answers its own ABI and the older ones: a driver older than this header answers 0 for this ABI
        inline uint32_t Capabilities(HMODULE driver)
        {
            auto capabilities = driver ? reinterpret_cast<PFN_D3D9On12Capabilities>(GetProcAddress(driver, D3D9ON12_CAPABILITIES_EXPORT)) : nullptr;
            if (!capabilities)
                return 0;
            for (uint32_t abi = D3D9ON12_CAPABILITIES_ABI; abi >= 1; --abi)
                if (auto caps = capabilities(abi))
                    return caps;
            return 0;
        }

        // Before Direct3DCreate9On12: Fusion Fix's own d3d9on12.dll next to the exe, told by its capability export,
        // loaded by its full path so the runtime's load by name finds it
        inline bool LoadDriver()
        {
            auto& s = Get();
            std::lock_guard lock(s.mutex);
            if (s.driverTried)
                return s.driver != nullptr;
            s.driverTried = true;
            auto path = GameRoot() / DriverFileName;
            std::error_code ec;
            if (!std::filesystem::exists(path, ec) || !std::filesystem::exists(GameRoot() / SignerFileName, ec))
            {
                Log("D3D9On12: d3d9on12.dll or dxbcSigner.dll isn't next to the game");
                return false;
            }
            if (auto other = GetModuleHandleW(DriverFileName))
            {
                auto loaded = ModulePath(other);
                if (!std::filesystem::equivalent(path, loaded, ec) || !GetProcAddress(other, D3D9ON12_CAPABILITIES_EXPORT))
                {
                    Log("D3D9On12: another d3d9on12.dll is already loaded (%s)", Narrow(loaded).c_str());
                    return false;
                }
                s.driver = other;
            }
            else
            {
                auto module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
                if (!module)
                {
                    Log("D3D9On12: %s couldn't be loaded (error %lu)", Narrow(path.wstring()).c_str(), GetLastError());
                    return false;
                }
                if (!GetProcAddress(module, D3D9ON12_CAPABILITIES_EXPORT))
                {
                    Log("D3D9On12: %s isn't Fusion Fix's driver (no capability export)", Narrow(path.wstring()).c_str());
                    FreeLibrary(module);
                    return false;
                }
                s.driver = module;
            }
            s.status.driverLoaded = true;
            s.status.driverCaps = Capabilities(s.driver);
            Log("D3D9On12: loaded %s (Fusion Fix's driver), capabilities 0x%X", Narrow(ModulePath(s.driver)).c_str(), s.status.driverCaps);
            return true;
        }

        // Whether an exclusive fullscreen device may be created: anywhere but on the driver, and there when it reports
        // EXCLUSIVE_FULLSCREEN (or FUSIONFIX_D3D9ON12_EXCLUSIVE=1) and the plugin wants no window (SetWindowedOnDriver)
        inline bool ExclusiveFullscreenAllowed()
        {
            auto& s = Get();
            if (s.status.effective != Api::D3D9On12 || !s.driver)
                return true;
            if (s.windowedOnDriver && s.windowedOnDriver())
                return false;
            static int allowed = -1;
            if (allowed >= 0)
                return allowed != 0;
            wchar_t env[8]{};
            if (GetEnvironmentVariableW(L"FUSIONFIX_D3D9ON12_EXCLUSIVE", env, DWORD(std::size(env))) && (env[0] == L'0' || env[0] == L'1'))
            {
                allowed = env[0] == L'1';
                Log("D3D9On12: exclusive fullscreen %s by FUSIONFIX_D3D9ON12_EXCLUSIVE", allowed ? "allowed" : "disallowed");
                return allowed != 0;
            }
            allowed = (Capabilities(s.driver) & D3D9ON12_CAP_EXCLUSIVE_FULLSCREEN) != 0;
            Log("D3D9On12: exclusive fullscreen %s by the driver's capabilities", allowed ? "allowed" : "disallowed (the device is created windowed)");
            return allowed != 0;
        }

        // On the driver without exclusive fullscreen: the device is windowed whatever the game asks for. True when changed.
        inline bool ForceWindowed(D3DPRESENT_PARAMETERS* parameters)
        {
            if (!parameters || parameters->Windowed || ExclusiveFullscreenAllowed())
                return false;
            parameters->Windowed = TRUE;
            parameters->FullScreen_RefreshRateInHz = 0;
            Log("D3D9On12: a fullscreen device was asked for, it's created windowed (the window covers the screen)");
            return true;
        }

        inline bool IsOn12(const void* object)
        {
            auto& s = Get();
            std::lock_guard lock(s.mutex);
            for (auto known : s.on12Objects)
                if (known == object)
                    return true;
            return false;
        }

        // -------------------------------------------------------------------------------------------------------------
        // Devices: the crash guard's presents, windowed on the driver

        inline void Presented(HRESULT hr)
        {
            auto& s = Get();
            if (s.guarding && SUCCEEDED(hr) && ++s.presents >= 2)
            {
                std::lock_guard lock(s.mutex);
                if (s.guarding)
                {
                    StopGuard();
                    Log("%s works: two frames presented", Name(s.status.effective));
                }
            }
        }

        inline HRESULT WINAPI Present(IDirect3DDevice9* device, const RECT* source, const RECT* dest, HWND window, const RGNDATA* dirty)
        {
            auto original = reinterpret_cast<PresentFn>(Get().present.Original(device));
            HRESULT hr = original ? original(device, source, dest, window, dirty) : E_FAIL;
            Presented(hr);
            return hr;
        }

        inline HRESULT WINAPI PresentEx(IDirect3DDevice9Ex* device, const RECT* source, const RECT* dest, HWND window, const RGNDATA* dirty, DWORD flags)
        {
            auto original = reinterpret_cast<PresentExFn>(Get().presentEx.Original(device));
            HRESULT hr = original ? original(device, source, dest, window, dirty, flags) : E_FAIL;
            Presented(hr);
            return hr;
        }

        inline HRESULT WINAPI SwapPresent(IDirect3DSwapChain9* chain, const RECT* source, const RECT* dest, HWND window, const RGNDATA* dirty, DWORD flags)
        {
            auto original = reinterpret_cast<SwapPresentFn>(Get().swapPresent.Original(chain));
            HRESULT hr = original ? original(chain, source, dest, window, dirty, flags) : E_FAIL;
            Presented(hr);
            return hr;
        }

        // A device the game made on the chosen API (the runtime's code hooks see every device of the process)
        inline bool OwnDevice(const void* device)
        {
            auto& s = Get();
            std::lock_guard lock(s.mutex);
            return std::find(s.devices.begin(), s.devices.end(), device) != s.devices.end();
        }

        inline HRESULT WINAPI Reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters)
        {
            if (OwnDevice(device))
                ForceWindowed(parameters);
            auto original = reinterpret_cast<ResetFn>(Get().reset.Original(device));
            HRESULT hr = original ? original(device, parameters) : E_FAIL;
            if (parameters)
                Log("Reset: %s %ux%u -> 0x%08lX", parameters->Windowed ? "windowed" : "fullscreen", parameters->BackBufferWidth, parameters->BackBufferHeight, static_cast<unsigned long>(hr));
            return hr;
        }

        inline HRESULT WINAPI ResetEx(IDirect3DDevice9Ex* device, D3DPRESENT_PARAMETERS* parameters, D3DDISPLAYMODEEX* mode)
        {
            if (OwnDevice(device) && ForceWindowed(parameters))
                mode = nullptr;
            auto original = reinterpret_cast<ResetExFn>(Get().resetEx.Original(device));
            return original ? original(device, parameters, mode) : E_FAIL;
        }

        inline void AfterCreateDevice(HRESULT hr, IDirect3DDevice9* device, bool extended, const D3DPRESENT_PARAMETERS* parameters)
        {
            auto& s = Get();
            if (parameters)
                Log("CreateDevice on %s: %s %ux%u %u Hz -> 0x%08lX", Name(s.status.effective), parameters->Windowed ? "windowed" : "fullscreen",
                    parameters->BackBufferWidth, parameters->BackBufferHeight, parameters->FullScreen_RefreshRateInHz, static_cast<unsigned long>(hr));
            if (SUCCEEDED(hr) && device)
            {
                std::lock_guard lock(s.mutex);
                if (std::find(s.devices.begin(), s.devices.end(), device) == s.devices.end())
                    s.devices.push_back(device);
                s.reset.Install(device, 16, reinterpret_cast<void*>(&Reset), s.system);
                s.present.Install(device, 17, reinterpret_cast<void*>(&Present), s.system);
                if (extended)
                {
                    s.presentEx.Install(device, 121, reinterpret_cast<void*>(&PresentEx), s.system);
                    s.resetEx.Install(device, 132, reinterpret_cast<void*>(&ResetEx), s.system);
                }
                IDirect3DSwapChain9* chain = nullptr;
                if (SUCCEEDED(device->GetSwapChain(0, &chain)) && chain)
                {
                    s.swapPresent.Install(chain, 3, reinterpret_cast<void*>(&SwapPresent), s.system);
                    chain->Release();
                }
                if (s.guarding)
                    WriteMarker(s.marker, s.status.effective, std::string("the game closed or crashed before its first frames on ") + Name(s.status.effective));
            }
            else if (FAILED(hr) && s.guarding)
            {
                // The game may retry with other parameters: the last call decides, the marker tells the next start
                char what[96];
                sprintf_s(what, "the device couldn't be created (0x%08lX)", static_cast<unsigned long>(hr));
                WriteMarker(s.marker, s.status.effective, what);
            }
        }

        inline bool Readable(const void* address, size_t size)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (!address || !VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
                return false;
            return static_cast<const uint8_t*>(address) + size <= static_cast<const uint8_t*>(info.BaseAddress) + info.RegionSize;
        }

        // The system's own code of a vtable slot of its objects: the slot's value, or, when a wrapper hooked the slot
        // (ReShade does), the value in the system's d3d9.dll on disk (mapped without running it, rebased)
        inline void* SystemSlotCode(void** vtable, size_t slot)
        {
            auto& s = Get();
            auto current = vtable[slot];
            if (InImage(current, s.system))
                return current;
            // The file as it is on disk: its pointers are based on its preferred base, never relocated
            std::ifstream in(ModulePath(s.system), std::ios::binary);
            std::string file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            auto base = reinterpret_cast<const uint8_t*>(s.system);
            auto rva = size_t(reinterpret_cast<const uint8_t*>(&vtable[slot]) - base);
            void* code = nullptr;
            auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
            if (file.size() > sizeof(IMAGE_DOS_HEADER) && dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0 &&
                size_t(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS) <= file.size())
            {
                auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(file.data() + dos->e_lfanew);
                auto sections = IMAGE_FIRST_SECTION(nt);
                for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
                {
                    auto& section = sections[i];
                    if (reinterpret_cast<const char*>(&section + 1) > file.data() + file.size())
                        break;
                    if (rva < section.VirtualAddress || rva + sizeof(void*) > size_t(section.VirtualAddress) + section.SizeOfRawData)
                        continue;
                    auto offset = size_t(section.PointerToRawData) + (rva - section.VirtualAddress);
                    if (offset + sizeof(uintptr_t) <= file.size())
                    {
                        auto value = *reinterpret_cast<const uintptr_t*>(file.data() + offset);
                        code = reinterpret_cast<void*>(value - uintptr_t(nt->OptionalHeader.ImageBase) + reinterpret_cast<uintptr_t>(base));
                    }
                    break;
                }
            }
            if (!code || !InImage(code, s.system))
            {
                Log("System objects: slot %u is hooked by %s, the system's code wasn't found (%p)", unsigned(slot), Narrow(ModulePath(ModuleOf(current))).c_str(), code);
                return nullptr;
            }
            Log("System objects: slot %u is hooked by %s, the system's code is hooked under it", unsigned(slot), Narrow(ModulePath(ModuleOf(current))).c_str());
            return code;
        }

        // Hooks the code of a slot of a system vtable (once per code), remembers its trampoline for the vtable
        inline void HookSystemSlot(void** vtable, size_t slot, void* replacement)
        {
            auto& s = Get();
            std::lock_guard lock(s.mutex);
            for (auto& known : s.systemSlots)
                if (known.vtable == vtable && known.slot == slot)
                    return;
            auto code = SystemSlotCode(vtable, slot);
            if (!code)
                return;
            CodeHook* hook = nullptr;
            for (auto& known : s.codeHooks)
                if (known->target == code)
                    hook = known.get();
            if (!hook)
            {
                auto created = std::make_unique<CodeHook>();
                created->target = code;
                created->hook = safetyhook::create_inline(code, replacement);
                if (!created->hook)
                {
                    Log("System objects: the code of slot %u couldn't be hooked", unsigned(slot));
                    return;
                }
                hook = created.get();
                s.codeHooks.push_back(std::move(created));
            }
            s.systemSlots.push_back({ vtable, slot, hook->hook.original<void*>() });
        }

        // The system's own code for an object's slot (the trampoline of the hook)
        inline void* SystemOriginal(const void* object, size_t slot)
        {
            auto& s = Get();
            auto vtable = *static_cast<void** const*>(object);
            {
                std::lock_guard lock(s.mutex);
                for (auto& known : s.systemSlots)
                    if (known.vtable == vtable && known.slot == slot)
                        return known.original;
                // Another vtable whose slot runs the same code: the same trampoline
                auto code = SystemSlotCode(vtable, slot);
                for (auto& known : s.codeHooks)
                    if (known->target == code)
                        return known->hook.original<void*>();
            }
            return nullptr;
        }

        // -------------------------------------------------------------------------------------------------------------
        // DXVK's objects: CreateDevice for the guard

        inline HRESULT WINAPI DxvkCreateDevice(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags,
            D3DPRESENT_PARAMETERS* parameters, IDirect3DDevice9** device)
        {
            auto original = reinterpret_cast<CreateDeviceFn>(Get().dxvkCreateDevice.Original(self));
            HRESULT hr = original ? original(self, adapter, type, window, flags, parameters, device) : E_FAIL;
            AfterCreateDevice(hr, device ? *device : nullptr, false, parameters);
            return hr;
        }

        inline HRESULT WINAPI DxvkCreateDeviceEx(IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags,
            D3DPRESENT_PARAMETERS* parameters, D3DDISPLAYMODEEX* mode, IDirect3DDevice9Ex** device)
        {
            auto original = reinterpret_cast<CreateDeviceExFn>(Get().dxvkCreateDeviceEx.Original(self));
            HRESULT hr = original ? original(self, adapter, type, window, flags, parameters, mode, device) : E_FAIL;
            AfterCreateDevice(hr, device ? *device : nullptr, true, parameters);
            return hr;
        }

        inline void HookDxvkObject(IDirect3D9* d3d)
        {
            auto& s = Get();
            std::lock_guard lock(s.mutex);
            s.dxvkCreateDevice.Install(d3d, 16, reinterpret_cast<void*>(&DxvkCreateDevice));
            s.dxvkCreateDeviceEx.Install(d3d, 20, reinterpret_cast<void*>(&DxvkCreateDeviceEx));
        }

        // DXVK's Vulkan instance, as it is created (vulkan-1.dll's vkCreateInstance, hooked while DXVK runs):
        // - HDR output. NVIDIA's driver composes the swap chains of an instance whose engine is named "DXVK" as SDR
        //   whatever their colour space (an EXTENDED_SRGB_LINEAR swap chain is clipped at scRGB 1.0); under any other
        //   engine name they go out in HDR. While the plugin wants HDR (SetVulkanHdr, asked when the instance is
        //   created) the engine is named VulkanHdrEngineName.
        // - VK_EXT_swapchain_colorspace, the instance extension for those colour spaces: DXVK 3.1.1 asks for it among
        //   the device extensions, its instance never has it. Added to an instance that presents (VK_KHR_surface)
        //   when the loader offers it.
        struct VkAppInfo
        {
            int32_t sType;
            const void* pNext;
            const char* pApplicationName;
            uint32_t applicationVersion;
            const char* pEngineName;
            uint32_t engineVersion;
            uint32_t apiVersion;
        };
        struct VkInstanceInfo
        {
            int32_t sType;
            const void* pNext;
            uint32_t flags;
            const VkAppInfo* pApplicationInfo;
            uint32_t enabledLayerCount;
            const char* const* ppEnabledLayerNames;
            uint32_t enabledExtensionCount;
            const char* const* ppEnabledExtensionNames;
        };
        constexpr const char* SwapchainColorSpaceExtension = "VK_EXT_swapchain_colorspace";
        constexpr const char* VulkanHdrEngineName = "DXVK (Fusion Fix HDR)";

        inline safetyhook::InlineHook& VkCreateInstanceHook()
        {
            static safetyhook::InlineHook hook;
            return hook;
        }

        inline bool& ColorSpaceExtensionOffered()
        {
            static bool offered = false;
            return offered;
        }

        // The extension list of an instance, with the colour space extension added when it presents and lacks it
        inline bool AddColorSpaceExtension(const VkInstanceInfo& info, std::vector<const char*>& names)
        {
            if (!ColorSpaceExtensionOffered())
                return false;
            bool surface = false;
            for (uint32_t i = 0; i < info.enabledExtensionCount; ++i)
            {
                auto name = info.ppEnabledExtensionNames[i];
                if (!name)
                    continue;
                if (strcmp(name, SwapchainColorSpaceExtension) == 0)
                    return false;
                if (strcmp(name, "VK_KHR_surface") == 0)
                    surface = true;
            }
            if (!surface)
                return false;
            names.assign(info.ppEnabledExtensionNames, info.ppEnabledExtensionNames + info.enabledExtensionCount);
            names.push_back(SwapchainColorSpaceExtension);
            return true;
        }

        inline int32_t __stdcall VkCreateInstance(const VkInstanceInfo* info, const void* allocator, void* instance)
        {
            auto& s = Get();
            if (!info)
                return VkCreateInstanceHook().stdcall<int32_t>(info, allocator, instance);
            auto copy = *info;
            std::vector<const char*> names;
            bool extension = AddColorSpaceExtension(*info, names);
            if (extension)
            {
                copy.enabledExtensionCount = uint32_t(names.size());
                copy.ppEnabledExtensionNames = names.data();
            }
            VkAppInfo app{};
            bool renamed = false;
            if (info->pApplicationInfo && info->pApplicationInfo->pEngineName && strcmp(info->pApplicationInfo->pEngineName, "DXVK") == 0 &&
                s.vulkanHdr && s.vulkanHdr())
            {
                app = *info->pApplicationInfo;
                app.pEngineName = VulkanHdrEngineName;
                copy.pApplicationInfo = &app;
                renamed = true;
            }
            if (!extension && !renamed)
                return VkCreateInstanceHook().stdcall<int32_t>(info, allocator, instance);
            int32_t result = VkCreateInstanceHook().stdcall<int32_t>(&copy, allocator, instance);
            Log("Vulkan: instance created%s%s (%d)", extension ? " with VK_EXT_swapchain_colorspace" : "",
                renamed ? ", engine named for HDR output" : "", result);
            if (result == 0)
            {
                s.vulkanHdrInstance = s.vulkanHdrInstance || renamed;
                return result;
            }
            return VkCreateInstanceHook().stdcall<int32_t>(info, allocator, instance);
        }

        // Once, before DXVK creates its instance
        inline void HookVulkanLoader()
        {
            static bool done = false;
            if (done)
                return;
            done = true;
            // The module DXVK loads by this name
            auto loader = LoadLibraryW(L"vulkan-1.dll");
            if (!loader)
                return;
            struct Extension { char name[256]; uint32_t specVersion; };
            using EnumerateFn = int32_t(__stdcall*)(const char*, uint32_t*, Extension*);
            auto enumerate = reinterpret_cast<EnumerateFn>(GetProcAddress(loader, "vkEnumerateInstanceExtensionProperties"));
            auto create = GetProcAddress(loader, "vkCreateInstance");
            if (!create)
                return;
            uint32_t count = 0;
            if (enumerate && enumerate(nullptr, &count, nullptr) == 0 && count)
            {
                std::vector<Extension> extensions(count);
                if (enumerate(nullptr, &count, extensions.data()) >= 0)
                    for (uint32_t i = 0; i < count && i < extensions.size(); ++i)
                        ColorSpaceExtensionOffered() = ColorSpaceExtensionOffered() || strcmp(extensions[i].name, SwapchainColorSpaceExtension) == 0;
            }
            VkCreateInstanceHook() = safetyhook::create_inline(reinterpret_cast<void*>(create), reinterpret_cast<void*>(&VkCreateInstance));
            Log("Vulkan: vkCreateInstance %s", VkCreateInstanceHook() ? "hooked" : "couldn't be hooked");
        }
        inline IDirect3D9* CreateDxvk(UINT sdk)
        {
            auto& s = Get();
            auto create = s.dxvk ? reinterpret_cast<Create9>(GetProcAddress(s.dxvk, "Direct3DCreate9")) : nullptr;
            auto d3d = create ? create(sdk) : nullptr;
            if (d3d)
                HookDxvkObject(d3d);
            return d3d;
        }

        inline HRESULT CreateDxvkEx(UINT sdk, IDirect3D9Ex** out)
        {
            auto& s = Get();
            auto create = s.dxvk ? reinterpret_cast<Create9Ex>(GetProcAddress(s.dxvk, "Direct3DCreate9Ex")) : nullptr;
            HRESULT hr = create ? create(sdk, out) : E_NOTIMPL;
            if (SUCCEEDED(hr) && out && *out)
                HookDxvkObject(*out);
            return hr;
        }

        // -------------------------------------------------------------------------------------------------------------
        // The driver's objects

        inline HRESULT WINAPI SystemCreateDevice(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
        inline HRESULT WINAPI SystemCreateDeviceEx(IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**);

        inline void RememberOn12(IDirect3D9* d3d)
        {
            auto& s = Get();
            std::lock_guard lock(s.mutex);
            if (!IsOn12(d3d))
                s.on12Objects.push_back(d3d);
            // Their vtable is the system's: hooked already, unless the runtime gave them their own
            HookSystemSlot(*reinterpret_cast<void***>(d3d), 16, reinterpret_cast<void*>(&SystemCreateDevice));
            HookSystemSlot(*reinterpret_cast<void***>(d3d), 20, reinterpret_cast<void*>(&SystemCreateDeviceEx));
        }

        // The runtime must have loaded the driver Install() loaded, never Windows' d3d9on12.dll (too slow)
        inline bool RuntimeUsesDriver()
        {
            auto& s = Get();
            auto loaded = GetModuleHandleW(DriverFileName);
            if (loaded && loaded == s.driver)
                return true;
            Log("D3D9On12: the runtime uses %s, not Fusion Fix's driver", loaded ? Narrow(ModulePath(loaded)).c_str() : "no d3d9on12.dll");
            return false;
        }

        inline IDirect3D9* CreateOn12(UINT sdk)
        {
            auto& s = Get();
            if (!LoadDriver())
            {
                FallBack("D3D9On12 (d3d9on12.dll next to the game) couldn't be loaded");
                return nullptr;
            }
            D3D9ON12_ARGS args{};
            args.Enable9On12 = TRUE;
            auto d3d = s.systemCreate9On12 ? s.systemCreate9On12(sdk, &args, 1) : nullptr;
            if (d3d && !RuntimeUsesDriver())
            {
                d3d->Release();
                d3d = nullptr;
            }
            if (!d3d)
            {
                FallBack("Direct3DCreate9On12 failed");
                return nullptr;
            }
            RememberOn12(d3d);
            return d3d;
        }

        inline HRESULT CreateOn12Ex(UINT sdk, IDirect3D9Ex** out)
        {
            auto& s = Get();
            if (!LoadDriver())
            {
                FallBack("D3D9On12 (d3d9on12.dll next to the game) couldn't be loaded");
                return E_FAIL;
            }
            D3D9ON12_ARGS args{};
            args.Enable9On12 = TRUE;
            HRESULT hr = s.systemCreate9On12Ex ? s.systemCreate9On12Ex(sdk, &args, 1, out) : E_NOTIMPL;
            if (SUCCEEDED(hr) && out && *out && !RuntimeUsesDriver())
            {
                (*out)->Release();
                *out = nullptr;
                hr = E_FAIL;
            }
            if (FAILED(hr) || !out || !*out)
            {
                char why[64];
                sprintf_s(why, "Direct3DCreate9On12Ex failed (0x%08lX)", static_cast<unsigned long>(hr));
                FallBack(why);
                return FAILED(hr) ? hr : E_FAIL;
            }
            RememberOn12(*out);
            return hr;
        }

        // The objects the system's objects create their devices on (wrappers on top, modules that took the system's
        // Direct3DCreate9): DXVK's for Vulkan, the driver's for DirectX 12. Made on first use, kept.
        inline IDirect3D9* BackendObject()
        {
            auto& s = Get();
            std::lock_guard lock(s.mutex);
            if (s.status.effective == Api::Vulkan && !s.dxvkObject)
                s.dxvkObject = CreateDxvk(D3D_SDK_VERSION);
            else if (s.status.effective == Api::D3D9On12 && !s.on12Object)
                s.on12Object = CreateOn12(D3D_SDK_VERSION);
            return s.status.effective == Api::Vulkan ? s.dxvkObject : s.status.effective == Api::D3D9On12 ? s.on12Object : nullptr;
        }

        inline IDirect3D9Ex* BackendObjectEx()
        {
            auto& s = Get();
            std::lock_guard lock(s.mutex);
            if (s.status.effective == Api::Vulkan && !s.dxvkObjectEx && FAILED(CreateDxvkEx(D3D_SDK_VERSION, &s.dxvkObjectEx)))
                s.dxvkObjectEx = nullptr;
            else if (s.status.effective == Api::D3D9On12 && !s.on12ObjectEx && FAILED(CreateOn12Ex(D3D_SDK_VERSION, &s.on12ObjectEx)))
                s.on12ObjectEx = nullptr;
            return s.status.effective == Api::Vulkan ? s.dxvkObjectEx : s.status.effective == Api::D3D9On12 ? s.on12ObjectEx : nullptr;
        }

        // IDirect3D9::CreateDevice of the system's runtime, hooked in its code (its objects and the driver's run the
        // same code): a system object creates the device on the backend's object; the driver's object creates it
        // itself (windowed when needed, with the guard). A wrapper's vtable hook runs before this and wraps the result.
        inline HRESULT WINAPI SystemCreateDevice(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags,
            D3DPRESENT_PARAMETERS* parameters, IDirect3DDevice9** device)
        {
            auto& s = Get();
            auto original = reinterpret_cast<CreateDeviceFn>(SystemOriginal(self, 16));
            if (!original)
                return E_FAIL;
            ShowNotice();
            if (s.status.effective != Api::Direct3D9 && !IsOn12(self))
            {
                Log("CreateDevice of a system object (a wrapper on top, or the system's Direct3DCreate9): on %s", Name(s.status.effective));
                if (auto backend = BackendObject())
                {
                    if (s.status.effective == Api::Vulkan)
                        return backend->CreateDevice(adapter, type, window, flags, parameters, device);
                    // The driver's object through the code, not its vtable: a wrapper's vtable hook mustn't wrap twice
                    self = backend;
                    original = reinterpret_cast<CreateDeviceFn>(SystemOriginal(self, 16));
                    if (!original)
                        return E_FAIL;
                }
                else if (s.status.effective == Api::Vulkan)
                    FallBack("DXVK's Direct3DCreate9 failed");
            }
            if (s.status.effective == Api::D3D9On12 && IsOn12(self))
            {
                ForceWindowed(parameters);
                HRESULT hr = original(self, adapter, type, window, flags, parameters, device);
                AfterCreateDevice(hr, device ? *device : nullptr, false, parameters);
                return hr;
            }
            return original(self, adapter, type, window, flags, parameters, device);
        }

        inline HRESULT WINAPI SystemCreateDeviceEx(IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags,
            D3DPRESENT_PARAMETERS* parameters, D3DDISPLAYMODEEX* mode, IDirect3DDevice9Ex** device)
        {
            auto& s = Get();
            auto original = reinterpret_cast<CreateDeviceExFn>(SystemOriginal(self, 20));
            if (!original)
                return E_FAIL;
            ShowNotice();
            if (s.status.effective != Api::Direct3D9 && !IsOn12(self))
            {
                Log("CreateDeviceEx of a system object (a wrapper on top, or the system's Direct3DCreate9Ex): on %s", Name(s.status.effective));
                if (auto backend = BackendObjectEx())
                {
                    if (s.status.effective == Api::Vulkan)
                        return backend->CreateDeviceEx(adapter, type, window, flags, parameters, mode, device);
                    self = backend;
                    original = reinterpret_cast<CreateDeviceExFn>(SystemOriginal(self, 20));
                    if (!original)
                        return E_FAIL;
                }
                else if (s.status.effective == Api::Vulkan)
                    FallBack("DXVK's Direct3DCreate9Ex failed");
            }
            if (s.status.effective == Api::D3D9On12 && IsOn12(self))
            {
                if (ForceWindowed(parameters))
                    mode = nullptr;
                HRESULT hr = original(self, adapter, type, window, flags, parameters, mode, device);
                AfterCreateDevice(hr, device ? *device : nullptr, true, parameters);
                return hr;
            }
            return original(self, adapter, type, window, flags, parameters, mode, device);
        }

        // The vtable of the system's object: the object's own, or, for a wrapper (a hot patched Direct3DCreate9 made
        // one), that of the object it keeps a pointer to among its first members
        inline void** SystemVtable(IUnknown* object, HMODULE system)
        {
            constexpr size_t slots = 21;    // up to CreateDeviceEx
            auto isSystem = [&](void* p)
            {
                if (!Readable(p, sizeof(void*)))
                    return false;
                auto vtable = *static_cast<void***>(p);
                // The vtable is the system's; its CreateDevice slot may already point at a wrapper's hook (ReShade's)
                return InImage(vtable, system) && Readable(vtable, slots * sizeof(void*));
            };
            if (!object)
                return nullptr;
            if (isSystem(object))
                return *reinterpret_cast<void***>(object);
            auto members = reinterpret_cast<void**>(object);
            for (size_t i = 1; i < 32 && Readable(&members[i], sizeof(void*)); ++i)
                if (members[i] && isSystem(members[i]))
                    return *static_cast<void***>(members[i]);
            return nullptr;
        }

        // Once: a system object of each kind shows the vtable (IDirect3D9 slot 16 CreateDevice, IDirect3D9Ex slot 20
        // CreateDeviceEx)
        inline void PatchSystemObjects()
        {
            auto& s = Get();
            std::lock_guard lock(s.mutex);
            if (s.systemPatched || !s.system)
                return;
            s.systemPatched = true;
            if (auto create = reinterpret_cast<Create9>(GetProcAddress(s.system, "Direct3DCreate9")))
            {
                if (auto d3d = create(D3D_SDK_VERSION))
                {
                    auto vtable = SystemVtable(d3d, s.system);
                    Log("System objects: IDirect3D9 %s, CreateDevice in %s", vtable ? "found" : "not found",
                        vtable ? Narrow(ModulePath(ModuleOf(vtable[16]))).c_str() : "-");
                    if (vtable)
                        HookSystemSlot(vtable, 16, reinterpret_cast<void*>(&SystemCreateDevice));
                    d3d->Release();
                }
            }
            if (auto create = reinterpret_cast<Create9Ex>(GetProcAddress(s.system, "Direct3DCreate9Ex")))
            {
                IDirect3D9Ex* d3d = nullptr;
                if (SUCCEEDED(create(D3D_SDK_VERSION, &d3d)) && d3d)
                {
                    if (auto vtable = SystemVtable(d3d, s.system))
                    {
                        HookSystemSlot(vtable, 16, reinterpret_cast<void*>(&SystemCreateDevice));
                        HookSystemSlot(vtable, 20, reinterpret_cast<void*>(&SystemCreateDeviceEx));
                    }
                    d3d->Release();
                }
            }
        }

        // -------------------------------------------------------------------------------------------------------------
        // The imports' replacements

        inline IDirect3D9* WINAPI Direct3DCreate9(UINT sdk)
        {
            auto& s = Get();
            Log("Direct3DCreate9 on %s", Name(s.status.effective));
            ShowNotice();
            if (s.status.effective == Api::Vulkan)
            {
                if (auto d3d = CreateDxvk(sdk))
                    return d3d;
                FallBack("DXVK's Direct3DCreate9 failed");
            }
            else if (s.status.effective == Api::D3D9On12)
            {
                if (auto d3d = CreateOn12(sdk))
                    return d3d;
            }
            return s.systemCreate9 ? s.systemCreate9(sdk) : nullptr;
        }

        inline HRESULT WINAPI Direct3DCreate9Ex(UINT sdk, IDirect3D9Ex** out)
        {
            auto& s = Get();
            Log("Direct3DCreate9Ex on %s", Name(s.status.effective));
            ShowNotice();
            if (s.status.effective == Api::Vulkan)
            {
                HRESULT hr = CreateDxvkEx(sdk, out);
                if (SUCCEEDED(hr))
                    return hr;
                FallBack("DXVK's Direct3DCreate9Ex failed");
            }
            else if (s.status.effective == Api::D3D9On12)
            {
                HRESULT hr = CreateOn12Ex(sdk, out);
                if (SUCCEEDED(hr))
                    return hr;
            }
            return s.systemCreate9Ex ? s.systemCreate9Ex(sdk, out) : E_NOTIMPL;
        }

        // A module that asks for D3D9On12 itself gets it on Fusion Fix's driver when it's there (never Windows' when
        // the driver can be loaded)
        inline IDirect3D9* WINAPI Direct3DCreate9On12(UINT sdk, D3D9ON12_ARGS* args, UINT count)
        {
            auto& s = Get();
            ShowNotice();
            bool on12 = args && count && args->Enable9On12;
            if (on12 && !LoadDriver())
                Log("D3D9On12: a module calls Direct3DCreate9On12 without Fusion Fix's driver");
            auto d3d = s.systemCreate9On12 ? s.systemCreate9On12(sdk, args, count) : nullptr;
            if (d3d && on12)
                RememberOn12(d3d);
            return d3d;
        }

        inline HRESULT WINAPI Direct3DCreate9On12Ex(UINT sdk, D3D9ON12_ARGS* args, UINT count, IDirect3D9Ex** out)
        {
            auto& s = Get();
            ShowNotice();
            bool on12 = args && count && args->Enable9On12;
            if (on12 && !LoadDriver())
                Log("D3D9On12: a module calls Direct3DCreate9On12Ex without Fusion Fix's driver");
            HRESULT hr = s.systemCreate9On12Ex ? s.systemCreate9On12Ex(sdk, args, count, out) : E_NOTIMPL;
            if (SUCCEEDED(hr) && out && *out && on12)
                RememberOn12(*out);
            return hr;
        }

        inline void* Replacement(const char* name)
        {
            if (strcmp(name, "Direct3DCreate9") == 0)
                return reinterpret_cast<void*>(&Direct3DCreate9);
            if (strcmp(name, "Direct3DCreate9Ex") == 0)
                return reinterpret_cast<void*>(&Direct3DCreate9Ex);
            if (strcmp(name, "Direct3DCreate9On12") == 0)
                return reinterpret_cast<void*>(&Direct3DCreate9On12);
            if (strcmp(name, "Direct3DCreate9On12Ex") == 0)
                return reinterpret_cast<void*>(&Direct3DCreate9On12Ex);
            return nullptr;
        }

        inline void Replace(IMAGE_THUNK_DATA* address, void* replacement)
        {
            DWORD protection = 0;
            if (VirtualProtect(&address->u1.Function, sizeof(address->u1.Function), PAGE_READWRITE, &protection))
            {
                address->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
                VirtualProtect(&address->u1.Function, sizeof(address->u1.Function), protection, &protection);
            }
        }

        // The d3d9.dll an import resolves to: replaced when it's the system's or Fusion Fix's, kept when it's a
        // third-party wrapper (it stays on top, the system's objects carry the API underneath)
        inline bool Redirectable(ULONG_PTR function)
        {
            auto& s = Get();
            auto module = ModuleOf(reinterpret_cast<const void*>(function));
            auto kind = Classify(module);
            if (kind == D3D9Module::Own)
                return false;
            // A wrapper may hook the system's exports itself and point the imports at them (ReShade does, when it loads
            // the system's d3d9.dll): with a third-party d3d9.dll loaded, imports of the system's functions stay too
            if (kind == D3D9Module::System)
            {
                auto loaded = GetModuleHandleW(L"d3d9.dll");
                if (loaded && loaded != module && Classify(loaded) == D3D9Module::Foreign)
                {
                    module = loaded;
                    kind = D3D9Module::Foreign;
                }
            }
            bool first = std::find(s.loggedModules.begin(), s.loggedModules.end(), module) == s.loggedModules.end();
            if (first)
                s.loggedModules.push_back(module);
            switch (kind)
            {
            case D3D9Module::System:
                return true;
            case D3D9Module::PassThrough:
                if (first)
                    Log("Fusion Fix's pass-through d3d9.dll: %s", Narrow(ModulePath(module)).c_str());
                return true;
            case D3D9Module::OldProxy:
                s.status.oldProxy = true;
                if (first)
                    Log("An old Fusion Fix d3d9.dll proxy is loaded (%s): bypassed, its d3d9.cfg has no effect", Narrow(ModulePath(module)).c_str());
                return true;
            default:
                if (s.status.wrapper.empty())
                    s.status.wrapper = module ? ModulePath(module) : L"?";
                if (first)
                    Log("A third-party d3d9.dll stays on top of %s: %s", Name(s.status.effective), Narrow(module ? ModulePath(module) : L"?").c_str());
                return false;
            }
        }

        // The imports of d3d9.dll in one module: by name, or, in exes without the names (no OriginalFirstThunk, e.g.
        // NFS Carbon's, rebuilt after unpacking), by the address the loader bound them to
        inline void PatchModule(HMODULE module)
        {
            auto base = reinterpret_cast<uint8_t*>(module);
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE)
                return;
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (!directory.VirtualAddress)
                return;

            for (auto import = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); import->Name; ++import)
            {
                if (_stricmp(reinterpret_cast<const char*>(base + import->Name), "d3d9.dll") != 0)
                    continue;
                if (!import->OriginalFirstThunk)
                {
                    auto d3d9 = GetModuleHandleW(L"d3d9.dll");
                    if (!d3d9)
                        continue;
                    for (auto addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(base + import->FirstThunk); addresses->u1.Function; ++addresses)
                    {
                        for (auto name : { "Direct3DCreate9", "Direct3DCreate9Ex", "Direct3DCreate9On12", "Direct3DCreate9On12Ex" })
                        {
                            auto bound = reinterpret_cast<ULONG_PTR>(GetProcAddress(d3d9, name));
                            if (bound && addresses->u1.Function == bound)
                            {
                                bool redirect = Redirectable(bound);
                                Log("Import of %s in %s (bound, from %s): %s", name, Narrow(ModulePath(module)).c_str(),
                                    Narrow(ModulePath(ModuleOf(reinterpret_cast<const void*>(bound)))).c_str(), redirect ? "redirected" : "kept");
                                if (redirect)
                                    Replace(addresses, Replacement(name));
                            }
                        }
                    }
                    continue;
                }
                auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + import->OriginalFirstThunk);
                auto addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(base + import->FirstThunk);
                for (; names->u1.AddressOfData; ++names, ++addresses)
                {
                    if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                        continue;
                    auto name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name;
                    auto replacement = Replacement(name);
                    if (replacement && addresses->u1.Function != reinterpret_cast<ULONG_PTR>(replacement))
                    {
                        bool redirect = Redirectable(addresses->u1.Function);
                        Log("Import of %s in %s (from %s): %s", name, Narrow(ModulePath(module)).c_str(),
                            Narrow(ModulePath(ModuleOf(reinterpret_cast<const void*>(addresses->u1.Function)))).c_str(), redirect ? "redirected" : "kept");
                        if (redirect)
                            Replace(addresses, replacement);
                    }
                }
            }
        }

        // Every module of the process but the system's, DXVK and the driver: modules loaded later (a plugin that
        // creates the device itself) are patched when Install() runs again
        inline void PatchAll()
        {
            auto& s = Get();
            // From kernel32 by name: psapi.h declares K32EnumProcessModules only for Windows 7 and later targets
            using EnumModules = BOOL(WINAPI*)(HANDLE, HMODULE*, DWORD, LPDWORD);
            auto enumModules = reinterpret_cast<EnumModules>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "K32EnumProcessModules"));
            HMODULE modules[1024];
            DWORD needed = 0;
            if (!enumModules || !enumModules(GetCurrentProcess(), modules, sizeof(modules), &needed))
                return;
            for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < std::size(modules); ++i)
            {
                if (modules[i] == s.dxvk || modules[i] == s.driver || InSystemDirectory(ModulePath(modules[i])))
                    continue;
                PatchModule(modules[i]);
            }
        }

        // The start: the marker, the decision, the setting set back when the guard says so. Install() runs it once.
        inline Decision Start(const Options& options, const Availability& available, std::filesystem::path& marker)
        {
            marker = MarkerPath(options.markerFolder, options.markerName);
            auto found = ReadMarker(marker);
            auto d = Decide(options.api, found, available);
            if (d.removeMarker)
                RemoveMarker(marker);
            if (d.setBack && options.callbacks.setApiSetting)
                options.callbacks.setApiSetting(Api::Direct3D9);
            return d;
        }
    }

    // What vulkan.dll next to the exe is: FusionFixHub's DXVK, the player's own Direct3D 9 implementation (any DXVK
    // build), or a file that can't be used (looked at again when the file, or a link's target, changes)
    inline DxvkInfo Dxvk()
    {
        static std::mutex mutex;
        static detail::FileStamp seen;
        static DxvkInfo info;
        std::lock_guard lock(mutex);
        auto stamp = detail::Stamp(DxvkPath());
        if (!(stamp == seen))
        {
            seen = stamp;
            info = detail::InspectDxvk(DxvkPath());
        }
        return info;
    }

    // Whether Vulkan can be chosen: vulkan.dll is DXVK, FusionFixHub's build or the player's own. Whether that build
    // has the interop DLAA and FSR need is known only on its device (the AA bridge).
    inline bool VulkanAvailable()
    {
        return Dxvk().Usable();
    }

    // Fusion Fix's D3D9On12 driver is next to the exe: d3d9on12.dll with its capability export, and dxbcSigner.dll
    // (looked at again when the file changes)
    inline bool DriverAvailable()
    {
        static std::mutex mutex;
        static detail::FileStamp seen;
        static bool known = false;
        std::lock_guard lock(mutex);
        auto path = GameRoot() / DriverFileName;
        auto stamp = detail::Stamp(path);
        if (!(stamp == seen))
        {
            seen = stamp;
            known = stamp.exists && detail::FileExports(path, D3D9ON12_CAPABILITIES_EXPORT);
        }
        std::error_code ec;
        return known && std::filesystem::is_regular_file(GameRoot() / SignerFileName, ec);
    }

    // Whether the system's runtime can run Direct3D 9 on 12 at all (Windows 10 and later)
    inline bool On12RuntimeAvailable()
    {
        static int available = -1;
        if (available < 0)
        {
            auto system = detail::SystemD3D9();
            available = system && GetProcAddress(system, "Direct3DCreate9On12") ? 1 : 0;
        }
        return available == 1;
    }

    // What menus offer (ASI Settings Availability): DirectX 9 always, Vulkan with DXVK, DirectX 12 with the driver
    inline bool Available(Api api)
    {
        switch (api)
        {
        case Api::Direct3D9: return true;
        case Api::Vulkan: return VulkanAvailable();
        case Api::D3D9On12: return On12RuntimeAvailable() && DriverAvailable();
        }
        return false;
    }

    // Applies the API. Returns the state; State() has it later too.
    inline Status Install(const Options& options)
    {
        auto& s = detail::Get();
        std::lock_guard lock(s.mutex);
        if (!s.status.installed)
        {
            s.options = options;
            s.status.installed = true;
            s.status.requested = options.api;
            s.system = detail::SystemD3D9();     // stays loaded
            if (s.system)
            {
                s.systemCreate9 = reinterpret_cast<detail::Create9>(GetProcAddress(s.system, "Direct3DCreate9"));
                s.systemCreate9Ex = reinterpret_cast<detail::Create9Ex>(GetProcAddress(s.system, "Direct3DCreate9Ex"));
                s.systemCreate9On12 = reinterpret_cast<detail::Create9On12>(GetProcAddress(s.system, "Direct3DCreate9On12"));
                s.systemCreate9On12Ex = reinterpret_cast<detail::Create9On12Ex>(GetProcAddress(s.system, "Direct3DCreate9On12Ex"));
            }
            detail::Availability available;
            if (options.api == Api::Vulkan)
            {
                auto dxvk = Dxvk();
                s.status.dxvk = dxvk.file;
                available.vulkan = dxvk.Usable();
                available.vulkanWhy = dxvk.why;
                if (dxvk.Usable())
                    detail::Log("Vulkan: %s is %s", detail::Narrow(DxvkPath().wstring()).c_str(),
                        dxvk.file == DxvkFile::Known ? "FusionFixHub's DXVK build" : "the player's own DXVK build");
            }
            available.driver = options.api == Api::D3D9On12 && DriverAvailable();
            available.on12Runtime = s.systemCreate9On12 != nullptr;
            auto d = detail::Start(s.options, available, s.marker);
            s.status.effective = d.effective;
            s.status.why = d.why;
            s.status.fellBack = d.setBack;
            if (d.notice)
                detail::QueueNotice(options.api, d.failure);

            if (s.status.effective == Api::Vulkan)
            {
                // By full path; DXVK's own imports (vulkan-1.dll) come from System32
                s.dxvk = LoadLibraryExW(DxvkPath().c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
                if (!s.dxvk || !GetProcAddress(s.dxvk, "Direct3DCreate9"))
                {
                    char why[96];
                    sprintf_s(why, "vulkan.dll couldn't be loaded (error %lu)", GetLastError());
                    if (s.dxvk)
                        FreeLibrary(s.dxvk);
                    s.dxvk = nullptr;
                    s.status.effective = Api::Direct3D9;
                    s.status.why = why;
                }
                else
                    detail::HookVulkanLoader();
            }
            // The driver now: its capabilities (exclusive fullscreen) are known before the game makes its window
            if (s.status.effective == Api::D3D9On12 && !detail::LoadDriver())
            {
                s.status.effective = Api::Direct3D9;
                s.status.why = "D3D9On12 (d3d9on12.dll next to the game) couldn't be loaded";
            }
            // A third-party d3d9.dll known not to work on top of the API: DirectX 9, the setting set back, a notice
            if (auto loaded = GetModuleHandleW(L"d3d9.dll"); loaded && detail::Classify(loaded) == D3D9Module::Foreign)
            {
                s.status.wrapper = detail::ModulePath(loaded);
                if (auto why = detail::IncompatibleWrapper(loaded, s.status.effective, s.status.driverCaps))
                {
                    detail::Log("%s: %s", Name(s.status.effective), why);
                    if (s.options.callbacks.setApiSetting)
                        s.options.callbacks.setApiSetting(Api::Direct3D9);
                    detail::QueueNotice(s.status.effective, why);
                    s.status.why = why;
                    s.status.fellBack = true;
                    s.status.effective = Api::Direct3D9;
                }
            }

            if (s.status.effective != Api::Direct3D9 && !s.marker.empty())
            {
                detail::WriteMarker(s.marker, s.status.effective, std::string("the game closed or crashed before its first frames on ") + Name(s.status.effective));
                s.guarding = true;
            }
            detail::Log("Graphics API: %s asked, %s used%s%s", Name(s.status.requested), Name(s.status.effective),
                s.status.why.empty() ? "" : ": ", s.status.why.c_str());
        }
        detail::PatchAll();
        if (s.status.effective != Api::Direct3D9 || s.noticePending)
            detail::PatchSystemObjects();
        // The notice names a wrapper found only now
        if (s.noticePending && !s.noticeShown)
            s.notice.text = detail::NoticeText(s.notice.failed, s.notice.reason, s.status.wrapper, s.status.dxvk == DxvkFile::Custom);
        return s.status;
    }

    // Applies the API without a crash guard or callbacks (plugins without settings to write back)
    inline Api Install(Api api)
    {
        Options options;
        options.api = api;
        return Install(options).effective;
    }

    // What was asked, what runs, why, what the driver can
    inline Status State()
    {
        auto& s = detail::Get();
        std::lock_guard lock(s.mutex);
        auto status = s.status;
        if (s.driver)
            status.driverCaps = detail::Capabilities(s.driver);
        return status;
    }

    // The capabilities of the loaded driver (D3D9ON12_CAP_*), 0 when it isn't loaded
    inline uint32_t DriverCaps()
    {
        auto& s = detail::Get();
        return detail::Capabilities(s.driver ? s.driver : nullptr);
    }

    // Whether the game may have an exclusive fullscreen device (false only on the driver without EXCLUSIVE_FULLSCREEN)
    inline bool ExclusiveFullscreenAllowed()
    {
        return detail::ExclusiveFullscreenAllowed();
    }

    // On the driver, a fullscreen device is created and reset windowed (a window over the monitor, like without
    // EXCLUSIVE_FULLSCREEN) while wanted() says so, asked at every device creation and Reset. For what needs a window's
    // swap chain: the driver's HDR output (D3D9On12SetHdrOutput) has no color space in exclusive fullscreen. Call it
    // from the plugin's init; wanted runs on the game's render thread.
    inline void SetWindowedOnDriver(std::function<bool()> wanted)
    {
        auto& s = detail::Get();
        std::lock_guard lock(s.mutex);
        s.windowedOnDriver = std::move(wanted);
    }

    // On Vulkan, whether the game may use HDR output in this run: asked once, when DXVK creates its Vulkan instance
    // (the game's Direct3DCreate9). True names DXVK's instance so that the driver presents its swap chains in HDR
    // (detail::VkCreateInstance); a run started without it shows HDR colour spaces as SDR. Call it from the plugin's
    // init, e.g. with "HDR is on, or a display is in HDR mode" (AnyDisplayHdr).
    inline void SetVulkanHdr(std::function<bool()> wanted)
    {
        auto& s = detail::Get();
        std::lock_guard lock(s.mutex);
        s.vulkanHdr = std::move(wanted);
    }

    // Whether DXVK's instance of this run was created for HDR output (SetVulkanHdr)
    inline bool VulkanHdrPresentable()
    {
        return detail::Get().vulkanHdrInstance;
    }

    // Whether a display of the system is in HDR mode (Windows HDR on), through DXGI (loaded by its full path)
    inline bool AnyDisplayHdr()
    {
        auto dxgi = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
        auto createFactory = dxgi ? reinterpret_cast<CreateFactoryFn>(GetProcAddress(dxgi, "CreateDXGIFactory1")) : nullptr;
        IDXGIFactory1* factory = nullptr;
        bool hdr = false;
        if (createFactory && SUCCEEDED(createFactory(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) && factory)
        {
            IDXGIAdapter1* adapter = nullptr;
            for (UINT a = 0; !hdr && factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a)
            {
                IDXGIOutput* output = nullptr;
                for (UINT o = 0; !hdr && adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; ++o)
                {
                    IDXGIOutput6* output6 = nullptr;
                    DXGI_OUTPUT_DESC1 desc{};
                    if (SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput6), reinterpret_cast<void**>(&output6))) && output6)
                    {
                        hdr = SUCCEEDED(output6->GetDesc1(&desc)) && desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
                        output6->Release();
                    }
                    output->Release();
                }
                adapter->Release();
            }
            factory->Release();
        }
        if (dxgi)
            FreeLibrary(dxgi);
        return hdr;
    }

    // The driver's HDR output (D3D9On12Extensions.h), when the loaded driver has that capability; both null otherwise
    struct HdrOutputFunctions
    {
        PFN_D3D9On12SetHdrOutput set = nullptr;
        PFN_D3D9On12QueryHdrOutput query = nullptr;
    };

    inline HdrOutputFunctions HdrOutput()
    {
        auto& s = detail::Get();
        auto driver = s.driver ? s.driver : GetModuleHandleW(DriverFileName);
        if (driver && !GetProcAddress(driver, D3D9ON12_CAPABILITIES_EXPORT))
            driver = nullptr;
        HdrOutputFunctions functions;
        if (detail::Capabilities(driver) & D3D9ON12_CAP_HDR_OUTPUT)
        {
            functions.set = reinterpret_cast<PFN_D3D9On12SetHdrOutput>(GetProcAddress(driver, D3D9ON12_SET_HDR_OUTPUT_EXPORT));
            functions.query = reinterpret_cast<PFN_D3D9On12QueryHdrOutput>(GetProcAddress(driver, D3D9ON12_QUERY_HDR_OUTPUT_EXPORT));
            if (!functions.set || !functions.query)
                functions = {};
        }
        return functions;
    }

    // The driver's queued interop (the AA bridge on DirectX 12), when the loaded driver has that capability
    inline PFN_D3D9On12QueueInterop QueuedInterop()
    {
        auto& s = detail::Get();
        auto driver = s.driver;
        if (!driver)
        {
            // Another plugin's Install() loaded it
            driver = GetModuleHandleW(DriverFileName);
            if (driver && !GetProcAddress(driver, D3D9ON12_CAPABILITIES_EXPORT))
                driver = nullptr;
        }
        if ((detail::Capabilities(driver) & D3D9ON12_CAP_QUEUED_INTEROP) == 0)
            return nullptr;
        return reinterpret_cast<PFN_D3D9On12QueueInterop>(GetProcAddress(driver, D3D9ON12_QUEUE_INTEROP_EXPORT));
    }

    // The API the game runs on: Install()'s state, else what's loaded
    inline Api Running()
    {
        auto& s = detail::Get();
        if (s.status.installed)
            return s.status.effective;
        if (GetModuleHandleW(DriverFileName))
            return Api::D3D9On12;
        if (GetModuleHandleW(L"vulkan-1.dll") || GetModuleHandleW(L"winevulkan.dll"))
            return Api::Vulkan;
        return Api::Direct3D9;
    }
}
