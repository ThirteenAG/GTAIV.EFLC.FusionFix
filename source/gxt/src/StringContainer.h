#pragma once

#include <fstream>
#include <assert.h>

#include "KeyContainer.h"
#include "DataContainer.h"

#include "umap.h"
#include "KeyChar.h"
#include "Hash.h"
#include "ConversionTable.h"

enum eKeyType
{
	KEYTYPE_CHAR8 = 0,
	KEYTYPE_CHAR16,
	KEYTYPE_CRC32,
	KEYTYPE_JENKINS,
	KEYTYPE_CHAR8_TIME,
	KEYTYPE_CHAR12_TIME,
	KEYTYPE_NONE = -1,
};

eKeyType GetKeyTypeFromWString(std::wstring& value);
eKeyType GetKeyTypeFromWString(const std::wstring& value);

class cStringContainer
{
	struct sHeaderTableEntry
	{
		TableName tableName;
		uint32_t offset = 0;
	};

	struct sTable
	{
		std::unique_ptr<cKeyContainerBase> pKeys;
		std::unique_ptr<cDataContainerBase> pData;

		sTable(eKeyType keyType, bool isWide);

		void Clear()
		{
			pKeys->Clear();
			pData->Clear();
		}

		void Read(std::ifstream& is, bool bSwapEndian)
		{
			pKeys->Read(is, bSwapEndian);
			pData->Read(is, bSwapEndian);
		}

		void Write(std::ofstream& os, bool bSwapEndian) const
		{
			pKeys->Write(os, bSwapEndian);
			pData->Write(os, bSwapEndian);
		}

		void AddString(const std::wstring& key, const std::wstring& str)
		{
			AddKey(key);
			pData->AddString(str);
		}

		std::wstring GetString(const std::wstring& key) const
		{
			uint32_t offset = pKeys->GetKeyOffset(key);
			if (offset == UINT32_MAX)
				return L"";

			return pData->GetString(offset);
		}

		void* GetStringPtr(const std::string_view key) const
		{
			uint32_t offset = pKeys->GetKeyOffset(key);
			if (offset == UINT32_MAX)
				return nullptr;

			return (void*)pData->GetStringPtr(offset);
		}

		bool Contains(const std::string_view key) const
		{
			uint32_t offset = pKeys->GetKeyOffset(key);
			if (offset != UINT32_MAX)
				return true;

			return false;
		}

		void GetMap(std::unordered_map<uint32_t, std::wstring>& dst)
		{
			for (size_t i = pKeys->GetNumberOfKeys(); i > 0; i--) {
				auto key = pKeys->GetKeyAsInt(i - 1);
				dst[key.first] = pData->GetString(key.second);
			}
		}

		void FindUnreachableStrings()
		{
			auto set = pData->GetAllStringsOffsets();
			for (const auto& [key, offset] : *pKeys)
			{
				auto it = set.find(offset);
				if (it != set.end())
					set.erase(it);
			}
			for (auto offset : set)
			{
				pKeys->AddKey(UNREACHABLE_ID_KEY, offset);
			}
		}

		void RemoveUnreachableStrings()
		{
			for (size_t i = pKeys->GetNumberOfKeys(); i > 0; i--) {
				auto key = pKeys->GetKey(i - 1);
				if (key.first == UNREACHABLE_ID_KEY)
					pKeys->RemoveKey(i - 1);
			}
		}

		class const_iterator
		{
			const sTable* table;
			size_t key;
		public:
			const_iterator(const sTable* t, size_t k) : table(t), key(k) {}
			const_iterator& operator++()
			{
				key++;
				return *this;
			}
			bool operator!=(const const_iterator& other) const { return table != other.table || key != other.key; }

			std::pair<std::wstring, std::wstring> operator*() const
			{
				auto k = table->pKeys->GetKey(key);
				return std::make_pair(k.first, table->pData->GetString(k.second));
			}
		};

