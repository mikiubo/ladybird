/*
 * Copyright (c) 2020, Stephan Unverwerth <s.unverwerth@serenityos.org>
 * Copyright (c) 2023, MacDue <macdue@dueutil.tech>
 * Copyright (c) 2023-2025, Andreas Kling <andreas@ladybird.org>
 * Copyright (c) 2025, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/AtomicRefCounted.h>
#include <AK/FlyString.h>
#include <AK/RefPtr.h>
#include <AK/Utf16String.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/Font/Typeface.h>
#include <LibGfx/ShapeFeature.h>

class SkFont;
struct hb_font_t;
struct hb_buffer_t;

namespace Gfx {

struct FontPixelMetrics {
    float x_height { 0 };
    float advance_of_ascii_zero { 0 };

    // Number of pixels the font extends above the baseline.
    float ascent { 0 };

    // Number of pixels the font descends below the baseline.
    float descent { 0 };
};

// https://learn.microsoft.com/en-us/typography/opentype/spec/os2#uswidthclass
enum FontWidth {
    UltraCondensed = 1,
    ExtraCondensed = 2,
    Condensed = 3,
    SemiCondensed = 4,
    Normal = 5,
    SemiExpanded = 6,
    Expanded = 7,
    ExtraExpanded = 8,
    UltraExpanded = 9
};

constexpr float text_shaping_resolution = 64;

// https://learn.microsoft.com/en-us/typography/opentype/spec/math#mathconstants-table
// Values mirror hb_ot_math_constant_t.
enum class OpenTypeMathConstant {
    ScriptPercentScaleDown = 0,
    ScriptScriptPercentScaleDown = 1,
    DelimitedSubFormulaMinHeight = 2,
    DisplayOperatorMinHeight = 3,
    MathLeading = 4,
    AxisHeight = 5,
    AccentBaseHeight = 6,
    FlattenedAccentBaseHeight = 7,
    SubscriptShiftDown = 8,
    SubscriptTopMax = 9,
    SubscriptBaselineDropMin = 10,
    SuperscriptShiftUp = 11,
    SuperscriptShiftUpCramped = 12,
    SuperscriptBottomMin = 13,
    SuperscriptBaselineDropMax = 14,
    SubSuperscriptGapMin = 15,
    SuperscriptBottomMaxWithSubscript = 16,
    SpaceAfterScript = 17,
    UpperLimitGapMin = 18,
    UpperLimitBaselineRiseMin = 19,
    LowerLimitGapMin = 20,
    LowerLimitBaselineDropMin = 21,
    StackTopShiftUp = 22,
    StackTopDisplayStyleShiftUp = 23,
    StackBottomShiftDown = 24,
    StackBottomDisplayStyleShiftDown = 25,
    StackGapMin = 26,
    StackDisplayStyleGapMin = 27,
    StretchStackTopShiftUp = 28,
    StretchStackBottomShiftDown = 29,
    StretchStackGapAboveMin = 30,
    StretchStackGapBelowMin = 31,
    FractionNumeratorShiftUp = 32,
    FractionNumeratorDisplayStyleShiftUp = 33,
    FractionDenominatorShiftDown = 34,
    FractionDenominatorDisplayStyleShiftDown = 35,
    FractionNumeratorGapMin = 36,
    FractionNumDisplayStyleGapMin = 37,
    FractionRuleThickness = 38,
    FractionDenominatorGapMin = 39,
    FractionDenomDisplayStyleGapMin = 40,
    SkewedFractionHorizontalGap = 41,
    SkewedFractionVerticalGap = 42,
    OverbarVerticalGap = 43,
    OverbarRuleThickness = 44,
    OverbarExtraAscender = 45,
    UnderbarVerticalGap = 46,
    UnderbarRuleThickness = 47,
    UnderbarExtraDescender = 48,
    RadicalVerticalGap = 49,
    RadicalDisplayStyleVerticalGap = 50,
    RadicalRuleThickness = 51,
    RadicalExtraAscender = 52,
    RadicalKernBeforeDegree = 53,
    RadicalKernAfterDegree = 54,
    RadicalDegreeBottomRaisePercent = 55,
};

class Font : public AtomicRefCounted<Font> {
public:
    Font(NonnullRefPtr<Typeface const>, float point_width, float point_height, FontVariationSettings const variations, ShapeFeatures const& features);
    ~Font();

    u64 id() const { return m_id; }
    float point_size() const;
    float pixel_size() const;
    FontPixelMetrics const& pixel_metrics() const { return m_pixel_metrics; }
    u8 slope() const { return m_typeface->slope(); }
    u16 weight() const { return m_typeface->weight(); }
    bool contains_glyph(u32 code_point) const { return m_typeface->glyph_id_for_code_point(code_point) > 0; }
    float glyph_width(u32 code_point) const;
    u32 glyph_id_for_code_point(u32 code_point) const { return m_typeface->glyph_id_for_code_point(code_point); }
    int x_height() const { return m_point_height; } // FIXME: Read from font
    float width(Utf16View const&) const;
    FlyString const& family() const { return m_typeface->family(); }

    NonnullRefPtr<Font> with_size(float point_size) const;

    Typeface const& typeface() const { return m_typeface; }

    SkFont skia_font(float scale) const;

    Font const& bold_variant() const;
    hb_font_t* harfbuzz_font() const;
    FontVariationSettings const& variation_settings() const { return m_font_variation_settings; }
    ShapeFeatures const& features() const { return m_shape_features; }

    struct ShapingCache {
        HashMap<Utf16String, hb_buffer_t*> map;
        hb_buffer_t* single_ascii_character_map[128] { nullptr };

        ~ShapingCache();
        void clear();
    };
    ShapingCache& shaping_cache() const { return m_shaping_cache; }

    bool is_emoji_font() const;

    // Returns true if the font has an OpenType MATH table.
    bool has_opentype_math_table() const;

    // Returns the value of a MATH table constant in CSS pixels (already scaled to the
    // font's current pixel size). Returns the fallback default if the font does not
    // have a MATH table, or for the percent-valued constants returns the raw integer
    // percentage.
    float opentype_math_constant(OpenTypeMathConstant) const;

private:
    u64 m_id { 0 };

    mutable RefPtr<Font const> m_bold_variant;
    mutable hb_font_t* m_harfbuzz_font { nullptr };

    mutable ShapingCache m_shaping_cache;

    mutable TriState m_is_emoji_font { TriState::Unknown };
    mutable TriState m_has_opentype_math_table { TriState::Unknown };

    NonnullRefPtr<Typeface const> m_typeface;
    float m_point_width { 0.0f };
    float m_point_height { 0.0f };
    FontVariationSettings const m_font_variation_settings;
    ShapeFeatures m_shape_features;
    FontPixelMetrics m_pixel_metrics;

    float m_pixel_size { 0.0f };
};

}
