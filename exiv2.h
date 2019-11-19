#pragma once

#include <memory>

#if TARGET_OS_IS_APPLE
namespace std {
template <typename T>
using auto_ptr = unique_ptr<T>;
}
#endif

#include <exiv2/exiv2.hpp>
