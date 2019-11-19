#pragma once

#include <CImg.h>
#include <boost/filesystem.hpp>

#include "settings.h"

cimg_library::CImg<uint8_t> render(boost::filesystem::path const& image_path,
                                   settings_t const& settings,
                                   boost::filesystem::path const& working_directory);