#pragma once

#include <boost/filesystem.hpp>

#include "to_setting.h"

class settings_t {
   public:
    template <typename T>
    void set(std::string const& category, std::string const& key, T const& value) {
        settings_[category][key] = to_setting_string<T>(value);
    }

    [[nodiscard]] bool empty() const { return settings_.empty(); }
    void load(boost::filesystem::path const& image_path);
    void commit_by(boost::filesystem::path const& image_path) const;
    void commit(boost::filesystem::path const& settings_path) const;

   private:
    std::map<std::string, std::map<std::string, std::string>> settings_;
};
