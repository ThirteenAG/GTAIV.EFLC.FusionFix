#pragma once

template<class T>
class TableEntry
{
public:
	uint32_t offset;
	T key;

	TableEntry() : offset(0) {};
	TableEntry(T _key, uint32_t _offset) { key = _key; offset = _offset; }
	TableEntry<T>& operator =(const TableEntry<T>& t)
	{
		key = t.key; offset = t.offset;
		return *this;
	}
};

template<class T>
bool operator<(const TableEntry<T>& l, const TableEntry<T>& r)
{
	return l.key < r.key;
}

template<class T>
bool offsetSort(const TableEntry<T>& l, const TableEntry<T>& r)
{
	return l.offset < r.offset;
}
