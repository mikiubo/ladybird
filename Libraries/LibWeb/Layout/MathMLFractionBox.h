/*
 * Copyright (c) 2026, mikiubo <michele.uboldi@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Layout/Box.h>
#include <LibWeb/MathML/MathMLMFracElement.h>
#include <LibWeb/PixelUnits.h>

namespace Web::Layout {

class MathMLFractionBox final : public Box {
    GC_CELL(MathMLFractionBox, Box);
    GC_DECLARE_ALLOCATOR(MathMLFractionBox);

public:
    MathMLFractionBox(DOM::Document&, MathML::MathMLMFracElement&, GC::Ref<CSS::ComputedProperties>);
    virtual ~MathMLFractionBox() override = default;

    MathML::MathMLMFracElement& dom_node() { return static_cast<MathML::MathMLMFracElement&>(*Box::dom_node()); }
    MathML::MathMLMFracElement const& dom_node() const { return static_cast<MathML::MathMLMFracElement const&>(*Box::dom_node()); }

    virtual RefPtr<Painting::Paintable> create_paintable() const override;

    // Geometry of the fraction bar, in the content-box coordinate space of this box.
    // Set during layout by MathFormattingContext. May be empty (e.g. linethickness == 0).
    CSSPixelRect const& fraction_bar_rect() const { return m_fraction_bar_rect; }
    void set_fraction_bar_rect(CSSPixelRect rect) { m_fraction_bar_rect = rect; }

private:
    virtual bool is_mathml_fraction_box() const override { return true; }

    CSSPixelRect m_fraction_bar_rect {};
};

template<>
inline bool Node::fast_is<MathMLFractionBox>() const { return is_mathml_fraction_box(); }

}
