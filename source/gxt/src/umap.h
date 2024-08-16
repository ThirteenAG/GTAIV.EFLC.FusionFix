#pragma once

template<class t1, class t2>
class TablePair : public std::pair<t1, t2>
{
public:
	bool operator<(const TablePair<t1, t2>& r) const { return this->first < r.first; }
};

template<class t1, class t2>
bool operator<(const TablePair<t1, t2>& l, const TablePair<t1, t2>& r) { return l.first < r.first; }

template<class t1, class t2>
class umap : public std::vector<TablePair<t1, t2>>
{
	static const t2 _empty;
public:
	const t2& operator[](const t1& k) const
	{
		for (auto elem = this->begin(); elem != this->end(); elem++)
		{
			if (elem->first == k)
				return elem->second;
		}
		return _empty;
	}

	const t2& operator[](t1&& k) const
	{
		for (auto elem = this->begin(); elem != this->end(); elem++)
		{
			if (elem->first == k)
				return elem->second;
		}
		return _empty;
	}

	t2& operator[](const t1& k)
	{
		for (auto elem = this->begin(); elem != this->end(); elem++)
		{
			if (elem->first == k)
				return elem->second;
		}
		this->resize(this->size() + 1);
		(this->end() - 1)->first = k;
		return (this->end() - 1)->second;
	}

	t2& operator[](t1&& k)
	{
		for (auto elem = this->begin(); elem != this->end(); elem++)
		{
			if (elem->first == k)
				return elem->second;
		}
		this->resize(this->size() + 1);
		(this->end() - 1)->first = k;
		return (this->end() - 1)->second;
	}
};

template<class t1, class t2>
const t2 umap<t1, t2>::_empty;