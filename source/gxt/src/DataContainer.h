#pragma once

#include "ContainerBase.h"

class cDataContainerBase : public cContainerBase
{
public:
	virtual void AddString(const std::wstring& str) = 0;
	virtual std::wstring GetString(size_t offset) const = 0;
	virtual void* GetStringPtr(size_t offset) const = 0;
	virtual std::set<size_t> GetAllStringsOffsets() const = 0;
};

template<class char_type>
class cDataContainer : public cDataContainerBase
{
	std::vector<char_type> data;
public:
	virtual void Clear()
	{
		data.clear();
	}

	virtual void AddString(const std::wstring& str)
	{
		size_t offset = data.size();
		data.resize(offset + str.length() + 1);
		for (const wchar_t c : str)
		{
			assert(sizeof(char_type) > 1 || c < 0x100);
			data[offset++] = static_cast<char_type>(c);
		}
		data[offset] = L'\0';
	}

	virtual std::wstring GetString(size_t offset) const
	{
		offset /= sizeof(char_type);
		std::wstring str;
		do
		{
			assert(offset < data.size());
			str.push_back(std::make_unsigned<char_type>::type(data[offset++]));
		} while (data[offset]);
		return str;
	}

	virtual void* GetStringPtr(size_t offset) const
	{
		offset /= sizeof(char_type);
		return (void*)&data[offset];
	}

	virtual std::set<size_t> GetAllStringsOffsets() const
	{
		std::set<size_t> set;
		set.insert(0);
		for (size_t i = 1; i < data.size(); i++) {
			if (data[i - 1] == '\0')
				set.insert(i * sizeof(char_type));
		}
		return set;
	}

	virtual size_t GetSize() const
	{
		return data.size() * sizeof(char_type);
	}

	virtual void Read(std::ifstream& is, bool bSwapEndian)
	{
		uint32_t buf;
		is.read((char*)&buf, sizeof(buf));
		assert(buf == 'TADT');
		is.read((char*)&buf, sizeof(buf));
		if (bSwapEndian)
			buf = _byteswap_ulong(buf);
		uint32_t numOfChars = buf / sizeof(char_type);
		assert(numOfChars * sizeof(char_type) == buf);

		data.resize(numOfChars);
		is.read((char*)data.data(), data.size() * sizeof(char_type));
		if (bSwapEndian && sizeof(char_type) > 1)
		{
			for (auto& c : data)
				c = t_byteswap(c);
		}
	}

	virtual void Write(std::ofstream& os, bool bSwapEndian) const
	{
		os.write("TDAT", 4);
		uint32_t n = data.size() * sizeof(char_type);
		if (bSwapEndian)
			n = _byteswap_ulong(n);
		os.write((const char*)&n, sizeof(n));

		if (bSwapEndian && sizeof(char_type) > 1)
		{
			for (auto c : data)
			{
				c = t_byteswap(c);
				os.write((const char*)c, sizeof(char_type));
			}
		}
		else
			os.write((const char*)data.data(), n);
	}
};