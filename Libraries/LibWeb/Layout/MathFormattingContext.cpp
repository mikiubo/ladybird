/*
 * Copyright (c) 2026, mikiubo <michele.uboldi@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGfx/Font/Font.h>
#include <LibWeb/CSS/Parser/Parser.h>
#include <LibWeb/CSS/StyleValues/LengthStyleValue.h>
#include <LibWeb/CSS/StyleValues/PercentageStyleValue.h>
#include <LibWeb/Layout/AvailableSpace.h>
#include <LibWeb/Layout/MathFormattingContext.h>
#include <LibWeb/Layout/MathMLFractionBox.h>
#include <LibWeb/MathML/AttributeNames.h>

namespace Web::Layout {

MathFormattingContext::MathFormattingContext(LayoutState& state, LayoutMode layout_mode, Box const& context_box, FormattingContext* parent)
    : FormattingContext(Type::Math, layout_mode, state, context_box, parent)
{
}

MathFormattingContext::~MathFormattingContext() = default;

CSSPixels MathFormattingContext::automatic_content_width() const
{
    return m_automatic_content_width;
}

CSSPixels MathFormattingContext::automatic_content_height() const
{
    return m_automatic_content_height;
}

void MathFormattingContext::run(AvailableSpace const& available_space)
{
    if (auto const* fraction_box = as_if<MathMLFractionBox>(context_box())) {
        run_fraction(*fraction_box, available_space);
        return;
    }
    // Other MathML boxes are not yet implemented.
}

// https://w3c.github.io/mathml-core/#fractions-mfrac
// Resolve the linethickness attribute of the <mfrac> element. Returns the
// resolved fraction line thickness in CSS pixels.
static CSSPixels resolve_line_thickness(MathMLFractionBox const& fraction_box, float default_thickness)
{
    auto const& element = fraction_box.dom_node();
    auto attribute = element.attribute(MathML::AttributeNames::linethickness);
    if (!attribute.has_value())
        return CSSPixels::nearest_value_for(default_thickness);

    // The linethickness attribute must have a value that is a valid <length-percentage>.
    // If the attribute is absent or has an invalid value, FractionRuleThickness is used as
    // the default value. A percentage is interpreted relative to that default value.
    // A negative value is interpreted as 0.
    CSS::Parser::ParsingParams parsing_params { element.document() };
    auto parsed = parse_css_type(parsing_params, attribute.value(), CSS::ValueType::LengthPercentage);
    if (!parsed)
        return CSSPixels::nearest_value_for(default_thickness);

    CSSPixels resolved;
    if (parsed->is_percentage()) {
        auto percentage = parsed->as_percentage().percentage().as_fraction();
        resolved = CSSPixels::nearest_value_for(default_thickness * percentage);
    } else if (parsed->is_length()) {
        resolved = parsed->as_length().length().to_px(fraction_box);
    } else {
        // FIXME: Properly resolve calc() relative to the default thickness.
        resolved = CSSPixels::nearest_value_for(default_thickness);
    }

    if (resolved < 0)
        return 0;
    return resolved;
}

void MathFormattingContext::run_fraction(MathMLFractionBox const& fraction_box, AvailableSpace const& available_space)
{
    auto& fraction_state = m_state.get_mutable(fraction_box);

    // Locate the first two in-flow Box children: numerator and denominator.
    Box const* numerator = nullptr;
    Box const* denominator = nullptr;
    size_t in_flow_count = 0;
    for (auto const* child = fraction_box.first_child_of_type<Box>(); child; child = child->next_sibling_of_type<Box>()) {
        if (child->is_out_of_flow())
            continue;
        if (in_flow_count == 0)
            numerator = child;
        else if (in_flow_count == 1)
            denominator = child;
        ++in_flow_count;
    }

    // https://w3c.github.io/mathml-core/#fractions-mfrac
    // If the <mfrac> element has less or more than two in-flow children, its layout algorithm
    // is the same as the <mrow> element. We don't yet implement that; fall back to running
    // a block layout (BlockFormattingContext) on the box. The MathMLFractionBox in this case
    // will not paint a fraction bar.
    if (in_flow_count != 2 || !numerator || !denominator) {
        // FIXME: Lay out children using the <mrow> algorithm. For now, fall back to a
        //        very simple stacked layout so we don't crash.
        CSSPixels total_height = 0;
        CSSPixels max_width = 0;
        for (auto const* child = fraction_box.first_child_of_type<Box>(); child; child = child->next_sibling_of_type<Box>()) {
            layout_inside(*child, m_layout_mode, available_space);
            auto const& child_state = m_state.get(*child);
            max_width = max(max_width, child_state.margin_box_width());
            total_height += child_state.margin_box_height();
        }
        m_automatic_content_width = max_width;
        m_automatic_content_height = total_height;
        if (!fraction_state.has_definite_width())
            fraction_state.set_content_width(max_width);
        if (!fraction_state.has_definite_height())
            fraction_state.set_content_height(total_height);
        const_cast<MathMLFractionBox&>(fraction_box).set_fraction_bar_rect({});
        return;
    }

    // Layout numerator and denominator. We resolve their content width/height from
    // their computed values (which include MathML presentational hints such as the
    // width/height attributes of <mspace>), then run their independent formatting
    // contexts so any descendant boxes get laid out.
    auto layout_child = [&](Box const& child) {
        auto& child_state = m_state.get_mutable(child);

        auto const& child_computed = child.computed_values();
        auto child_available_space = available_space;

        // Resolve width.
        if (!child_computed.width().is_auto()) {
            auto width = calculate_inner_width(child, available_space.width, child_computed.width());
            child_state.set_content_width(width);
            child_state.set_has_definite_width(true);
        }

        // Resolve height.
        if (!child_computed.height().is_auto()) {
            auto height = calculate_inner_height(child, available_space, child_computed.height());
            child_state.set_content_height(height);
            child_state.set_has_definite_height(true);
        }

        if (auto child_context = layout_inside(child, LayoutMode::Normal, child_available_space))
            child_context->parent_context_did_dimension_child_root_box();
    };

    layout_child(*numerator);
    layout_child(*denominator);

    auto const& numerator_state = m_state.get(*numerator);
    auto const& denominator_state = m_state.get(*denominator);

    // Read OpenType MATH constants from the first available font on the <mfrac> element.
    // The math-style determines which gap/shift to use.
    auto const& font = fraction_box.first_available_font();
    auto is_display_style = fraction_box.computed_values().math_style() == CSS::MathStyle::Normal;

    auto default_rule_thickness = font.opentype_math_constant(Gfx::OpenTypeMathConstant::FractionRuleThickness);
    auto axis_height = CSSPixels::nearest_value_for(font.opentype_math_constant(Gfx::OpenTypeMathConstant::AxisHeight));

    auto numerator_shift_up = CSSPixels::nearest_value_for(font.opentype_math_constant(is_display_style
            ? Gfx::OpenTypeMathConstant::FractionNumeratorDisplayStyleShiftUp
            : Gfx::OpenTypeMathConstant::FractionNumeratorShiftUp));
    auto denominator_shift_down = CSSPixels::nearest_value_for(font.opentype_math_constant(is_display_style
            ? Gfx::OpenTypeMathConstant::FractionDenominatorDisplayStyleShiftDown
            : Gfx::OpenTypeMathConstant::FractionDenominatorShiftDown));
    auto numerator_gap_min = CSSPixels::nearest_value_for(font.opentype_math_constant(is_display_style
            ? Gfx::OpenTypeMathConstant::FractionNumDisplayStyleGapMin
            : Gfx::OpenTypeMathConstant::FractionNumeratorGapMin));
    auto denominator_gap_min = CSSPixels::nearest_value_for(font.opentype_math_constant(is_display_style
            ? Gfx::OpenTypeMathConstant::FractionDenomDisplayStyleGapMin
            : Gfx::OpenTypeMathConstant::FractionDenominatorGapMin));

    auto line_thickness = resolve_line_thickness(fraction_box, default_rule_thickness);
    auto half_thickness = line_thickness / 2;

    auto numerator_margin_box_width = numerator_state.margin_box_width();
    auto denominator_margin_box_width = denominator_state.margin_box_width();
    auto numerator_margin_box_height = numerator_state.margin_box_height();
    auto denominator_margin_box_height = denominator_state.margin_box_height();

    // The numerator's margin box has its alphabetic baseline at its block-end edge for
    // simple cases (the box's line-ascent equals its margin-box height, and line-descent is 0).
    // FIXME: Compute proper ink and alphabetic baselines for the numerator and denominator.
    CSSPixels numerator_line_ascent = numerator_margin_box_height;
    CSSPixels numerator_line_descent = 0;
    CSSPixels numerator_ink_line_descent = 0;
    CSSPixels denominator_line_ascent = denominator_margin_box_height;
    CSSPixels denominator_line_descent = 0;
    CSSPixels denominator_ink_line_ascent = denominator_margin_box_height;

    // Inline size of the math content.
    CSSPixels inline_size = max(numerator_margin_box_width, denominator_margin_box_width);

    // Decide between zero-thickness and nonzero-thickness layouts.
    CSSPixels line_ascent;
    CSSPixels line_descent;
    CSSPixels numerator_shift;
    CSSPixels denominator_shift;
    CSSPixels bar_center_block_offset_from_baseline = axis_height;

    if (line_thickness > 0) {
        // 3.3.2.1 Fraction with nonzero line thickness
        numerator_shift = max(numerator_shift_up, axis_height + half_thickness + numerator_gap_min + numerator_ink_line_descent);
        denominator_shift = max(denominator_shift_down, half_thickness + denominator_gap_min + denominator_ink_line_ascent - axis_height);

        line_ascent = max(numerator_shift + numerator_line_ascent, max(-denominator_shift + denominator_line_ascent, axis_height + half_thickness));
        line_descent = max(-numerator_shift + numerator_line_descent,
            max(denominator_shift + denominator_line_descent,
                max(-axis_height + half_thickness, CSSPixels(0))));
    } else {
        // 3.3.2.2 Fraction with zero line thickness (stack)
        auto top_shift = CSSPixels::nearest_value_for(font.opentype_math_constant(is_display_style
                ? Gfx::OpenTypeMathConstant::StackTopDisplayStyleShiftUp
                : Gfx::OpenTypeMathConstant::StackTopShiftUp));
        auto bottom_shift = CSSPixels::nearest_value_for(font.opentype_math_constant(is_display_style
                ? Gfx::OpenTypeMathConstant::StackBottomDisplayStyleShiftDown
                : Gfx::OpenTypeMathConstant::StackBottomShiftDown));
        auto gap_min = CSSPixels::nearest_value_for(font.opentype_math_constant(is_display_style
                ? Gfx::OpenTypeMathConstant::StackDisplayStyleGapMin
                : Gfx::OpenTypeMathConstant::StackGapMin));

        // Approximation: ink_line_ascent_denominator == line_ascent_denominator,
        //                ink_line_descent_numerator == line_descent_numerator (=0 here).
        auto gap = (bottom_shift - denominator_line_ascent) + (top_shift - numerator_line_descent);
        if (auto delta = gap_min - gap; delta > 0) {
            top_shift += delta / 2;
            bottom_shift += delta - delta / 2;
        }

        numerator_shift = top_shift;
        denominator_shift = bottom_shift;

        line_ascent = max(numerator_shift + numerator_line_ascent, -denominator_shift + denominator_line_ascent);
        line_descent = max(-numerator_shift + numerator_line_descent,
            max(denominator_shift + denominator_line_descent, CSSPixels(0)));
    }

    auto block_size = line_ascent + line_descent;

    // Position children. The math content box and the content box share their block-start
    // edges, and the inline-start edges' midpoints coincide. For now we assume direction: ltr.
    CSSPixels numerator_inline_offset = (inline_size - numerator_margin_box_width) / 2;
    CSSPixels denominator_inline_offset = (inline_size - denominator_margin_box_width) / 2;

    // The alphabetic baseline of the math content sits at block offset = line_ascent.
    CSSPixels math_baseline_y = line_ascent;

    // Numerator alphabetic baseline shifted by numerator_shift towards line-over.
    // Its baseline is at math_baseline_y - numerator_shift; its block-start edge is
    // baseline - numerator_line_ascent.
    CSSPixels numerator_y = math_baseline_y - numerator_shift - numerator_line_ascent;
    CSSPixels denominator_y = math_baseline_y + denominator_shift - denominator_line_ascent;

    auto& numerator_state_mut = m_state.get_mutable(*numerator);
    numerator_state_mut.set_content_offset({ numerator_inline_offset + numerator_state_mut.margin_box_left(),
        numerator_y + numerator_state_mut.margin_box_top() });

    auto& denominator_state_mut = m_state.get_mutable(*denominator);
    denominator_state_mut.set_content_offset({ denominator_inline_offset + denominator_state_mut.margin_box_left(),
        denominator_y + denominator_state_mut.margin_box_top() });

    // Set automatic content size for the fraction box. The parent BFC reads this back
    // via resolve_used_height_if_treated_as_auto() to size the mfrac in the block axis,
    // and via automatic_content_width() for intrinsic sizing in the inline axis.
    m_automatic_content_width = inline_size;
    m_automatic_content_height = block_size;

    // Override the inline size with the math content's inline size, since the parent BFC
    // would otherwise stretch the mfrac to fill its containing block.
    fraction_state.set_content_width(inline_size);
    fraction_state.set_has_definite_width(true);
    if (!fraction_state.has_definite_height())
        fraction_state.set_content_height(block_size);

    // Compute fraction bar rect (in content-box coordinates of the fraction box).
    CSSPixelRect bar_rect;
    if (line_thickness > 0) {
        // Inline size of the bar is the inline size of the content box; its inline-start edge
        // is aligned with the content box's. Its center sits at math_baseline_y - bar_center_block_offset_from_baseline.
        CSSPixels bar_center_y = math_baseline_y - bar_center_block_offset_from_baseline;
        CSSPixels bar_top_y = bar_center_y - half_thickness;
        bar_rect = CSSPixelRect { CSSPixels(0), bar_top_y, fraction_state.content_width(), line_thickness };
    }

    // The MathMLFractionBox is logically const w.r.t. layout but stores paintable hints; cast away.
    const_cast<MathMLFractionBox&>(fraction_box).set_fraction_bar_rect(bar_rect);
}

}
