//
// Created by josh on 11/12/19.
//
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <exiv2/exiv2.hpp>
#include <iostream>
#include <optional>
#include <utility>

#include "interpolate.h"

using namespace std::string_literals;

struct options_t {
    std::vector<std::string> inputs;
};

auto parse_options(int argc, char* const* argv) {
    options_t options;

    boost::program_options::options_description o;
    // clang-format off
    o.add_options()
    ("help", "show this help message")
    ("input,i", boost::program_options::value(&options.inputs)->required(), "input file or directory")
    ;
    // clang-format on
    boost::program_options::positional_options_description p;
    p.add("input", -1);

    boost::program_options::variables_map v;
    boost::program_options::store(
        boost::program_options::command_line_parser(argc, argv).options(o).positional(p).run(), v);
    boost::program_options::notify(v);

    if (v.count("help")) {
        std::cerr << o << std::endl;
        exit(1);
    }

    return options;
}

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

class metadata_t {
   public:
    explicit metadata_t(boost::filesystem::path const& path) {
        image_ = Exiv2::ImageFactory::open(path.string());
        assert(image_);
        image_->readMetadata();
        std::cerr << "Read metadata from " << path << std::endl;
        auto sidecar_path = path;
        sidecar_path.replace_extension(".xmp");
        if (boost::filesystem::is_regular_file(sidecar_path)) {
            try {
                sidecar_ = Exiv2::ImageFactory::open(sidecar_path.string());
            } catch (Exiv2::AnyError const&) {
            }
        }
        if (sidecar_) {
            sidecar_->readMetadata();
            std::cerr << "Read metadata from sidecar " << sidecar_path << std::endl;
        }
    }

    [[nodiscard]] auto width() const { return image_->pixelWidth(); }
    [[nodiscard]] auto height() const { return image_->pixelHeight(); }

    template <typename T>
    [[nodiscard]] std::optional<T> get(std::vector<std::string> const& keys) const {
        std::optional<T> result;
        if (sidecar_) {
            for (auto&& key : keys) {
                if (boost::starts_with(key, "Xmp.")) {
                    auto i = sidecar_->xmpData().findKey(Exiv2::XmpKey{key});
                    if (i != sidecar_->xmpData().end()) result = get_value<T>(i->value());
                    if (result) return result;
                }
            }
        }
        for (auto&& key : keys) {
            if (boost::starts_with(key, "Xmp.")) {
                auto i = image_->xmpData().findKey(Exiv2::XmpKey{key});
                if (i != image_->xmpData().end()) result = get_value<T>(i->value());
                if (result) return result;
            } else if (boost::starts_with(key, "Exif.")) {
                auto i = image_->exifData().findKey(Exiv2::ExifKey{key});
                if (i != image_->exifData().end()) result = get_value<T>(i->value());
                if (result) return result;
            }
        }
        return std::nullopt;
    }

    template <typename T>
    [[nodiscard]] std::optional<T> get(std::string const& key) const {
        return get<T>(std::vector{key});
    }

    friend std::ostream& operator<<(std::ostream& s, metadata_t const& m) {
        s << "WxH: " << m.width() << "x" << m.height() << std::endl;
        s << "EXIF from file:" << std::endl;
        for (auto&& datum : m.image_->exifData()) {
            s << datum.key() << ": " << datum.value().toString() << " ("
              << Exiv2::TypeInfo::typeName(datum.value().typeId()) << ")" << std::endl;
        }
        s << "XMP from file:" << std::endl;
        for (auto&& datum : m.image_->xmpData()) {
            s << datum.key() << ": " << datum.value().toString() << " ("
              << Exiv2::TypeInfo::typeName(datum.value().typeId()) << ")" << std::endl;
        }
        if (m.sidecar_) {
            s << "XMP from sidecar:" << std::endl;
            for (auto&& datum : m.sidecar_->xmpData()) {
                s << datum.key() << ": " << datum.value().toString() << " ("
                  << Exiv2::TypeInfo::typeName(datum.value().typeId()) << ")" << std::endl;
            }
        }
        return s;
    }

   private:
    std::unique_ptr<Exiv2::Image> image_;
    std::unique_ptr<Exiv2::Image> sidecar_;
};

class source_file_t {
   public:
    explicit source_file_t(boost::filesystem::path path) : path_{std::move(path)} {}

    bool is_xmp() const { return boost::iequals(path_.extension().string(), ".xmp"); }

    std::unique_ptr<metadata_t> load_metadata() {
        try {
            return std::make_unique<metadata_t>(path_);
        } catch (Exiv2::AnyError const&) {
            return nullptr;
        }
    }

