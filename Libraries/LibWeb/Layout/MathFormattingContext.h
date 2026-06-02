/*
 * Copyright (c) 2026, mikiubo <michele.uboldi@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Forward.h>
#include <LibWeb/Layout/FormattingContext.h>

namespace Web::Layout {

class MathMLFractionBox;

// Implements the layout algorithm for the MathML <mfrac> element as described
// in MathML Core section 3.3.2. Currently only supports MathMLFractionBox as
// the context box.
class MathFormattingContext final : public FormattingContext {
public:
    MathFormattingContext(LayoutState&, LayoutMode, Box const&, FormattingContext* parent);
    virtual ~MathFormattingContext() override;

    virtual void run(AvailableSpace const&) override;
    virtual CSSPixels automatic_content_width() const override;
    virtual CSSPixels automatic_content_height() const override;

private:
    void run_fraction(MathMLFractionBox const&, AvailableSpace const&);

    CSSPixels m_automatic_content_width { 0 };
    CSSPixels m_automatic_content_height { 0 };
};

}
