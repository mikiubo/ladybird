/*
 * Copyright (c) 2022, Andreas Kling <andreas@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Layout/CanvasBox.h>
#include <LibWeb/Painting/PaintableBox.h>

namespace Web::Painting {

class CanvasPaintable final : public PaintableBox {
    GC_CELL(CanvasPaintable, PaintableBox);
    GC_DECLARE_ALLOCATOR(CanvasPaintable);

public:
    static GC::Ref<CanvasPaintable> create(Layout::CanvasBox const&);

    virtual void paint(DisplayListRecordingContext&, PaintPhase) const override;

    Layout::CanvasBox const& layout_box() const;

private:
    CanvasPaintable(Layout::CanvasBox const&);
};

}
