#pragma once

#include <memory>

namespace std {
    template<typename T>
    using auto_ptr = unique_ptr<T>;
}

#include <exiv2/exiv2.hpp>
