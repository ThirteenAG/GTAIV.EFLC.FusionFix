module;

#include <common.hxx>

export module contributing;

import common;

class Contributing
{
public:
    Contributing()
    {
        FusionFix::onInitEvent() += []()
        {
            // Restored a small detail regarding pedprops from the console versions that was changed on PC. Regular cops & fat cops will now spawn with their hat prop disabled when in a vehicle.
            {
                auto pattern = find_pattern("3B 05 ? ? ? ? 74 6C 3B 05 ? ? ? ? 74 64 3B 05 ? ? ? ?", "3B 05 ? ? ? ? 74 6E 3B 05 ? ? ? ? 74 66 3B 05 ? ? ? ?");
                injector::MakeNOP(pattern.get_first(0), 16, true);
                injector::MakeNOP(pattern.get_first(24), 8, true);
            }
        };
    }
} Contributing;