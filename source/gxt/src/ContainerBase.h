#pragma once

class cContainerBase
{
public:
	virtual size_t GetSize() const = 0;
	virtual void Read(std::ifstream& is, bool bSwapEndian) = 0;
	virtual void Write(std::ofstream& of, bool bSwapEndian) const = 0;
	virtual void Clear() = 0;
};