#pragma once

#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace detail {

using namespace std::string_literals;

template <typename T, typename = void>
struct to_setting_string_impl {};

template <>
struct to_setting_string_impl<std::string> {
    static auto impl(std::string const& value) { return value; }
};

template <>
struct to_setting_string_impl<bool> {
    static auto impl(bool value) { return value ? "true"s : "false"s; }
};

template <typename T>
struct to_setting_string_impl<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    static auto impl(T value) {
        std::ostringstream o;
        o << value;
        return o.str();
    }
};

template <typename T>
struct to_setting_string_impl<std::vector<T>> {
    static auto impl(std::vector<T> const& value) {
        std::ostringstream s;
        bool any = false;
        for (auto&& x : value) {
            if (any) s << ';';
            s << x;
            any = true;
        }
        return s.str();
    }
};

}  // namespace detail

template <typename T>
auto to_setting_string(T const& value) {
    return detail::to_setting_string_impl<T>::impl(value);
}
