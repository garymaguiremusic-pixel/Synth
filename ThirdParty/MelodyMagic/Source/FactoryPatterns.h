#pragma once

#include "mm/Pattern.h"
#include <map>
#include <string>

/** Built-in patterns (original material), shown under "Factory" in the library. */
namespace mmp
{
    const std::map<std::string, mm::Pattern>& factoryPatterns();
    mm::Pattern defaultPattern();
}
