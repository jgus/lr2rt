//
// Created by josh on 11/12/19.
//
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <exiv2/exiv2.hpp>
#include <iostream>
#include <utility>

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

class SourceMetadata {
   public:
    explicit SourceMetadata(boost::filesystem::path const& path) {
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

   private:
    std::unique_ptr<Exiv2::Image> image_;
    std::unique_ptr<Exiv2::Image> sidecar_;
};

class SourceFile {
   public:
    explicit SourceFile(boost::filesystem::path path) : path_{std::move(path)} {}

    bool is_xmp() const { return boost::iequals(path_.extension().string(), ".xmp"); }

    std::unique_ptr<SourceMetadata> load_metadata() {
        try {
            return std::make_unique<SourceMetadata>(path_);
        } catch (Exiv2::AnyError const&) {
            return nullptr;
        }
    }

   private:
    boost::filesystem::path path_;
};

void process_file(boost::filesystem::path const& path) {
    SourceFile source{path};
    if (source.is_xmp()) return;
    auto metadata = source.load_metadata();
    if (!metadata) return;
    // TODO
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