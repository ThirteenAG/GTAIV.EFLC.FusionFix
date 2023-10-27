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
            // Add your code here
        };
    }
} Contributing;