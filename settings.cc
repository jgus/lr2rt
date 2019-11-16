#include "settings.h"

void settings_t::commit(const boost::filesystem::path& image_path) const {
    auto pp3_path = image_path;
    auto new_ext = pp3_path.extension().string() + ".pp3";
    pp3_path.replace_extension(new_ext);
    boost::filesystem::ofstream o{pp3_path};
    for (auto&& [category_name, category_contents] : settings_) {
        o << "[" << category_name << "]" << std::endl;
        for (auto&& [key, value] : category_contents) o << key << "=" << value << std::endl;
        o << std::endl;
    }
}
