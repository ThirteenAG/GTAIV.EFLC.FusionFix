#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <shlobj.h>

struct d3d9_dll
{
    HMODULE dll;
    FARPROC D3DPERF_BeginEvent;
    FARPROC D3DPERF_EndEvent;
    FARPROC D3DPERF_GetStatus;
    FARPROC D3DPERF_QueryRepeatFrame;
    FARPROC D3DPERF_SetMarker;
    FARPROC D3DPERF_SetOptions;
    FARPROC D3DPERF_SetRegion;
    FARPROC DebugSetLevel;
    FARPROC DebugSetMute;
    FARPROC Direct3D9EnableMaximizedWindowedModeShim;
    FARPROC Direct3DCreate9;
    FARPROC Direct3DCreate9Ex;
    FARPROC Direct3DCreate9On12;
    FARPROC Direct3DCreate9On12Ex;
    FARPROC Direct3DShaderValidatorCreate9;
    FARPROC PSGPError;
    FARPROC PSGPSampleTexture;
} d3d9;

__declspec(naked) void _D3DPERF_BeginEvent()
{
    _asm { jmp[d3d9.D3DPERF_BeginEvent] }
}
__declspec(naked) void _D3DPERF_EndEvent()
{
    _asm { jmp[d3d9.D3DPERF_EndEvent] }
}
__declspec(naked) void _D3DPERF_GetStatus()
{
    _asm { jmp[d3d9.D3DPERF_GetStatus] }
}
__declspec(naked) void _D3DPERF_QueryRepeatFrame()
{
    _asm { jmp[d3d9.D3DPERF_QueryRepeatFrame] }
}
__declspec(naked) void _D3DPERF_SetMarker()
{
    _asm { jmp[d3d9.D3DPERF_SetMarker] }
}
__declspec(naked) void _D3DPERF_SetOptions()
{
    _asm { jmp[d3d9.D3DPERF_SetOptions] }
}
__declspec(naked) void _D3DPERF_SetRegion()
{
    _asm { jmp[d3d9.D3DPERF_SetRegion] }
}
__declspec(naked) void _DebugSetLevel()
{
    _asm { jmp[d3d9.DebugSetLevel] }
}
__declspec(naked) void _DebugSetMute()
{
    _asm { jmp[d3d9.DebugSetMute] }
}
__declspec(naked) void _Direct3D9EnableMaximizedWindowedModeShim()
{
    _asm { jmp[d3d9.Direct3D9EnableMaximizedWindowedModeShim] }
}
__declspec(naked) void _Direct3DCreate9()
{
    _asm { jmp[d3d9.Direct3DCreate9] }
}
__declspec(naked) void _Direct3DCreate9Ex()
{
    _asm { jmp[d3d9.Direct3DCreate9Ex] }
}
__declspec(naked) void _Direct3DCreate9On12()
{
    _asm { jmp[d3d9.Direct3DCreate9On12] }
}
__declspec(naked) void _Direct3DCreate9On12Ex()
{
    _asm { jmp[d3d9.Direct3DCreate9On12Ex] }
}
__declspec(naked) void _Direct3DShaderValidatorCreate9()
{
    _asm { jmp[d3d9.Direct3DShaderValidatorCreate9] }
}
__declspec(naked) void _PSGPError()
{
    _asm { jmp[d3d9.PSGPError] }
}
__declspec(naked) void _PSGPSampleTexture()
{
    _asm { jmp[d3d9.PSGPSampleTexture] }
}

void _FusionFixGraphicsApiSwitch()
{
    return;
}

std::filesystem::path GetModulePath(HMODULE hModule)
{
    static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
    static constexpr auto MAX_ITERATIONS = 7;

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

    return {};
}