		const_iterator begin() const
		{
			return const_iterator(this, 0);
		}

		const_iterator end() const
		{
			return const_iterator(this, pKeys->GetNumberOfKeys());
		}
	private:
		size_t AddKey(const std::wstring& key)
		{
			size_t offset = pData->GetSize();
			pKeys->AddKey(key, offset);
			return offset;
		}
	};

	eKeyType m_keyType;
	bool m_isWide;
	bool m_enableTagHighlight; // all ~TAGS~ will have 0x8000 or'ed to them
	std::set<std::wstring> fallthroughKeys;

public:
	umap<TableName, std::unique_ptr<sTable>> m_missionTables;
	std::unique_ptr<sTable> m_mainTable;

	cConversionTable conversionTable;

	union sWriteFlags
	{
		struct
		{
			uint32_t m_LangId : 8;
			uint32_t m_bHeaderType : 2;
			uint32_t m_bSortMainTableKeys : 1;
		};
		uint32_t flags = 0;
	};

	cStringContainer(eKeyType keyType, bool isWide, bool enableTagHighlight) : m_keyType(keyType), m_isWide(isWide), m_enableTagHighlight(enableTagHighlight)
	{
		m_mainTable = std::make_unique<sTable>(m_keyType, m_isWide);
	}

	void Clear()
	{
		m_mainTable->Clear();
		m_missionTables.clear();
	}

	void AddFallthroughKeys(const std::wstring& keys);

	__forceinline void AddFallthroughKey(const std::wstring& key)
	{
		fallthroughKeys.insert(key);
	}

	__forceinline void AddFallthroughKey(std::wstring&& key)
	{
		fallthroughKeys.emplace(key);
	}

	__forceinline void SortMissionTables()
	{
		std::sort(m_missionTables.begin(), m_missionTables.end(),
			[](const std::pair<TableName, std::unique_ptr<sTable>>& l, const std::pair<TableName, std::unique_ptr<sTable>>& r)
			{
				return strcmp(l.first.value, r.first.value) < 0;
			});
	}

	std::wstring ConvertUnicodeStringToGXTString(const std::wstring& text, bool bIsFallthrough) const;
	std::wstring ConvertGXTStringToUnicodeString(const std::wstring& text, bool bIsFallthrough) const;

	void ConvertUnicodeStringToGXTString(std::wstring& str, bool bIsFallthrough) const;
	void ConvertGXTStringToUnicodeString(std::wstring& str, bool bIsFallthrough) const;

	void AddString(const TableName&tablename, const std::wstring& key, const std::wstring& text);
	void AddString(const TableName&tablename, const std::wstring& key, std::wstring& text);

	bool ReadGXT(const std::filesystem::path& path, bool bSwapEndian = false);
	bool WriteGXT(const std::filesystem::path& path, sWriteFlags flags, bool bSwapEndian = false) const;
	bool WriteGXT(std::ofstream& os, sWriteFlags flags, bool bSwapEndian = false) const;
	bool WriteTXT(const std::filesystem::path& path, const cStringContainer* otherGXT = nullptr, bool bSortUsingOriginal = true) const;

private:
	sTable& GetTable(const TableName& name = TableName());
	const sTable& GetTable(const TableName& name = TableName()) const;

	bool HasTable(const TableName& name) const;

	void AddString(sTable& table, const std::wstring& key, const std::wstring& text);
	void AddString(sTable& table, const std::wstring& key, std::wstring& text);

	std::wstring GetStringFromKey(const sTable& table, const std::wstring& key) const;

	void WriteTableToTXT(std::ofstream& os, sTable& table, const cStringContainer* otherGXT = nullptr, bool bSortUsingOriginal = true, const TableName& tableName = TableName()) const;

	void WriteGXTTableList(std::ofstream& os) const;
	void WriteGXTFileHeader(std::ofstream& os) const;
};
