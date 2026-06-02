/*
 * Copyright (c) 2026, mikiubo <michele.uboldi@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Layout/MathMLFractionBox.h>
#include <LibWeb/Painting/MathMLFractionPaintable.h>

namespace Web::Layout {

GC_DEFINE_ALLOCATOR(MathMLFractionBox);

MathMLFractionBox::MathMLFractionBox(DOM::Document& document, MathML::MathMLMFracElement& element, GC::Ref<CSS::ComputedProperties> style)
    : Box(document, &element, move(style))
{
}

RefPtr<Painting::Paintable> MathMLFractionBox::create_paintable() const
{
    return Painting::MathMLFractionPaintable::create(*this);
}

}
