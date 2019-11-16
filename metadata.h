#pragma once

#include <boost/filesystem.hpp>
#include <exiv2/exiv2.hpp>

#include "get_value.h"

class metadata_t {
   public:
    explicit metadata_t(boost::filesystem::path const& path);

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

    [[nodiscard]] bool is_lightroom() const {
        auto tool_name = get<std::string>("Xmp.xmp.CreatorTool");
        return tool_name && (boost::icontains(*tool_name, "lightroom") || boost::icontains(*tool_name, "camera raw"));
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
