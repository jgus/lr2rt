#include "render.h"

#include <boost/format.hpp>
#include <cstdlib>

cimg_library::CImg<uint8_t> render(boost::filesystem::path const& image_path,
                                   settings_t const& settings,
                                   boost::filesystem::path const& working_directory) {
    auto settings_file = working_directory / "settings.pp3";
    auto rendered_file = working_directory / "rendered.tif";
    settings.commit(settings_file);
    auto command =
        (boost::format("rawtherapee-cli -p %2% -o %3% -t -Y -c %1%") % image_path % settings_file % rendered_file)
            .str();
    std::system(command.c_str());
    cimg_library::CImg<uint8_t> result(rendered_file.c_str());
    boost::filesystem::remove(settings_file);
    boost::filesystem::remove(rendered_file);
    return result;
}