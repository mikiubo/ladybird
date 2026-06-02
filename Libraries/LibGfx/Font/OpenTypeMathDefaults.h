/*
 * Copyright (c) 2026, mikiubo <michele.uboldi@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGfx/Font/Font.h>

namespace Gfx {

// Default values used when a font has no OpenType MATH table.
// Most values are expressed as a fraction of the font's pixel size (em),
// derived from the MathML Core specification and TeX's default constants.
//
// Percentage-valued constants (Script*PercentScaleDown, RadicalDegreeBottomRaisePercent)
// are returned as raw integer percentages (0-100), not as factors.
//
// The "displayOperatorMinHeight" is in font design units; we approximate as 1.5em.
inline float default_opentype_math_constant(OpenTypeMathConstant constant, float pixel_size)
{
    auto em = [&](float fraction) { return fraction * pixel_size; };
    switch (constant) {
    case OpenTypeMathConstant::ScriptPercentScaleDown:
        return 71.0f;
    case OpenTypeMathConstant::ScriptScriptPercentScaleDown:
        return 50.0f;
    case OpenTypeMathConstant::DelimitedSubFormulaMinHeight:
        return em(1.5f);
    case OpenTypeMathConstant::DisplayOperatorMinHeight:
        return em(1.5f);
    case OpenTypeMathConstant::MathLeading:
        return em(0.15f);
    case OpenTypeMathConstant::AxisHeight:
        return em(0.25f);
    case OpenTypeMathConstant::AccentBaseHeight:
        return em(0.75f);
    case OpenTypeMathConstant::FlattenedAccentBaseHeight:
        return em(0.75f);
    case OpenTypeMathConstant::SubscriptShiftDown:
        return em(0.247f);
    case OpenTypeMathConstant::SubscriptTopMax:
        return em(0.4f);
    case OpenTypeMathConstant::SubscriptBaselineDropMin:
        return em(0.05f);
    case OpenTypeMathConstant::SuperscriptShiftUp:
        return em(0.363f);
    case OpenTypeMathConstant::SuperscriptShiftUpCramped:
        return em(0.289f);
    case OpenTypeMathConstant::SuperscriptBottomMin:
        return em(0.125f);
    case OpenTypeMathConstant::SuperscriptBaselineDropMax:
        return em(0.075f);
    case OpenTypeMathConstant::SubSuperscriptGapMin:
        return em(0.2f);
    case OpenTypeMathConstant::SuperscriptBottomMaxWithSubscript:
        return em(0.4f);
    case OpenTypeMathConstant::SpaceAfterScript:
        return em(0.05f);
    case OpenTypeMathConstant::UpperLimitGapMin:
        return em(0.2f);
    case OpenTypeMathConstant::UpperLimitBaselineRiseMin:
        return em(0.15f);
    case OpenTypeMathConstant::LowerLimitGapMin:
        return em(0.2f);
    case OpenTypeMathConstant::LowerLimitBaselineDropMin:
        return em(0.6f);
    case OpenTypeMathConstant::StackTopShiftUp:
        return em(0.39f);
    case OpenTypeMathConstant::StackTopDisplayStyleShiftUp:
        return em(0.78f);
    case OpenTypeMathConstant::StackBottomShiftDown:
        return em(0.255f);
    case OpenTypeMathConstant::StackBottomDisplayStyleShiftDown:
        return em(0.715f);
    case OpenTypeMathConstant::StackGapMin:
        return em(0.15f);
    case OpenTypeMathConstant::StackDisplayStyleGapMin:
        return em(0.6f);
    case OpenTypeMathConstant::StretchStackTopShiftUp:
        return em(0.5f);
    case OpenTypeMathConstant::StretchStackBottomShiftDown:
        return em(0.5f);
    case OpenTypeMathConstant::StretchStackGapAboveMin:
        return em(0.15f);
    case OpenTypeMathConstant::StretchStackGapBelowMin:
        return em(0.15f);
    case OpenTypeMathConstant::FractionNumeratorShiftUp:
        return em(0.39f);
    case OpenTypeMathConstant::FractionNumeratorDisplayStyleShiftUp:
        return em(0.78f);
    case OpenTypeMathConstant::FractionDenominatorShiftDown:
        return em(0.255f);
    case OpenTypeMathConstant::FractionDenominatorDisplayStyleShiftDown:
        return em(0.715f);
    case OpenTypeMathConstant::FractionNumeratorGapMin:
        return em(0.05f);
    case OpenTypeMathConstant::FractionNumDisplayStyleGapMin:
        return em(0.15f);
    case OpenTypeMathConstant::FractionRuleThickness:
        return em(0.05f);
    case OpenTypeMathConstant::FractionDenominatorGapMin:
        return em(0.05f);
    case OpenTypeMathConstant::FractionDenomDisplayStyleGapMin:
        return em(0.15f);
    case OpenTypeMathConstant::SkewedFractionHorizontalGap:
        return em(0.2f);
    case OpenTypeMathConstant::SkewedFractionVerticalGap:
        return em(0.1f);
    case OpenTypeMathConstant::OverbarVerticalGap:
        return em(0.15f);
    case OpenTypeMathConstant::OverbarRuleThickness:
        return em(0.05f);
    case OpenTypeMathConstant::OverbarExtraAscender:
        return em(0.05f);
    case OpenTypeMathConstant::UnderbarVerticalGap:
        return em(0.15f);
    case OpenTypeMathConstant::UnderbarRuleThickness:
        return em(0.05f);
    case OpenTypeMathConstant::UnderbarExtraDescender:
        return em(0.05f);
    case OpenTypeMathConstant::RadicalVerticalGap:
        return em(0.06f);
    case OpenTypeMathConstant::RadicalDisplayStyleVerticalGap:
        return em(0.16f);
    case OpenTypeMathConstant::RadicalRuleThickness:
        return em(0.05f);
    case OpenTypeMathConstant::RadicalExtraAscender:
        return em(0.05f);
    case OpenTypeMathConstant::RadicalKernBeforeDegree:
        return em(0.27f);
    case OpenTypeMathConstant::RadicalKernAfterDegree:
        return em(-0.55f);
    case OpenTypeMathConstant::RadicalDegreeBottomRaisePercent:
        return 60.0f;
    }
    return 0.0f;
}

// Returns true if this MATH constant is a percentage value (0-100) rather than a length.
inline bool opentype_math_constant_is_percentage(OpenTypeMathConstant constant)
{
    switch (constant) {
    case OpenTypeMathConstant::ScriptPercentScaleDown:
    case OpenTypeMathConstant::ScriptScriptPercentScaleDown:
    case OpenTypeMathConstant::RadicalDegreeBottomRaisePercent:
        return true;
    default:
        return false;
    }
}

}
