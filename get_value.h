#pragma once

#include <boost/algorithm/string.hpp>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "exiv2.h"

namespace detail {

template <typename T, typename = void>
struct get_value_impl {};

template <>
struct get_value_impl<bool> {
    static std::optional<bool> impl(Exiv2::Value const& x) {
        switch (x.typeId()) {
            case Exiv2::TypeId::unsignedByte:
            case Exiv2::TypeId::unsignedShort:
            case Exiv2::TypeId::unsignedLong:
            case Exiv2::TypeId::signedByte:
            case Exiv2::TypeId::signedShort:
            case Exiv2::TypeId::signedLong:
            case Exiv2::TypeId::unsignedLongLong:
            case Exiv2::TypeId::signedLongLong:
            case Exiv2::TypeId::tiffIfd8:
                return x.toLong();

            case Exiv2::TypeId::asciiString:
            case Exiv2::TypeId::string:
            case Exiv2::TypeId::xmpText: {
                auto s = x.toString();
                if (boost::iequals(s, "true")) return true;
                if (boost::iequals(s, "yes")) return true;
                if (boost::iequals(s, "on")) return true;
                if (boost::iequals(s, "1")) return true;
                if (boost::iequals(s, "false")) return false;
                if (boost::iequals(s, "no")) return false;
                if (boost::iequals(s, "off")) return false;
                if (boost::iequals(s, "0")) return false;
                return std::nullopt;
            }
            default:
                throw std::logic_error("not implemented");
        }
    }
};

template <typename T>
struct get_value_impl<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    static std::optional<T> impl(Exiv2::Value const& x) {
        auto type_id = x.typeId();
        switch (type_id) {
            case Exiv2::TypeId::unsignedByte:
            case Exiv2::TypeId::unsignedShort:
            case Exiv2::TypeId::unsignedLong:
            case Exiv2::TypeId::signedByte:
            case Exiv2::TypeId::signedShort:
            case Exiv2::TypeId::signedLong:
            case Exiv2::TypeId::unsignedLongLong:
            case Exiv2::TypeId::signedLongLong:
            case Exiv2::TypeId::tiffIfd8:
                return x.toLong();

            case Exiv2::TypeId::unsignedRational:
            case Exiv2::TypeId::signedRational:
            case Exiv2::TypeId::tiffFloat:
            case Exiv2::TypeId::tiffDouble:
                return x.toFloat();

            case Exiv2::TypeId::asciiString:
            case Exiv2::TypeId::string:
            case Exiv2::TypeId::xmpText: {
                std::istringstream i{x.toString()};
                T value;
                i >> value;
                return value;
            }
            default:
                throw std::logic_error("not implemented");
        }
    }
};

template <>
struct get_value_impl<std::string> {
    static std::optional<std::string> impl(Exiv2::Value const& x) {
        switch (x.typeId()) {
            case Exiv2::TypeId::asciiString:
            case Exiv2::TypeId::string:
            case Exiv2::TypeId::xmpText:
                return x.toString();
            case Exiv2::TypeId::langAlt:
                return x.toString(1);
            case Exiv2::TypeId::xmpBag:
            case Exiv2::TypeId::xmpSeq: {
                static std::string const delimiter = "\n";
                std::ostringstream o;
                bool any = false;
                for (long i = 0; i < x.count(); ++i) {
                    if (any) o << delimiter;
                    o << x.toString(i);
                    any = true;
                }
                return o.str();
            }
            default:
                throw std::logic_error("not implemented");
        }
    }
};

template <>
struct get_value_impl<std::vector<std::string>> {
    static std::optional<std::vector<std::string>> impl(Exiv2::Value const& x) {
        switch (x.typeId()) {
            case Exiv2::TypeId::asciiString:
            case Exiv2::TypeId::string:
            case Exiv2::TypeId::xmpText:
            case Exiv2::TypeId::xmpAlt:
            case Exiv2::TypeId::xmpBag:
            case Exiv2::TypeId::xmpSeq:
            case Exiv2::TypeId::langAlt: {
                std::vector<std::string> values;
                for (long i = 0; i < x.count(); ++i) {
                    auto value = x.toString(i);
                    values.push_back(std::move(value));
                }
                return values;
            }
            default:
                throw std::logic_error("not implemented");
        }
    }
};

}  // namespace detail

template <typename T>
std::optional<T> get_value(Exiv2::Value const& x) {
    return detail::get_value_impl<T>::impl(x);
}
