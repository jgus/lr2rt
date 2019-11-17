#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/extension/io/tiff.hpp>
#include <boost/gil/io/io.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include "settings.h"

struct options_t {
    std::string image_path;
    std::string target_path;
};

auto parse_options(int argc, char* const* argv) {
    options_t options;

    boost::program_options::options_description o;
    // clang-format off
    o.add_options()
    ("help", "show this help message")
    ("image,i", boost::program_options::value(&options.image_path)->required(), "image file")
    ("target,t", boost::program_options::value(&options.target_path)->required(), "image file with target development")
    ;
    // clang-format on

    boost::program_options::variables_map v;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(o).run(), v);
    boost::program_options::notify(v);

    if (v.count("help")) {
        std::cerr << o << std::endl;
        exit(1);
    }

    return options;
}

std::optional<boost::gil::rgb8_image_t> read_image(std::string const& path) {
    std::ifstream image_stream(path, std::ios::binary);
    if (!image_stream.is_open()) return std::nullopt;
    boost::gil::rgb8_image_t image;
    try {
        boost::gil::read_image(image_stream, image, boost::gil::image_read_settings<boost::gil::jpeg_tag>{});
        return image;
    } catch (std::ios_base::failure const&) {
    }
    try {
        boost::gil::read_image(image_stream, image, boost::gil::image_read_settings<boost::gil::tiff_tag>{});
        return image;
    } catch (std::ios_base::failure const&) {
    }
    try {
        boost::gil::read_image(image_stream, image, boost::gil::image_read_settings<boost::gil::png_tag>{});
        return image;
    } catch (std::ios_base::failure const&) {
    }
    return std::nullopt;
}

struct histogram_t {
    std::vector<double> r;
    std::vector<double> g;
    std::vector<double> b;
};

histogram_t get_histogram(boost::gil::rgb8_image_t const& image) {
    histogram_t result;
    result.r.resize(256, 0.0);
    result.g.resize(256, 0.0);
    result.b.resize(256, 0.0);
    size_t count = 0;
    for (auto&& p : boost::gil::const_view(image)) {
        result.r[p[0]] += 1;
        result.g[p[1]] += 1;
        result.b[p[2]] += 1;
        ++count;
    }
    if (count > 0) {
        for (auto& x : result.r) x *= 256.0 / count;
        for (auto& x : result.g) x *= 256.0 / count;
        for (auto& x : result.b) x *= 256.0 / count;
    }
    return result;
};

int main(int argc, char* argv[]) {
    auto options = parse_options(argc, argv);
    auto image = read_image(options.image_path);
    if(!image) exit(1);
    auto histogram = get_histogram(*image);
}