#pragma once

struct Jenkins
{
public:
    static const std::filesystem::path DictionaryPath;

    static uint32_t GetHash(const char* str);
    static uint32_t GetHash(const wchar_t* str);
    inline static uint32_t GetHash(const std::string& str) { return GetHash(str.c_str()); }
    inline static uint32_t GetHash(const std::wstring& str) { return GetHash(str.c_str()); }
};
