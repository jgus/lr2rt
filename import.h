#pragma once

#include "metadata.h"
#include "settings.h"

template <typename T, typename TKey>
bool import_simple(metadata_t const& metadata,
                   TKey const& xmp_key,
                   settings_t& settings,
                   std::string const& settting_category,
                   std::string const& setting_key) {
    auto value = metadata.get<T>(xmp_key);
    if (value) {
        settings.set(settting_category, setting_key, *value);
        return true;
    }
    return false;
}

template <typename TSource, typename TTarget, typename TKey>
bool import_convert(metadata_t const& metadata,
                    TKey const& xmp_key,
                    std::function<std::optional<TTarget>(TSource)> conversion,
                    settings_t& settings,
                    std::string const& settting_category,
                    std::string const& setting_key) {
    auto source = metadata.get<TSource>(xmp_key);
    if (source) {
        auto target = conversion(*source);
        if (target) {
            settings.set(settting_category, setting_key, *target);
            return true;
        }
    }
    return false;
}
