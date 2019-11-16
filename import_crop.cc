#include "import_crop.h"

#include <cmath>

#include "orient_op.h"

namespace {

std::tuple<float, float> rotate(std::tuple<float, float> p, float r) {
    return {std::get<0>(p) * std::cos(r) + std::get<1>(p) * std::sin(r),
            -std::get<0>(p) * std::sin(r) + std::get<1>(p) * std::cos(r)};
}

}  // namespace

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