   private:
    boost::filesystem::path path_;
};

namespace detail {

template <typename T, typename = void>
struct to_setting_string_impl {};

template <>
struct to_setting_string_impl<std::string> {
    static auto impl(std::string const& value) { return value; }
};

template <>
struct to_setting_string_impl<bool> {
    static auto impl(bool value) { return value ? "true"s : "false"s; }
};

template <typename T>
struct to_setting_string_impl<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    static auto impl(T value) {
        std::ostringstream o;
        o << value;
        return o.str();
    }
};

template <typename T>
struct to_setting_string_impl<std::vector<T>> {
    static auto impl(std::vector<T> const& value) {
        std::ostringstream s;
        bool any = false;
        for (auto&& x : value) {
            if (any) s << ';';
            s << x;
            any = true;
        }
        return s.str();
    }
};

}  // namespace detail

template <typename T>
auto to_setting_string(T const& value) {
    return detail::to_setting_string_impl<T>::impl(value);
}

class settings_t {
   public:
    template <typename T>
    void set(std::string const& category, std::string const& key, T const& value) {
        settings_[category][key] = to_setting_string<T>(value);
    }

    void commit(boost::filesystem::path const& image_path) const {
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

   private:
    std::map<std::string, std::map<std::string, std::string>> settings_;
};

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

std::optional<int> convert_color_label(std::string const& x) {
    if (boost::iequals(x, "red")) return 1;
    if (boost::iequals(x, "yellow")) return 2;
    if (boost::iequals(x, "green")) return 3;
    if (boost::iequals(x, "blue")) return 4;
    if (boost::iequals(x, "purple")) return 5;
    return std::nullopt;
}

std::optional<float> convert_tint(int x) {
    static Interpolator const lr_tint_to_lnrg{{
        {150, 0.4472347561f},
        {120, 0.3614738364f},
        {90, 0.2902822029f},
        {60, 0.2320119442f},
        {30, 0.1789069729f},
        {20, 0.1613647324f},
        {10, 0.1427758973f},
        {0, 0.1248534423f},
        {-10, 0.1036493101f},
        {-20, 0.08258857431f},
        {-30, 0.0629298779f},
        {-60, -0.002147326005f},
        {-90, -0.06668486396f},
        {-120, -0.125869729f},
        {-150, -0.1808872982f},
    }};
    static Interpolator const lnrg_to_rt_tint{{
        {0.8171373437, 0.19562},   {0.7811030438, 0.21224},  {0.7450388954, 0.23028},    {0.7087309148, 0.24986},
        {0.6720998692, 0.27110},   {0.6351289632, 0.29414},  {0.5979481717, 0.31914},    {0.5606603801, 0.34627},
        {0.523284775, 0.37570},    {0.4858030276, 0.40764},  {0.448275121, 0.44229},     {0.4107057305, 0.47988},
        {0.3730293203, 0.52067},   {0.3351195742, 0.56493},  {0.2970600413, 0.61295},    {0.2589332983, 0.66505},
        {0.2207358646, 0.72157},   {0.1824888955, 0.78291},  {0.1442752227, 0.84946},    {0.1061831113, 0.92166},
        {0.0681933581, 1.00000},   {0.03029290848, 1.08500}, {-0.007514875596, 1.17722}, {-0.04525204435, 1.27729},
        {-0.08291626329, 1.38586}, {-0.1205209756, 1.50366}, {-0.1580656479, 1.63147},   {-0.195191121, 1.77014},
        {-0.2319562788, 1.92060},  {-0.2683265738, 2.08386}, {-0.3042466894, 2.26098},   {-0.3397271162, 2.45317},
        {-0.3747203568, 2.66169},  {-0.4091412238, 2.88793}, {-0.4430431349, 3.13340},   {-0.4764123107, 3.39974},
        {-0.5092487958, 3.68872},  {-0.541583611, 4.00226},  {-0.5733997287, 4.34245},   {-0.604695869, 4.71156},
        {-0.6354722459, 5.11205},
    }};
    static float const lnrg_factor = 0.0681933581 - 0.1427758973f;

    return lnrg_to_rt_tint(lr_tint_to_lnrg(float(x)) + lnrg_factor);
}

std::optional<int> convert_contrast(int x) {
    static Interpolator const lr_to_std{{
        {100, 0.36458},  {80, 0.353671},  {60, 0.341999},  {40, 0.329606},   {30, 0.323169},
        {20, 0.3166},    {15, 0.313276},  {10, 0.309932},  {5, 0.306573},    {0, 0.303206},
        {-5, 0.300125},  {-10, 0.297033}, {-15, 0.293934}, {-20, 0.290831},  {-30, 0.284629},
        {-40, 0.278439}, {-60, 0.266103}, {-80, 0.253801}, {-100, 0.241532},
    }};
    static Interpolator const std_to_rt{{
        {0.0004, -100}, {0.0267, -90}, {0.0540, -80}, {0.0823, -70}, {0.1117, -60}, {0.1425, -50}, {0.1750, -40},
        {0.2098, -30},  {0.2477, -20}, {0.2900, -10}, {0.3400, 0},   {0.3888, 10},  {0.4420, 20},  {0.5011, 30},
        {0.5683, 40},   {0.6454, 50},  {0.7325, 60},  {0.8280, 70},  {0.9291, 80},  {1.0282, 90},  {1.1115, 100},
    }};

    return int(std::round(std_to_rt(lr_to_std(float(x)))));
}

std::optional<int> convert_saturation(int x) {
    static Interpolator const lr_to_sat{{
        {100, 0.66407400}, {80, 0.63174100},  {60, 0.58658300},  {40, 0.52428900},   {30, 0.48529900},
        {20, 0.44199100},  {15, 0.41848600},  {10, 0.39317300},  {5, 0.36691800},    {0, 0.33974700},
        {-5, 0.31369300},  {-10, 0.29088400}, {-15, 0.27016500}, {-20, 0.25119000},  {-30, 0.21557400},
        {-40, 0.18172600}, {-60, 0.11832800}, {-80, 0.05678380}, {-100, 0.00000000},
    }};
    static Interpolator const sat_to_rt{{
        {0.00036138, -100}, {0.02665059, -90}, {0.05398929, -80}, {0.08223383, -70}, {0.11160132, -60},
        {0.14236492, -50},  {0.17488792, -40}, {0.20968295, -30}, {0.24753367, -20}, {0.28982319, -10},
        {0.33974700, 0},    {0.38854096, 10},  {0.44163229, 20},  {0.50069355, 30},  {0.56787097, 40},
        {0.64492921, 50},   {0.73193242, 60},  {0.82738070, 70},  {0.92841901, 80},  {1.02739169, 90},
        {1.11065176, 100},
    }};

    return int(std::round(sat_to_rt(lr_to_sat(float(x)))));
}

std::optional<int> convert_higlights(int x) {
    static Interpolator const lr_to_m{{
        {-100, 31097.3},
        {-80, 31555.1},
        {-60, 31975.7},
        {-40, 32360},
        {0, 33023.3},
    }};
    static Interpolator const m_to_rt{{
        {33023.3, 0},
        {32539.50801, 10},
        {32052.71843, 20},
        {31562.93126, 30},
        {31070.41902, 40},
        {30575.45421, 50},
        {30078.85436, 60},
        {29581.52781, 70},
        {29085.38214, 80},
        {28662.54103, 90},
        {28105.26279, 100},
    }};

    return int(std::round(m_to_rt(lr_to_m(float(x)))));
}

std::optional<int> convert_shadows(int x) {
    static Interpolator const lr_to_m{{
        {0, 33023.3},
        {20, 34027.1},
        {40, 35070.3},
        {60, 36149.4},
        {80, 37262.2},
    }};
    static Interpolator const m_to_rt{{
        {33023.3, 0},
        {34187.18037, 10},
        {35490.22133, 20},
        {36929.33446, 30},
        {38492.98362, 40},
        {40160.73071, 50},
        {41905.68832, 60},
        {43699.4248, 70},
        {45515.50692, 80},
        {47330.40817, 90},
        {49125.41635, 100},
    }};

    return int(std::round(m_to_rt(lr_to_m(float(x)))));
}

struct orient_op_t {
    bool horizontal_flip = false;
    int rotate = 0;

