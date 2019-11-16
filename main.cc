#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include "import_crop.h"
#include "import_development.h"
#include "import_tags.h"
#include "metadata.h"
#include "settings.h"

struct options_t {
    std::vector<std::string> inputs;
    bool force = false;
};

auto parse_options(int argc, char* const* argv) {
    options_t options;

    boost::program_options::options_description o;
    // clang-format off
    o.add_options()
    ("help", "show this help message")
    ("input,i", boost::program_options::value(&options.inputs)->required(), "input file or directory")
    ("force,f", boost::program_options::bool_switch(&options.force), "force processing, even if the file isn't marked as a lightroom file")
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

void import(metadata_t const& metadata, settings_t& settings) {
    import_tags(metadata, settings);
    import_development(metadata, settings);
    import_crop(metadata, settings);
}

void process_file(boost::filesystem::path const& path, bool force) {
    source_file_t source{path};
    if (source.is_xmp()) return;
    auto metadata = source.load_metadata();
    if (!metadata) return;
    if (!metadata->is_lightroom() && !force) {
        std::cerr << path << " does not appear to be a lightroom file; skipping" << std::endl;
        return;
    }
    // std::cerr << *metadata;
    settings_t settings;
    settings.load(path);
    import(*metadata, settings);
    if (!settings.empty()) settings.commit(path);
}

void process_directory(boost::filesystem::path const& path, bool force) {
    for (boost::filesystem::recursive_directory_iterator i{path};
         i != boost::filesystem::recursive_directory_iterator{};
         ++i)
        process_file(*i, force);
}

int main(int argc, char* argv[]) {
    auto options = parse_options(argc, argv);
    for (auto&& input : options.inputs) {
        try {
            auto path = boost::filesystem::canonical(input);
            if (boost::filesystem::is_directory(path))
                process_directory(path, options.force);
            else
                process_file(path, options.force);
        } catch (boost::filesystem::filesystem_error const& e) {
            std::cerr << "Couldn't find " << input << ": " << e.what() << std::endl;
        }
    }
}