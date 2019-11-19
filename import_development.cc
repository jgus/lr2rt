#include "import_development.h"

#include <cmath>

#include "import.h"
#include "interpolate.h"

namespace {

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
    static float const lnrg_factor = 0.0681933581f - 0.1427758973f;

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

}  // namespace

void import_development(metadata_t const& metadata, settings_t& settings) {
    using namespace std::string_literals;

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
}
