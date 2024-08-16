#pragma once

class cConversionTable
{
	std::map<wchar_t, wchar_t> map;
	std::map<wchar_t, wchar_t> reverseMap;
public:
	__forceinline wchar_t GetGxtChar(wchar_t wc) const
	{
		auto it = map.find(wc);
		if (it == map.end())
			return wc;
		return it->second;
	}

	__forceinline wchar_t GetUnicodeChar(wchar_t gxtc) const
	{
		auto it = reverseMap.find(gxtc);
		if (it == reverseMap.end())
			return gxtc;
		return it->second;
	}

	__forceinline void SetUnicodeChar(wchar_t wc, wchar_t gxtc)
	{
		map.emplace(wc, gxtc);
		reverseMap.emplace(gxtc, wc);
	}

	void ReadFromFile(const std::filesystem::path& path);
	void WriteToFile(const std::filesystem::path& path) const;
};