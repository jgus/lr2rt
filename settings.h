#pragma once

#include <boost/filesystem.hpp>

#include "to_setting.h"

class settings_t {
   public:
    template <typename T>
    void set(std::string const& category, std::string const& key, T const& value) {
        settings_[category][key] = to_setting_string<T>(value);
    }

    void commit(boost::filesystem::path const& image_path) const;

   private:
    std::map<std::string, std::map<std::string, std::string>> settings_;
};
