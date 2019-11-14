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
        switch (x.typeId()) {
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

    template <typename T>
    [[nodiscard]] std::optional<T> get(std::string const& key) const {
        std::optional<T> result;
        if (sidecar_) {
            auto i = sidecar_->xmpData().findKey(Exiv2::XmpKey{key});
            if (i != sidecar_->xmpData().end()) result = get_value<T>(i->value());
            if (result) return result;
        }
        auto i = image_->xmpData().findKey(Exiv2::XmpKey{key});
        if (i != image_->xmpData().end()) result = get_value<T>(i->value());
        return result;
    }

    friend std::ostream& operator<<(std::ostream& s, metadata_t const& m) {
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

template <typename T>
bool import_simple(metadata_t const& metadata,
                   std::string const& xmp_key,
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

template <typename TSource, typename TTarget>
bool import_convert(metadata_t const& metadata,
                    std::string const& xmp_key,
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

    return lnrg_to_rt_tint(lr_tint_to_lnrg(x) + lnrg_factor);
}

void import(metadata_t const& metadata, settings_t& settings) {
    // IPTC Metadata
    import_simple<std::string>(metadata, "Xmp.dc.description", settings, "IPTC", "Caption");
    import_simple<std::string>(metadata, "Xmp.dc.rights", settings, "IPTC", "Copyright");
    import_simple<std::string>(metadata, "Xmp.dc.creator", settings, "IPTC", "Creator");
    import_simple<std::string>(metadata, "Xmp.dc.title", settings, "IPTC", "Title");
    if (!import_simple<std::vector<std::string>>(metadata, "Xmp.lr.hierarchicalSubject", settings, "IPTC", "Keywords"))
        import_simple<std::vector<std::string>>(metadata, "Xmp.dc.subject", settings, "IPTC", "Keywords");

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

    // TODO
}

void process_file(boost::filesystem::path const& path) {
    source_file_t source{path};
    if (source.is_xmp()) return;
    auto metadata = source.load_metadata();
    if (!metadata) return;
    //    if (!metadata->is_lightroom()) return;
    std::cerr << *metadata;
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