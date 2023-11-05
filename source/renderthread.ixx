module;

#include <common.hxx>

export module renderthread;

import common;
import comvars;

export class CBaseDC 
{
public:
    uint32_t field_1;

public:
    static inline void* AppendAddr;
    void Append() 
    {
        reinterpret_cast<void(__thiscall*)(CBaseDC*)>(AppendAddr)(this);
    }

    static inline void* operator_newAddr;
    void* operator new(std::size_t size) 
    {
        return reinterpret_cast<void*(__cdecl*)(std::size_t, int32_t)>(operator_newAddr)(size, 0);
    }

    virtual ~CBaseDC() {}
    virtual void DrawCommand() {}
    virtual int32_t GetSize() { return sizeof(CBaseDC); }
};

export class T_CB_Generic_NoArgs : public CBaseDC 
{
public:
    void (*cb)();

public:
    T_CB_Generic_NoArgs(void (*c)()) : CBaseDC()
    {
        cb = c;
    }

    void DrawCommand() override
    {
        cb();
    }

    int32_t GetSize() override 
    {
        return sizeof(T_CB_Generic_NoArgs);
    }
};

class RenderThread 
{
public:
    RenderThread()
    {
        auto pattern = find_pattern("56 57 8B F9 8B 07 FF 50 08 25");
        if (!pattern.empty())
            CBaseDC::AppendAddr = pattern.get_first(0);

        pattern = find_pattern("53 56 57 8B 7C 24 10 FF 74 24 14");
        if (!pattern.empty())
            CBaseDC::operator_newAddr = pattern.get_first(0);
    }
} RenderThread;