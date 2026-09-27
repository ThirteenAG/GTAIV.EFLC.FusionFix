/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef NVSDK_NGX_LOADER_H
#define NVSDK_NGX_LOADER_H

#include "nvsdk_ngx_defs.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vector>
#include <Winternl.h>
#include <dxgi.h>
#include <d3dkmthk.h>

#else  // defined(_WIN32)

#include <algorithm>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>

#endif  // defined(_WIN32)

#if defined(NVSDK_NGX_LOG)
#define NGX_LOADER_LOG NVSDK_NGX_LOG
#define NGX_LOADER_LOGV NVSDK_NGX_LOG
#else  // defined(_WIN32)
#define NGX_LOADER_LOG
#define NGX_LOADER_LOGV
#endif  // defined(_WIN32)

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_WIN32)

typedef LUID    NVSDK_NGX_LUID;
typedef HMODULE NGXLibHandle;
typedef FARPROC NGXLibSymPtr;

#else  // defined(_WIN32)

typedef struct {
    unsigned int lowPart;
    int highPart;
} NVSDK_NGX_LUID;

typedef void *NGXLibHandle;
typedef void (*NGXLibSymPtr)(void);

#define NGX_CORE_LIBRARY_NAME "libnvidia-ngx.so.1"

#endif  // defined(_WIN32)

#if defined(NVSDK_NGX_HEADER_ONLY)

bool NGXCloseLibrary(NGXLibHandle handle)
{
#if defined(_WIN32)
    return FreeLibrary(handle) == TRUE;
#else  // defined(_WIN32)
    return (dlclose(handle) == 0);
#endif  // defined(_WIN32)
}

#if defined(_WIN32)
static LONG GetStringRegKey(HKEY InKey, const WCHAR *InValueName, WCHAR *OutValue, DWORD dwBufferSize)
{
    ULONG nError = RegQueryValueExW(InKey, InValueName, 0, NULL, (LPBYTE)OutValue, &dwBufferSize);
    return nError;
}

