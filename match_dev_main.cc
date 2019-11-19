#include <CImg.h>
#include <boost/program_options.hpp>
#include <iostream>

#include "render.h"
#include "settings.h"
#include "temp_directory.h"

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

struct histograms {
    cimg_library::CImg<float> r;
    cimg_library::CImg<float> g;
    cimg_library::CImg<float> b;
    cimg_library::CImg<float> s;
    cimg_library::CImg<float> i;

    template <typename T>
    histograms(cimg_library::CImg<T> const& rgb, int level_count, float blur_sigma) {
        if (rgb.spectrum() != 3) throw std::runtime_error("Image is not RGB");
        auto hsi = rgb.get_RGBtoHSI();
        r = rgb.get_shared_channel(0).get_histogram(level_count);
        r.blur(blur_sigma, false, true);
        g = rgb.get_shared_channel(1).get_histogram(level_count);
        g.blur(blur_sigma, false, true);
        b = rgb.get_shared_channel(2).get_histogram(level_count);
        b.blur(blur_sigma, false, true);
        s = hsi.get_shared_channel(1).get_histogram(level_count);
        s.blur(blur_sigma, false, true);
        i = hsi.get_shared_channel(2).get_histogram(level_count);
        i.blur(blur_sigma, false, true);
    };

    [[nodiscard]] double error(histograms const& other, double wr, double wg, double wb, double ws, double wi) const {
        return wr * r.MSE(other.r) + wg * g.MSE(other.g) + wb * b.MSE(other.b) + ws * s.MSE(other.s) +
               wi * i.MSE(other.i);
    }
};

int main(int argc, char* argv[]) {
    temp_directory temp;
    auto options = parse_options(argc, argv);
    cimg_library::CImg<float> target(options.target_path.c_str());
    histograms target_histograms(target, 256, 16);
    settings_t settings;
    settings.load(options.image_path);
    auto rendered = render(options.image_path, settings, temp);
    histograms rendered_histograms(rendered, 256, 16);
    std::cerr << "Rendered : " << target_histograms.error(rendered_histograms, 1, 1, 1, 2, 2) << std::endl;
}