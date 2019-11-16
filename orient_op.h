#pragma once

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

    [[nodiscard]] bool is_portrait() const { return rotate % 180 == 90; }

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