    static orient_op_t from_tiff(int x) {
        switch (x) {
            case 1:  // ORIENTATION_TOPLEFT
            default:
                return {false, 0};
            case 2:  // ORIENTATION_TOPRIGHT
                return {true, 0};
            case 3:  // ORIENTATION_BOTRIGHT
                return {false, 180};
            case 4:  // ORIENTATION_BOTLEFT
                return {true, 180};
            case 5:  // ORIENTATION_LEFTTOP
                return {true, 90};
            case 6:  // ORIENTATION_RIGHTTOP
                return {false, 90};
            case 7:  // ORIENTATION_RIGHTBOT
                return {true, 270};
            case 8:  // ORIENTATION_LEFTBOT
                return {false, 270};
        }
    }

    bool is_portrait() const { return rotate % 180 == 90; }

    [[nodiscard]] orient_op_t append_horizontal_flip(bool x) const {
        return x ? orient_op_t{!horizontal_flip, (360 - rotate) % 360} : *this;
    }

    [[nodiscard]] orient_op_t append_rotate(int x) const { return {horizontal_flip, (rotate + x) % 360}; }

    [[nodiscard]] orient_op_t compose(orient_op_t const& b) const {
        return append_horizontal_flip(b.horizontal_flip).append_rotate(b.rotate);
    }

