#pragma once

#include <tchar.h>
#include <string>
#include <vector>

namespace ffs {

    using String = std::basic_string<TCHAR>;

    using Strings = std::vector<String>;
}