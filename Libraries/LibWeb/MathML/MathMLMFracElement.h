/*
 * Copyright (c) 2026, mikiubo <michele.uboldi@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/MathML/MathMLElement.h>

namespace Web::MathML {

class MathMLMFracElement final : public MathMLElement {
    WEB_NON_IDL_PLATFORM_OBJECT(MathMLMFracElement, MathMLElement);
    GC_DECLARE_ALLOCATOR(MathMLMFracElement);

public:
    virtual ~MathMLMFracElement() override = default;

private:
    MathMLMFracElement(DOM::Document&, DOM::QualifiedName);
};

}
