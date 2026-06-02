/*
 * Copyright (c) 2026, mikiubo <michele.uboldi@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/MathML/MathMLMFracElement.h>

namespace Web::MathML {

GC_DEFINE_ALLOCATOR(MathMLMFracElement);

MathMLMFracElement::MathMLMFracElement(DOM::Document& document, DOM::QualifiedName qualified_name)
    : MathMLElement(document, move(qualified_name))
{
}

}