    friend orient_op_t operator+(orient_op_t const& a, orient_op_t const& b) { return a.compose(b); }

    [[nodiscard]] orient_op_t invert() const {
        return orient_op_t{}.append_rotate(360 - rotate).append_horizontal_flip(horizontal_flip);
    }

    friend orient_op_t operator-(orient_op_t const& x) { return x.invert(); }
};

std::tuple<float, float> rotate(std::tuple<float, float> p, float r) {
    return {std::get<0>(p) * std::cos(r) + std::get<1>(p) * std::sin(r),
            -std::get<0>(p) * std::sin(r) + std::get<1>(p) * std::cos(r)};
}

void import_crop(metadata_t const& metadata, settings_t& settings) {
    if (!metadata.get<bool>("Xmp.crs.HasCrop").value_or(false)) return;

    // cap + user = final
    // -cap + cap + user = -cap + final
    // user = -cap + final
    auto final_orientation = orient_op_t::from_tiff(metadata.get<int>("Xmp.tiff.Orientation").value_or(1));
    auto capture_orientation = orient_op_t::from_tiff(metadata.get<int>("Exif.Image.Orientation").value_or(1));
    auto user_orient_op = -capture_orientation + final_orientation;

    auto sensor_width = metadata.width();
    auto sensor_height = metadata.height();
    auto image_width = metadata.get<int>("Xmp.tiff.ImageWidth").value_or(sensor_width);
    auto image_height = metadata.get<int>("Xmp.tiff.ImageLength").value_or(sensor_height);
    if (final_orientation.is_portrait()) {
        std::swap(sensor_width, sensor_height);
        std::swap(image_width, image_height);
    }

    auto top = metadata.get<float>("Xmp.crs.CropTop").value_or(0);
    auto left = metadata.get<float>("Xmp.crs.CropLeft").value_or(0);
    auto bottom = metadata.get<float>("Xmp.crs.CropBottom").value_or(1);
    auto right = metadata.get<float>("Xmp.crs.CropRight").value_or(1);
    auto angle = metadata.get<float>("Xmp.crs.CropAngle").value_or(0);
    assert(0 <= top && top <= 1);
    assert(0 <= left && left <= 1);
    assert(0 <= bottom && bottom <= 1);
    assert(0 <= right && right <= 1);
    assert(-45 <= angle && angle <= 45);

    int corner_swaps = 0;
    if (final_orientation.horizontal_flip) {
        auto left_n = 1.0 - right;
        auto right_n = 1.0 - left;
        left = left_n;
        right = right_n;
        assert(left <= right);
        ++corner_swaps;
    }
    for (auto i = 0; i < final_orientation.rotate / 90; ++i) {
        auto top_n = left;
        auto left_n = 1.0 - bottom;
        auto bottom_n = right;
        auto right_n = 1.0 - top;
        top = top_n;
        left = left_n;
        bottom = bottom_n;
        right = right_n;
        assert(left <= right);
        assert(top <= bottom);
        ++corner_swaps;
    }
    corner_swaps = corner_swaps % 2;

    auto top_s = (top - 0.5) * sensor_height;
    auto left_s = (left - 0.5) * sensor_width;
    auto bottom_s = (bottom - 0.5) * sensor_height;
    auto right_s = (right - 0.5) * sensor_width;
    auto angle_r = angle * M_PI / 180;
    assert(std::cos(angle_r) >= 0);
    auto [x0, y0] = (corner_swaps == 0) ? rotate({left_s, top_s}, angle_r) : rotate({right_s, top_s}, angle_r);
    auto [x1, y1] = (corner_swaps == 0) ? rotate({right_s, bottom_s}, angle_r) : rotate({left_s, bottom_s}, angle_r);
    auto top_r = std::min(y0, y1) + 0.5 * image_height;
    auto bottom_r = std::max(y0, y1) + 0.5 * image_height;
    auto left_r = std::min(x0, x1) + 0.5 * image_width;
    auto right_r = std::max(x0, x1) + 0.5 * image_width;

    settings.set("Coarse Transformation", "Rotate", user_orient_op.rotate);
    settings.set("Coarse Transformation", "HorizontalFlip", user_orient_op.horizontal_flip);
    settings.set("Coarse Transformation", "VerticalFlip", false);
    settings.set("Crop", "Enabled", true);
    //        settings.set("Crop", "FixedRatio", true);
    settings.set("Crop", "X", int(round(left_r)));
    settings.set("Crop", "W", int(round(right_r - left_r)));
    settings.set("Crop", "Y", int(round(top_r)));
    settings.set("Crop", "H", int(round(bottom_r - top_r)));
    settings.set("Common Properties for Transformations", "AutoFill", false);
    settings.set("Rotation", "Degree", angle);
}

void import(metadata_t const& metadata, settings_t& settings) {
    // IPTC Metadata
    import_simple<std::string>(metadata, "Xmp.dc.description", settings, "IPTC", "Caption");
    import_simple<std::string>(metadata, "Xmp.dc.rights", settings, "IPTC", "Copyright");
    import_simple<std::string>(metadata, "Xmp.dc.creator", settings, "IPTC", "Creator");
    import_simple<std::string>(metadata, "Xmp.dc.title", settings, "IPTC", "Title");
    import_simple<std::vector<std::string>>(metadata,
                                            std::vector<std::string>{"Xmp.lr.hierarchicalSubject", "Xmp.dc.subject"},
                                            settings,
                                            "IPTC",
                                            "Keywords");

    // Labels
    import_simple<int>(metadata, "Xmp.xmp.Rating", settings, "General", "Rank");
    import_convert<std::string, int>(
        metadata, "Xmp.xmp.Label", &convert_color_label, settings, "General", "ColorLabel");

    // White Balance
    bool has_wb = false;
    has_wb = import_simple<int>(metadata, "Xmp.crs.Temperature", settings, "White Balance", "Temperature") || has_wb;
    has_wb = import_convert<int, float>(metadata, "Xmp.crs.Tint", &convert_tint, settings, "White Balance", "Green") ||
             has_wb;
    if (has_wb) {
        settings.set("White Balance", "Enabled", true);
        settings.set("White Balance", "Setting", "Custom"s);
    }

    // Exposure
    import_simple<float>(metadata,
                         std::vector<std::string>{"Xmp.crs.Exposure2012", "Xmp.crs.Exposure"},
                         settings,
                         "Exposure",
                         "Compensation");
    import_convert<int, int>(metadata,
                             std::vector<std::string>{"Xmp.crs.Contrast2012", "Xmp.crs.Contrast"},
                             &convert_contrast,
                             settings,
                             "Exposure",
                             "Contrast");
    import_convert<int, int>(metadata, "Xmp.crs.Saturation", &convert_saturation, settings, "Exposure", "Saturation");

    // Shadows & Highlights
    bool has_sh = false;
    has_sh = import_convert<int, int>(metadata,
                                      std::vector<std::string>{"Xmp.crs.Highlights2012", "Xmp.crs.Highlights"},
                                      &convert_higlights,
                                      settings,
                                      "Shadows & Highlights",
                                      "Highlights") ||
             has_sh;
    has_sh = import_convert<int, int>(metadata,
                                      std::vector<std::string>{"Xmp.crs.Shadows2012", "Xmp.crs.Shadows"},
                                      &convert_shadows,
                                      settings,
                                      "Shadows & Highlights",
                                      "Shadows") ||
             has_sh;
    if (has_sh) {
        settings.set("Shadows & Highlights", "Enabled", true);
    }

    // Crop
    import_crop(metadata, settings);

    // TODO
}

void process_file(boost::filesystem::path const& path) {
    source_file_t source{path};
    if (source.is_xmp()) return;
    auto metadata = source.load_metadata();
    if (!metadata) return;
    //    if (!metadata->is_lightroom()) return;
    //    std::cerr << *metadata;
    settings_t settings;
    import(*metadata, settings);
    settings.commit(path);
}

void process_directory(boost::filesystem::path const& path) {
    for (boost::filesystem::recursive_directory_iterator i{path};
         i != boost::filesystem::recursive_directory_iterator{};
         ++i)
        process_file(*i);
}

int main(int argc, char* argv[]) {
    auto options = parse_options(argc, argv);
    for (auto&& input : options.inputs) {
        try {
            auto path = boost::filesystem::canonical(input);
            if (boost::filesystem::is_directory(path))
                process_directory(path);
            else
                process_file(path);
        } catch (boost::filesystem::filesystem_error const& e) {
            std::cerr << "Couldn't find " << input << ": " << e.what() << std::endl;
        }
    }
}