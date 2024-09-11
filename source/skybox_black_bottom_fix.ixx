module;

#include <common.hxx>

export module skybox_black_bottom_fix;

import common;

class skybox_black_bottom_fix
{
public:
    skybox_black_bottom_fix()
    {
        FusionFix::onInitEvent() += []()
        {
            // preCE
          	if ((ver.GetMajorVersion() <= 1 && ver.GetMinorVersion() <= 0 && ver.GetMinorRevisionVersion() <= 8 && ver.GetMajorRevisionVersion() <= 0) ||
          		(ver.GetMajorVersion() <= 1 && ver.GetMinorVersion() <= 1 && ver.GetMinorRevisionVersion() <= 3 && ver.GetMajorRevisionVersion() <= 0)) {
          		p4 = hook::pattern("C7 46 ? ? ? ? ? C7 46 ? ? ? ? ? 5E 59 C3");
          		if (p4.empty()) return;
          		addr = (size_t)p4.get_first(3);
          	}
          	// CE
          	else { 
          		MessageBoxA(NULL, NULL, NULL, 0x10);
          		p4 = hook::pattern("C7 44 24 ? ? ? ? ? C7 44 24 ? ? ? ? ? E8 ? ? ? ? 8B 44 24 44");
          		if (p4.empty()) return;
          		addr = (size_t)p4.get_first(4);
          	}
          
          	DWORD* pU = *(DWORD**)addr;
          	for (size_t i = 0; i < (0x180c / 4) / 3; i++) {
          		float* pVertex = (float*)(pU + i * 3);
          		if (i < 32) {
          			if (pVertex[1] < 0.f) {
          				injector::WriteMemory<float>(pVertex, 0.f);
          				injector::WriteMemory<float>(pVertex + 2, 0.f);
          			}
          		}
          		else injector::WriteMemory<float>(pVertex + 1, (pVertex[1] - 0.16037700f) * 1.78f - 1.f);
          	}
        };
    }
} skybox_black_bottom_fix;
