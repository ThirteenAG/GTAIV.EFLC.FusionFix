#pragma once

#include "ContainerBase.h"
#include "TableData.h"

class cKeyContainerBase : public cContainerBase
{
public:
	virtual void AddKey(const std::wstring& key, uint32_t offset) = 0;
	virtual std::pair<std::wstring, uint32_t> GetKey(size_t i) const = 0;
	virtual std::pair<uint32_t, uint32_t> GetKeyAsInt(size_t i) const = 0;
	virtual void RemoveKey(size_t i) = 0;
	virtual uint32_t GetKeyOffset(const std::wstring& key) const = 0;
	virtual uint32_t GetKeyOffset(const std::string_view key) const = 0;
	virtual size_t GetNumberOfKeys() const = 0;
	virtual void SortByKeys() const = 0;
	virtual void SortByOffset(const cKeyContainerBase* otherKeys = nullptr) const = 0;
	virtual size_t KeySize() const = 0;
	virtual size_t GetKeyTypeHash() const = 0;

	class iterator
	{
		const cKeyContainerBase* ptr;
		size_t value;
	public:
		iterator(const cKeyContainerBase* c, size_t v) : ptr(c), value(v) {}

		iterator& operator++()
		{
			value++;
			return *this;
		}

		iterator operator++(int)
		{
			iterator ci(ptr, value + 1);
			return ci;
		}

		iterator& operator--()
		{
			value--;
			return *this;
		}

		iterator operator--(int)
		{
			iterator ci(ptr, value - 1);
			return ci;
		}

		bool operator==(const iterator& other) const { return ptr == other.ptr && value == other.value; }
		bool operator!=(const iterator& other) const { return ptr != other.ptr || value != other.value; }

		std::pair<std::wstring, uint32_t> operator*() const { return ptr->GetKey(value); }
	};

	iterator begin() const
	{
		return iterator(this, 0);
	}

	iterator end() const
	{
		return iterator(this, GetNumberOfKeys());
	}
};

template<class key_type>
class cKeyContainer : public cKeyContainerBase
{
	mutable std::vector<TableEntry<key_type>> keys;
	mutable bool bIsSortedByKeys = false;
public:
	virtual size_t GetKeyTypeHash() const
	{
		return typeid(key_type).hash_code();
	}

	virtual void Clear()
	{
		keys.clear();
	}

	virtual void AddKey(const std::wstring& key, uint32_t offset)
	{
		keys.emplace_back(key.c_str(), offset);
		bIsSortedByKeys = false;
	}

	virtual void RemoveKey(size_t i)
	{
		keys.erase(keys.begin() + i);
	}

	virtual std::pair<std::wstring, uint32_t> GetKey(size_t i) const
	{
		assert(i < keys.size());
		const TableEntry<key_type>& entry = keys[i];
		return std::pair<std::wstring, uint32_t>(std::move(to_wstring(entry.key)), entry.offset);
	}

	virtual std::pair<uint32_t, uint32_t> GetKeyAsInt(size_t i) const
	{
		assert(i < keys.size());
		const TableEntry<key_type>& entry = keys[i];
		return std::pair<uint32_t, uint32_t>((uint32_t)entry.key.value, entry.offset);
	}

	virtual size_t GetNumberOfKeys() const
	{
		return keys.size();
	}

	virtual void SortByKeys() const
	{
		if (bIsSortedByKeys) return;
		std::sort(keys.begin(), keys.end());
		bIsSortedByKeys = true;
	}

