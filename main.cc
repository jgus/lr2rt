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

std::string to_string(Exiv2::TypeId x) {
    switch (x) {
        case Exiv2::TypeId::xmpText:
            return "xmpText";
        case Exiv2::TypeId::xmpAlt:
            return "xmpAlt";
        case Exiv2::TypeId::xmpBag:
            return "xmpBag";
        case Exiv2::TypeId::xmpSeq:
            return "xmpSeq";
        case Exiv2::TypeId::langAlt:
            return "langAlt";
        default: {
            std::ostringstream s;
            s << x;
            return s.str();
        }
    }
}

namespace detail {

template <typename T, typename = void>
struct get_value_impl {};

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
    std::optional<T> get(std::string const& key) const {
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

    bool is_lightroom() const {
        auto tool_name = get<std::string>("Xmp.xmp.CreatorTool");
        return tool_name && (boost::icontains(*tool_name, "lightroom") || boost::icontains(*tool_name, "camera raw"));
    }

    friend std::ostream& operator<<(std::ostream& s, metadata_t const& m) {
        s << "XMP from file:" << std::endl;
        for (auto&& datum : m.image_->xmpData()) {
            s << datum.key() << ": " << datum.value().toString() << " (" << to_string(datum.value().typeId()) << ")"
              << std::endl;
        }
        if (m.sidecar_) {
            s << "XMP from sidecar:" << std::endl;
            for (auto&& datum : m.sidecar_->xmpData()) {
                s << datum.key() << ": " << datum.value().toString() << " (" << to_string(datum.value().typeId()) << ")"
                  << std::endl;
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

template <typename T>
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

void import(metadata_t const& metadata, settings_t& settings) {
    import_simple<std::string>(metadata, "Xmp.dc.description", settings, "IPTC", "Caption");
    import_simple<std::string>(metadata, "Xmp.dc.rights", settings, "IPTC", "Copyright");
    import_simple<std::string>(metadata, "Xmp.dc.creator", settings, "IPTC", "Creator");
    import_simple<std::string>(metadata, "Xmp.dc.title", settings, "IPTC", "Title");
    if (!import_simple<std::vector<std::string>>(metadata, "Xmp.lr.hierarchicalSubject", settings, "IPTC", "Keywords"))
        import_simple<std::vector<std::string>>(metadata, "Xmp.dc.subject", settings, "IPTC", "Keywords");

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