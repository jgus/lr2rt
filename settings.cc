#include "settings.h"

#include <regex>

namespace {

std::regex const category_regex{"^\\[(.*)\\]$"};
std::regex const value_regex{"^(.*)=(.*)$"};

auto pp3_path(const boost::filesystem::path& image_path) {
    auto pp3_path = image_path;
    auto new_ext = pp3_path.extension().string() + ".pp3";
    pp3_path.replace_extension(new_ext);
    return pp3_path;
}

}  // namespace

void settings_t::load(const boost::filesystem::path& image_path) {
    boost::filesystem::ifstream i{pp3_path(image_path)};
    if (!i.is_open()) return;
    std::string category;
    std::string line;
    while (std::getline(i, line)) {
        std::smatch match;
        if (std::regex_match(line, match, category_regex)) {
            category = match[1];
        } else if (std::regex_match(line, match, value_regex)) {
            settings_[category][match[1]] = match[2];
        }
    }
}

void settings_t::commit(const boost::filesystem::path& image_path) const {
    boost::filesystem::ofstream o{pp3_path(image_path)};
    for (auto&& [category_name, category_contents] : settings_) {
        o << "[" << category_name << "]" << std::endl;
        for (auto&& [key, value] : category_contents) o << key << "=" << value << std::endl;
        o << std::endl;
    }
}
