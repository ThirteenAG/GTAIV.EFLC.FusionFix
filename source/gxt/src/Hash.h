#pragma once

extern std::set<std::wstring> usedKeys;

void DumpUsedKeys();

template<class T>
struct Hash
{
	uint32_t value = 0;

public:
	Hash() = default;
	Hash(uint32_t val) : value(val) {}
	Hash(const char* str)
	{
		if (str[0] == '#')
		{
			if (sscanf_s(str + 1, "%x", &value) == 1)
				return;
		}
		value = T::GetHash(str);
	}

	Hash(const wchar_t* str)
	{
		if (str[0] == L'#')
		{
			if (swscanf_s(str + 1, L"%x", &value) == 1)
				return;
		}
		value = T::GetHash(str);
	}
	Hash(const std::string& str) { Hash(str.c_str()); }
	Hash(const std::wstring& str) { Hash(str.c_str()); }

	inline friend bool operator<(const Hash<T>& l, const Hash<T>& r) { return l.value < r.value; }
	inline friend bool operator>(const Hash<T>& l, const Hash<T>& r) { return l.value > r.value; }
	inline friend bool operator<=(const Hash<T>& l, const Hash<T>& r) { return l.value <= r.value; }
	inline friend bool operator>=(const Hash<T>& l, const Hash<T>& r) { return l.value >= r.value; }
	inline friend bool operator==(const Hash<T>& l, const Hash<T>& r) { return l.value == r.value; }
	inline friend bool operator!=(const Hash<T>& l, const Hash<T>& r) { return l.value != r.value; }

	friend std::wstring to_wstring(const Hash<T>& v)
	{
		// load dictionary once we need it
		static std::map<uint32_t, std::wstring> dictionary = ReadDictionary(T::DictionaryPath);
		auto it = dictionary.find(v.value);
		if (it != dictionary.end())
		{
			usedKeys.insert(it->second);
			return it->second;
		}

		wchar_t out[10];
		swprintf_s(out, L"#%08X", v.value);
		return out;
	}

	inline friend void SwapEndian(Hash& hash)
	{
		hash.value = _byteswap_ulong(hash.value);
	}
private:
	static std::map<uint32_t, std::wstring> ReadDictionary(std::filesystem::path path)
	{
		std::map<uint32_t, std::wstring> dictionary;

		if (!std::filesystem::exists(path))
		{
			if (!path.is_absolute())
			{
				path = GetApplicationPath() / path;
				if (!std::filesystem::exists(path))
				{
					wprintf_s(L"Couldn't load dictionary %s\n", path.c_str());
					return dictionary;
				}
			}
			else
			{
				wprintf_s(L"Couldn't load dictionary %s\n", path.c_str());
				return dictionary;
			}
		}

		std::wifstream is(path, std::ios::in);

		dictionary.emplace(T::GetHash(UNREACHABLE_ID_KEY), UNREACHABLE_ID_KEY);

		if (is.is_open())
		{
			std::wstring wstr;
			while (std::getline(is, wstr))
				dictionary.emplace(T::GetHash(wstr), wstr);

			is.close();
			wprintf_s(L"Dictionary %s loaded\n", path.c_str());
		}

		return dictionary;
	}
};
