#include "import_tags.h"

#include "import.h"

namespace {

std::optional<int> convert_color_label(std::string const& x) {
    if (boost::iequals(x, "red")) return 1;
    if (boost::iequals(x, "yellow")) return 2;
    if (boost::iequals(x, "green")) return 3;
    if (boost::iequals(x, "blue")) return 4;
    if (boost::iequals(x, "purple")) return 5;
    return std::nullopt;
}

}  // namespace

void import_tags(metadata_t const& metadata, settings_t& settings) {
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
}