static bool IsValidDirectoryPath(const WCHAR *path)
{
    if (path == nullptr || path[0] == L'\0')
    {
        return false;
    }

    DWORD attrs = GetFileAttributesW(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static LONG NGXGetPathFromRegistry(const WCHAR *InRegKeyHive, const WCHAR *InRegKeyName, WCHAR *OutPath)
{
    HKEY Key;
    NGX_LOADER_LOGV("Trying registry key %ls in %ls", InRegKeyName, InRegKeyHive);
    LONG Res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, InRegKeyHive, 0, KEY_READ, &Key);
    if (Res == ERROR_SUCCESS)
    {
        Res = GetStringRegKey(Key, InRegKeyName, OutPath, MAX_PATH);
        if (Res == ERROR_SUCCESS)
        {
            NGX_LOADER_LOGV("Found registry key %ls in %ls", InRegKeyName, InRegKeyHive);
            if (!IsValidDirectoryPath(OutPath))
            {
                NGX_LOADER_LOGV("Registry path is not a valid directory: %ls", OutPath);
                Res = ERROR_PATH_NOT_FOUND;
            }
        }
        RegCloseKey(Key);
    }
    return Res;
}

static HRESULT NGXGetPathUsingQAI(const LUID adapterLUID, WCHAR* pathToDriverStore)
{
    HRESULT ret = S_OK;
    NGX_LOADER_LOGV("Checking for path to NGX using QueryAdapterInfo");

    D3DKMT_HANDLE hAdapter = 0;

    const wchar_t *dllName = L"gdi32.dll";
    HMODULE hModule = LoadLibraryW(dllName);
    if (!hModule) {
        return E_UNEXPECTED;
    }

    PFND3DKMT_ENUMADAPTERS2 pfnEnumAdapters2;
    PFND3DKMT_CLOSEADAPTER pfnCloseAdapter;
    PFND3DKMT_QUERYADAPTERINFO pfnQueryAdapterInfo;

    pfnEnumAdapters2 = (PFND3DKMT_ENUMADAPTERS2) GetProcAddress(hModule, "D3DKMTEnumAdapters2");
    pfnCloseAdapter = (PFND3DKMT_CLOSEADAPTER) GetProcAddress(hModule, "D3DKMTCloseAdapter");
    pfnQueryAdapterInfo = (PFND3DKMT_QUERYADAPTERINFO) GetProcAddress(hModule, "D3DKMTQueryAdapterInfo");

    if (!pfnEnumAdapters2 || !pfnCloseAdapter || !pfnQueryAdapterInfo)
    {
        NGXCloseLibrary(hModule);
        return E_UNEXPECTED;
    }

    D3DKMT_ENUMADAPTERS2 enumAdapters2 = {};
    enumAdapters2.NumAdapters = 0;
    enumAdapters2.pAdapters = NULL;

    // Get the max number of adapters
    NTSTATUS nt = pfnEnumAdapters2(&enumAdapters2);
    if (!NT_SUCCESS(nt))
    {
        NGXCloseLibrary(hModule);
        return HRESULT_FROM_NT(nt);
    }

    if (enumAdapters2.NumAdapters <= 0) {
        NGXCloseLibrary(hModule);
        return E_UNEXPECTED;
    }

    // Get actual valid adapters
    D3DKMT_ADAPTERINFO * adapterInfos = (D3DKMT_ADAPTERINFO*) ::malloc(sizeof(D3DKMT_ADAPTERINFO) * enumAdapters2.NumAdapters);
    if (!adapterInfos)
    {
        NGXCloseLibrary(hModule);
        return E_OUTOFMEMORY;
    }
    enumAdapters2.pAdapters = adapterInfos;
    nt = pfnEnumAdapters2(&enumAdapters2);
    if (!NT_SUCCESS(nt))
    {
        NGXCloseLibrary(hModule);
        ::free(adapterInfos);
        return HRESULT_FROM_NT(nt);
    }

    for (unsigned int i = 0; i < enumAdapters2.NumAdapters; i++)
    {
        if (adapterInfos[i].AdapterLuid.HighPart == adapterLUID.HighPart && adapterInfos[i].AdapterLuid.LowPart == adapterLUID.LowPart)
        {
            hAdapter = adapterInfos[i].hAdapter;
            break;
        }
    }


    if (hAdapter)
    {
        // The OS has a bug where it under-estimates the required amount of space.
        // As a result, just passing a buffer of size = sizeof(D3DDDI_QUERYREGISTRY_INFO) is not enough to hold all the data.
        // Use malloc here so as to ensure that enough space is allocated.
        D3DDDI_QUERYREGISTRY_INFO* pArgs = (D3DDDI_QUERYREGISTRY_INFO*) malloc(sizeof(D3DDDI_QUERYREGISTRY_INFO) + MAX_PATH * sizeof(WCHAR));
        if (pArgs)
        {
            const wchar_t *pathKeys[2] = {
                L"Parameters\\NGXCore\\NGXPath",
                L"NGXCore\\NGXPath",
            };

            for (int i = 0; i < sizeof(pathKeys) / sizeof(wchar_t *); i++)
            {
                ZeroMemory(pArgs, sizeof(D3DDDI_QUERYREGISTRY_INFO) + MAX_PATH * sizeof(WCHAR));

                NGX_LOADER_LOG("Attempting to read from %ls", pathKeys[i]);

                pArgs->QueryType = D3DDDI_QUERYREGISTRY_SERVICEKEY;
                pArgs->QueryFlags.TranslatePath = 0;
                pArgs->ValueType = REG_SZ;
                wcscpy_s(pArgs->ValueName, MAX_PATH, pathKeys[i]);

                D3DKMT_QUERYADAPTERINFO Args1 = {};
                Args1.hAdapter = hAdapter;
                Args1.Type = KMTQAITYPE_QUERYREGISTRY;
                Args1.pPrivateDriverData = pArgs;
                Args1.PrivateDriverDataSize = sizeof(D3DDDI_QUERYREGISTRY_INFO) + MAX_PATH * sizeof(WCHAR);

                NTSTATUS Status = pfnQueryAdapterInfo(&Args1);
                if (NT_SUCCESS(Status) && pArgs->Status == D3DDDI_QUERYREGISTRY_STATUS_SUCCESS)
                {
                    wcscpy_s(pathToDriverStore, MAX_PATH, pArgs->OutputString);
                    if (!IsValidDirectoryPath(pathToDriverStore))
                    {
                        NGX_LOADER_LOG("QAI path is not a valid directory: %ls", pathToDriverStore);
                        ret = E_FAIL;
                    }
                    else
                    {
                        NGX_LOADER_LOG("Path to driverStore found using QAI: %ls", pathToDriverStore);
                        ret = S_OK;
                        break;
                    }
                }
                else
                {
                    ret = E_FAIL;
                }
            }

            free(pArgs);
        }
        else
        {
            NGX_LOADER_LOG("error: out of memory... unable to query adapter info");
            ret = E_FAIL;
        }
    }
    else
    {
        NGX_LOADER_LOG("error: no matching adapter found");
        ret = E_INVALIDARG;
    }

    for (unsigned int i = 0; i < enumAdapters2.NumAdapters; i++)
    {
        D3DKMT_CLOSEADAPTER closeAdapter = {};
        closeAdapter.hAdapter = adapterInfos[i].hAdapter;
        pfnCloseAdapter(&closeAdapter);
    }
    ::free(adapterInfos);

    NGXCloseLibrary(hModule);
    return ret;
}
#endif  // defined(_WIN32)

NGXLibSymPtr NGXGetLibrarySymbol(NGXLibHandle handle, const char *symbol)
{
#if defined(_WIN32)
    return GetProcAddress(handle, symbol);
#else  // defined(_WIN32)
    return (NGXLibSymPtr)dlsym(handle, symbol);
#endif  // defined(_WIN32)
}

#if defined(_WIN32)
NGXLibHandle NGXLoadLibrary(const wchar_t *filename)
{
    HMODULE Lib = LoadLibraryW(filename);
    if (Lib)
    {
        NGX_LOADER_LOGV("Loaded NGXCore from path (%ls)", filename);
    }
    else
    {
        NGX_LOADER_LOG("error: failed to load NGXCore: %d (%ls)", GetLastError(), filename);
    }

    return Lib;
}
#else  // defined(_WIN32)
NGXLibHandle NGXLoadLibrary(const char *filename)
{
    NGXLibHandle handle = nullptr;

    handle = dlopen(filename, RTLD_NOW);
    if (handle) {
        NGX_LOADER_LOGV("Loaded library from path (%s)", filename);
    }

    return handle;
}
#endif  // defined(_WIN32)

NGXLibHandle NGXLoadCoreLibrary(NVSDK_NGX_LUID adapterLUID)
{
#if defined(_WIN32)
    NGXLibHandle Lib = nullptr;
    WCHAR Path[MAX_PATH];

    std::vector<const wchar_t*> dll_filenames;

#if defined(_M_ARM64)
    dll_filenames.push_back(L"\\_arm64_nvngx.dll");
#else
    // drivers 471.11 and later have to use "_nvngx.dll" instead of "nvngx.dll" - so check both names
    dll_filenames.push_back(L"\\_nvngx.dll");
    dll_filenames.push_back(L"\\nvngx.dll");
#endif

    // Modified to comply with DCH changes https://confluence.nvidia.com/display/MSTET/NGX
    // First, check if NGXCore is present next to the app binary
    DWORD ret = GetModuleFileNameW(NULL, Path, MAX_PATH);
    if (ret)
    {
        for (size_t i = 0; Lib == nullptr && i < dll_filenames.size(); ++i)
        {
            // Remove the name of the app
            wchar_t* PSTR = wcsrchr(Path, L'\\');
            if (PSTR)
            {
                *PSTR = L'\0';
            }
            wcscat_s(Path, dll_filenames[i]);
            Lib = NGXLoadLibrary(Path);
        }
    }
    else
    {
        NGX_LOADER_LOGV("warning: unable to find filename for the application");
    }

    if (!Lib)
    {
        // Now check the regkeys and look for NGXCore in the production folders
        NGX_LOADER_LOGV("NGXCore not found next to the application");

        LONG Res = ERROR_FUNCTION_FAILED;

        // First try using QueryAdapterInfo to read the regkey set by driver installer (would only work for NGX installed via declarative INF)
        HRESULT hr = NGXGetPathUsingQAI(adapterLUID, Path);
        if (FAILED(hr))
        {
            // Perhaps there was an error while trying to use QAI. So, try the raw OS regkey read function (would only work for NGX installed via declarative INF)
            Res = NGXGetPathFromRegistry(L"System\\CurrentControlSet\\Services\\nvlddmkm\\Parameters\\NGXCore", L"NGXPath", Path);
            if (Res != ERROR_SUCCESS)
            {
                Res = NGXGetPathFromRegistry(L"System\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore", L"NGXPath", Path);
                if (Res != ERROR_SUCCESS)
                {
                    // Finally, fall back to legacy location (all non-DCH drivers should have this regkey present)
                    // NOTE: Do not remove! This regkey is actively used by
                    // Proton/Wine which don't have a concept of DCH drivers or a
                    // "Driver Store".
                    // See: https://github.com/ValveSoftware/wine/commit/0bff1b4fc6929694b706bd7e659ed0b58ff51f2d
                    Res = NGXGetPathFromRegistry(L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", L"FullPath", Path);
                }
            }
        }
        if (SUCCEEDED(hr) || Res == ERROR_SUCCESS)
        {
            for (size_t i = 0; i < dll_filenames.size(); ++i)
            {
                wcscat_s(Path, dll_filenames[i]);
                Lib = NGXLoadLibrary(Path);
                if (Lib)
                {
                    NGX_LOADER_LOG("Loading %ls succeeded", Path);
                    break;
                }
                else
                {
                    NGX_LOADER_LOG("warning: failed to load %ls", Path);
                    // Remove the name of the DLL that has failed to load
                    wchar_t *PSTR = wcsrchr(Path, L'\\');
                    if (PSTR)
                    {
                        *PSTR = L'\0';
                    }
                }
            }
            if (!Lib)
            {
                NGX_LOADER_LOG("error: failed to load nvngx");
            }
        }
        else
        {
            NGX_LOADER_LOG("error: failed to locate NGX core path via registry key - error %ld", Res);
        }
    }
    return Lib;
#else  // defined(_WIN32)
    (void)adapterLUID;

    NGXLibHandle handle = nullptr;

    handle = NGXLoadLibrary(NGX_CORE_LIBRARY_NAME);
    if (handle) {
        NGX_LOADER_LOGV("Loaded NGXCore from global search paths");
        return handle;
    }
    return nullptr;
#endif  // defined(_WIN32)
}

#endif /* NVSDK_NGX_HEADER_ONLY */

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* NVSDK_NGX_LOADER_H */
