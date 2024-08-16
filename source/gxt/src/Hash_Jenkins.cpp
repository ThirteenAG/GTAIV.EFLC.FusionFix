#include "common.h"
#include "Hash_Jenkins.h"

const std::filesystem::path Jenkins::DictionaryPath = L"Jenkins_dictionary.txt";

template <class T>
uint32_t jenkins_one_at_a_time_hash(const T* key)
{
	uint32_t hash = 0;
	typedef std::make_unsigned<T>::type char_type;
	while (*key)
	{
		hash += (char_type)t_tolower(*(key++));
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}

uint32_t Jenkins::GetHash(const char* str)
{
	return jenkins_one_at_a_time_hash(str);
}

uint32_t Jenkins::GetHash(const wchar_t* str)
{
	return jenkins_one_at_a_time_hash(str);
}