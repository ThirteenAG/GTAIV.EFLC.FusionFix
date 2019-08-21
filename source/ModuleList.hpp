#pragma once

#include <vector>
#include <algorithm>
#include <cassert>
#include <tuple>

// Stores a list of loaded modules with their names, WITHOUT extension
class ModuleList
{
public:
    enum class SearchLocation
    {
        All,
        LocalOnly,
        SystemOnly,
    };

    // Initializes module list
    // Needs to be called before any calls to Get or GetAll
    void Enumerate(SearchLocation location = SearchLocation::All)
    {
        constexpr size_t INITIAL_SIZE = sizeof(HMODULE) * 256;
        HMODULE* modules = static_cast<HMODULE*>(malloc(INITIAL_SIZE));
        if (modules != nullptr)
        {
            typedef BOOL(WINAPI * Func)(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);

            HMODULE hLib = LoadLibrary(TEXT("kernel32"));
            assert(hLib != nullptr); // If this fails then everything is probably broken anyway

            Func pEnumProcessModules = reinterpret_cast<Func>(GetProcAddress(hLib, "K32EnumProcessModules"));
            if (pEnumProcessModules == nullptr)
            {
                // Try psapi
                FreeLibrary(hLib);
                hLib = LoadLibrary(TEXT("psapi"));
                if (hLib != nullptr)
                {
                    pEnumProcessModules = reinterpret_cast<Func>(GetProcAddress(hLib, "EnumProcessModules"));
                }
            }

            if (pEnumProcessModules != nullptr)
            {
                const HANDLE currentProcess = GetCurrentProcess();
                DWORD cbNeeded = 0;
                if (pEnumProcessModules(currentProcess, modules, INITIAL_SIZE, &cbNeeded) != 0)
                {
                    if (cbNeeded > INITIAL_SIZE)
                    {
                        HMODULE* newModules = static_cast<HMODULE*>(realloc(modules, cbNeeded));
                        if (newModules != nullptr)
                        {
                            modules = newModules;

                            if (pEnumProcessModules(currentProcess, modules, cbNeeded, &cbNeeded) != 0)
                            {
                                EnumerateInternal(modules, location, cbNeeded / sizeof(HMODULE));
                            }
                        }
                    }
                    else
                    {
                        EnumerateInternal(modules, location, cbNeeded / sizeof(HMODULE));
                    }
                }
            }

            if (hLib != nullptr)
            {
                FreeLibrary(hLib);
            }

            free(modules);
        }
    }

    // Recreates module list
    void ReEnumerate(SearchLocation location = SearchLocation::All)
    {
        Clear();
        Enumerate(location);
    }

    // Clears module list
    void Clear()
    {
        m_moduleList.clear();
    }

    // Gets handle of a loaded module with given name, NULL otherwise
    HMODULE Get(const wchar_t* moduleName) const
    {
        // If vector is empty then we're trying to call it without calling Enumerate first
        assert(m_moduleList.size() != 0);

        auto it = std::find_if(m_moduleList.begin(), m_moduleList.end(), [&](const auto& e) {
            return _wcsicmp(moduleName, std::get<1>(e).c_str()) == 0;
            });
        return it != m_moduleList.end() ? std::get<0>(*it) : nullptr;
    }

    // Gets handles to all loaded modules with given name
    std::vector<HMODULE> GetAll(const wchar_t* moduleName) const
    {
        // If vector is empty then we're trying to call it without calling Enumerate first
        assert(m_moduleList.size() != 0);

        std::vector<HMODULE> results;
        for (auto& e : m_moduleList)
        {
            if (_wcsicmp(moduleName, std::get<1>(e).c_str()) == 0)
            {
                results.push_back(std::get<0>(e));
            }
        }

        return results;
    }

private:
    void EnumerateInternal(HMODULE* modules, SearchLocation location, size_t numModules)
    {
        auto GetModuleFileNameW = [](HMODULE hModule) -> std::wstring
        {
            static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
            static constexpr auto MAX_ITERATIONS = 7;
            std::wstring ret;
            auto bufferSize = INITIAL_BUFFER_SIZE;
            for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
            {
                ret.resize(bufferSize);
                auto charsReturned = ::GetModuleFileNameW(hModule, &ret[0], bufferSize);
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
            return L"";
        };

        const auto exeModulePath = GetModuleFileNameW(NULL).substr(0, GetModuleFileNameW(NULL).find_last_of(L"/\\"));

        m_moduleList.reserve(numModules);
        for (size_t i = 0; i < numModules; i++)
        {
            // Obtain module name, with resizing if necessary
            auto moduleName = GetModuleFileNameW(*modules);

            if (!moduleName.empty())
            {
                auto starts_with = [](const std::wstring &big_str, const std::wstring &small_str) -> bool
                {
                    return big_str.compare(0, small_str.length(), small_str) == 0;
                };

                const wchar_t* nameBegin = wcsrchr(moduleName.c_str(), '\\') + 1;
                const wchar_t* dotPos = wcsrchr(nameBegin, '.');
                bool isLocal = starts_with(std::wstring(moduleName), exeModulePath);

                if ((isLocal && location != SearchLocation::SystemOnly) || (!isLocal && location != SearchLocation::LocalOnly))
                {
                    if (dotPos != nullptr)
                    {
                        m_moduleList.emplace_back(*modules, std::wstring(nameBegin, dotPos), isLocal);
                    }
                    else
                    {
                        m_moduleList.emplace_back(*modules, nameBegin, isLocal);
                    }
                }
            }

            modules++;
        }
    }

public: std::vector< std::tuple<HMODULE, std::wstring, bool> > m_moduleList;
};