	virtual void SortByOffset(const cKeyContainerBase* otherKeys) const
	{
		if (otherKeys)
		{
			otherKeys->SortByKeys();
			if (otherKeys->GetKeyTypeHash() == GetKeyTypeHash())
			{
				cKeyContainer<key_type>* otherKeys_t = (cKeyContainer<key_type>*)otherKeys;

				std::sort(keys.begin(), keys.end(), [&otherKeys_t](const TableEntry<key_type>& l, const TableEntry<key_type>& r)
					{
						uint32_t loff = otherKeys_t->GetKeyOffset(l.key);
						uint32_t roff = otherKeys_t->GetKeyOffset(r.key);
						if (loff == UINT32_MAX && roff == UINT32_MAX)
							return l.offset < r.offset;

						return loff < roff;
					});
			}
			else
			{
				std::sort(keys.begin(), keys.end(), [&otherKeys](const TableEntry<key_type>& l, const TableEntry<key_type>& r)
					{
						// ngl this is kinda slow
						uint32_t loff = otherKeys->GetKeyOffset(to_wstring(l.key));
						uint32_t roff = otherKeys->GetKeyOffset(to_wstring(r.key));
						if (loff == UINT32_MAX && roff == UINT32_MAX)
							return l.offset < r.offset;

						return loff < roff;
					});
			}
		}
		else
			std::sort(keys.begin(), keys.end(), offsetSort<key_type>);
		bIsSortedByKeys = false;
	}

	virtual size_t GetSize() const
	{
		return keys.size() * sizeof(TableEntry<key_type>);
	}

	virtual void Read(std::ifstream& is, bool bSwapEndian)
	{
		uint32_t buf;
		is.read((char*)&buf, sizeof(buf));
		assert(buf == 'YEKT');
		is.read((char*)&buf, sizeof(buf));
		if (bSwapEndian)
			buf = _byteswap_ulong(buf);
		uint32_t numOfKeys = buf / sizeof(TableEntry<key_type>);
		assert(numOfKeys * sizeof(TableEntry<key_type>) == buf);

		keys.resize(numOfKeys);
		is.read((char*)keys.data(), keys.size() * sizeof(TableEntry<key_type>));
		if (bSwapEndian)
		{
			for (auto& entry : keys)
			{
				entry.offset = _byteswap_ulong(entry.offset);
				SwapEndian(entry.key);
			}
		}
	}

	virtual void Write(std::ofstream& os, bool bSwapEndian) const
	{
		os.write("TKEY", 4);
		uint32_t n = keys.size() * sizeof(TableEntry<key_type>);
		if (bSwapEndian)
			n = _byteswap_ulong(n);
		os.write((const char*)&n, sizeof(uint32_t));

		for (const auto& key : keys) {
			uint32_t offset = key.offset;
			key_type k = key.key;
			if (bSwapEndian)
			{
				offset = _byteswap_ulong(offset);
				SwapEndian(k);
			}
			os.write((const char*)&offset, sizeof(uint32_t));
			os.write((const char*)&k, sizeof(key_type));
		}
	}

	virtual size_t KeySize() const
	{
		return sizeof(key_type);
	}

	static int __cdecl compare(void*, const void* key, const void* entry)
	{
		key_type* k = (key_type*)key;
		TableEntry<key_type>* d = (TableEntry<key_type>*)entry;
		if (*k < d->key) return -1;
		if (*k > d->key) return 1;
		return 0;
	}

	uint32_t GetKeyOffset(key_type k) const
	{
		if (!bIsSortedByKeys)
			SortByKeys();

		//doesn't work for some reason
		//TableEntry<key_type>* entry = (TableEntry<key_type>*)bsearch_s(&k, keys.data(), keys.size(), sizeof(TableEntry<key_type>), compare, nullptr);

		auto entry = std::find_if(keys.begin(), keys.end(), [&k](auto& i) { return k == i.key; });
		if (entry != keys.end())
			return entry->offset;
		return UINT32_MAX;
	}

	virtual uint32_t GetKeyOffset(const std::wstring& key) const
	{
		return GetKeyOffset((key_type)key.c_str());
	}

	virtual uint32_t GetKeyOffset(const std::string_view key) const
	{
		return GetKeyOffset((key_type)key.data());
	}

};