std::filesystem::path GetExeModulePath()
{
    std::filesystem::path r = GetModulePath(NULL);
    return r.parent_path();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        d3d9.dll = NULL;

        WCHAR path[MAX_PATH];
        int apiValue = 0;
        {
            std::vector<std::filesystem::path> cfgPaths;
            auto cfgName = L"d3d9.cfg";
            cfgPaths.emplace_back(GetExeModulePath() / cfgName);
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path)))
                cfgPaths.emplace_back(std::filesystem::path(path) / L"Rockstar Games\\GTA IV\\" / cfgName);
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path)))
                cfgPaths.emplace_back(std::filesystem::path(path) / cfgName);
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, 0, path)))
                cfgPaths.emplace_back(std::filesystem::path(path) / cfgName);

            std::filesystem::path cfgPath;

            // First, try to find an existing readable file
            for (auto& it : cfgPaths)
            {
                std::error_code ec;
                if (std::filesystem::exists(it, ec) && !ec)
                {
                    auto status = std::filesystem::status(it, ec);
                    if (!ec && status.type() == std::filesystem::file_type::regular)
                    {
                        // Check if we can read from this file
                        std::ifstream testFile(it);
                        if (testFile.good())
                        {
                            cfgPath = it;
                            testFile.close();
                            break;
                        }
                    }
                }
            }

            // If no existing readable file found, find a writable location for new file creation
            if (cfgPath.empty())
            {
                for (auto& it : cfgPaths)
                {
                    std::error_code ec;
                    auto status = std::filesystem::status(it, ec);

                    if (ec && ec.value() == 2) // File doesn't exist, check if directory is writable
                    {
                        std::ofstream ofile;
                        ofile.open(it, std::ios::binary);
                        if (ofile.is_open())
                        {
                            cfgPath = it;
                            ofile.close();
                            break;
                        }
                    }
                    else if (!ec && ((std::filesystem::perms::owner_read & status.permissions()) == std::filesystem::perms::owner_read &&
                        (std::filesystem::perms::owner_write & status.permissions()) == std::filesystem::perms::owner_write))
                    {
                        cfgPath = it;
                        break;
                    }
                }
            }

            // Fallback to first path if nothing else worked
            if (cfgPath.empty())
            {
                cfgPath = cfgPaths.front();
            }

            apiValue = GetPrivateProfileIntW(L"MAIN", L"API", 0, cfgPath.c_str());
        }

        if (apiValue)
        {
            lstrcpyW(path, L"vulkan.dll");
            d3d9.dll = LoadLibraryW(path);
        }

        if (d3d9.dll == NULL)
        {
            DWORD pathLen = GetSystemDirectoryW(path, MAX_PATH - wcslen(L"\\d3d9.dll"));
            lstrcpyW(path + pathLen, L"\\d3d9.dll");
            d3d9.dll = LoadLibraryW(path);
        }

        if (d3d9.dll == NULL)
        {
            DWORD error = GetLastError();
            wchar_t errorMsg[512];
            wsprintfW(errorMsg, L"Failed to load both vulkan.dll and d3d9.dll!\nError code: %lu\nPath: %ls", error, path);
            MessageBoxW(0, errorMsg, L"DLL Load Error", MB_ICONERROR);
            ExitProcess(0);
        }

        d3d9.D3DPERF_BeginEvent = GetProcAddress(d3d9.dll, "D3DPERF_BeginEvent");
        d3d9.D3DPERF_EndEvent = GetProcAddress(d3d9.dll, "D3DPERF_EndEvent");
        d3d9.D3DPERF_GetStatus = GetProcAddress(d3d9.dll, "D3DPERF_GetStatus");
        d3d9.D3DPERF_QueryRepeatFrame = GetProcAddress(d3d9.dll, "D3DPERF_QueryRepeatFrame");
        d3d9.D3DPERF_SetMarker = GetProcAddress(d3d9.dll, "D3DPERF_SetMarker");
        d3d9.D3DPERF_SetOptions = GetProcAddress(d3d9.dll, "D3DPERF_SetOptions");
        d3d9.D3DPERF_SetRegion = GetProcAddress(d3d9.dll, "D3DPERF_SetRegion");
        d3d9.DebugSetLevel = GetProcAddress(d3d9.dll, "DebugSetLevel");
        d3d9.DebugSetMute = GetProcAddress(d3d9.dll, "DebugSetMute");
        d3d9.Direct3D9EnableMaximizedWindowedModeShim = GetProcAddress(d3d9.dll, "Direct3D9EnableMaximizedWindowedModeShim");
        d3d9.Direct3DCreate9 = GetProcAddress(d3d9.dll, "Direct3DCreate9");
        d3d9.Direct3DCreate9Ex = GetProcAddress(d3d9.dll, "Direct3DCreate9Ex");
        d3d9.Direct3DCreate9On12 = GetProcAddress(d3d9.dll, "Direct3DCreate9On12");
        d3d9.Direct3DCreate9On12Ex = GetProcAddress(d3d9.dll, "Direct3DCreate9On12Ex");
        d3d9.Direct3DShaderValidatorCreate9 = GetProcAddress(d3d9.dll, "Direct3DShaderValidatorCreate9");
        d3d9.PSGPError = GetProcAddress(d3d9.dll, "PSGPError");
        d3d9.PSGPSampleTexture = GetProcAddress(d3d9.dll, "PSGPSampleTexture");
        break;
    }
    case DLL_PROCESS_DETACH:
    {
        FreeLibrary(d3d9.dll);
        break;
    }
    }
    return TRUE;
}