#include "metadata.h"

metadata_t::metadata_t(boost::filesystem::path const& path) {
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
