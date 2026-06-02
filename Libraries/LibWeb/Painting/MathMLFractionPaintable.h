/*
 * Copyright (c) 2026, mikiubo <michele.uboldi@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Layout/MathMLFractionBox.h>
#include <LibWeb/Painting/PaintableBox.h>

namespace Web::Painting {

class MathMLFractionPaintable final : public PaintableBox {
public:
    static NonnullRefPtr<MathMLFractionPaintable> create(Layout::MathMLFractionBox const&);
    virtual StringView class_name() const override { return "MathMLFractionPaintable"sv; }

    virtual void paint(DisplayListRecordingContext&, PaintPhase) const override;

    Layout::MathMLFractionBox const& layout_box() const;

private:
    MathMLFractionPaintable(Layout::MathMLFractionBox const&);
};

}
