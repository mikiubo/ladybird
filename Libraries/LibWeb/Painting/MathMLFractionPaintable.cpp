/*
 * Copyright (c) 2026, mikiubo <michele.uboldi@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Layout/MathMLFractionBox.h>
#include <LibWeb/Painting/DisplayListRecorder.h>
#include <LibWeb/Painting/MathMLFractionPaintable.h>

namespace Web::Painting {

NonnullRefPtr<MathMLFractionPaintable> MathMLFractionPaintable::create(Layout::MathMLFractionBox const& layout_box)
{
    return adopt_ref(*new MathMLFractionPaintable(layout_box));
}

MathMLFractionPaintable::MathMLFractionPaintable(Layout::MathMLFractionBox const& layout_box)
    : PaintableBox(layout_box)
{
}

Layout::MathMLFractionBox const& MathMLFractionPaintable::layout_box() const
{
    return static_cast<Layout::MathMLFractionBox const&>(layout_node());
}

void MathMLFractionPaintable::paint(DisplayListRecordingContext& context, PaintPhase phase) const
{
    PaintableBox::paint(context, phase);

    if (!is_visible())
        return;
    if (phase != PaintPhase::Foreground)
        return;

    // https://w3c.github.io/mathml-core/#fraction-with-nonzero-line-thickness
    // The fraction bar must only be painted if the visibility of the <mfrac> element is `visible`.
    // In that case, the fraction bar must be painted with the color of the <mfrac> element.
    auto const& bar_rect = layout_box().fraction_bar_rect();
    if (bar_rect.is_empty())
        return;

    auto color = computed_values().color();
    auto absolute_bar_rect = bar_rect.translated(absolute_rect().location());
    auto device_rect = context.enclosing_device_rect(absolute_bar_rect);
    context.display_list_recorder().fill_rect(device_rect.to_type<int>(), color);
}

}
