#pragma once
#include <cctype>
#include <cwctype>

// trim from start (in place)
static inline void ltrim(std::wstring& s) {
	size_t pos = s.find_first_not_of(L" \f\n\r\t\v");
	if (pos != std::wstring::npos)
		s.erase(0, pos);
	else
		s = L"";
}

// trim from end (in place)
static inline void rtrim(std::wstring& s) {
	size_t pos = s.find_last_not_of(L" \f\n\r\t\v");
	if (pos == std::wstring::npos)
	{
		s = L"";
		return;
	}
	if (pos != s.size())
		s.resize(pos + 1);
}

// trim from both ends (in place)
static inline void trim(std::wstring& s) {
	rtrim(s);
	ltrim(s);
}

// trim from start (copying)
static inline std::wstring ltrim_copy(std::wstring s) {
	ltrim(s);
	return s;
}

// trim from end (copying)
static inline std::wstring rtrim_copy(std::wstring s) {
	rtrim(s);
	return s;
}

// trim from both ends (copying)
static inline std::wstring trim_copy(std::wstring s) {
	trim(s);
	return s;
}

static inline void to_upper(std::wstring& s)
{
	for (wchar_t& c : s)
		c = towupper(c);
}

static inline std::wstring to_upper_copy(const std::wstring& s)
{
	std::wstring out(s.size(), L'\0');
	size_t i = 0;
	for (wchar_t c : s)
		out[i++]=towupper(c);
	return out;
}

const std::filesystem::path& GetApplicationPath();
std::wstring ReadFileToWString(const std::filesystem::path& filename, int suggestedEncoding);
