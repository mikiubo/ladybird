/*
 * Copyright (c) 2023-2024, Shannon Booth <shannon@serenityos.org>
 * Copyright (c) 2024, Pavel Shliak <shlyakpavel@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Bindings/SVGScriptElementPrototype.h>
#include <LibWeb/Fetch/Fetching/Fetching.h>
#include <LibWeb/Fetch/Infrastructure/FetchAlgorithms.h>
#include <LibWeb/HTML/Scripting/ClassicScript.h>
#include <LibWeb/Namespace.h>
#include <LibWeb/SVG/AttributeNames.h>
#include <LibWeb/SVG/SVGScriptElement.h>

namespace Web::SVG {

GC_DEFINE_ALLOCATOR(SVGScriptElement);

SVGScriptElement::SVGScriptElement(DOM::Document& document, DOM::QualifiedName qualified_name)
    : SVGElement(document, move(qualified_name))
{
}

void SVGScriptElement::initialize(JS::Realm& realm)
{
    WEB_SET_PROTOTYPE_FOR_INTERFACE(SVGScriptElement);
    Base::initialize(realm);
}

void SVGScriptElement::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    SVGURIReferenceMixin::visit_edges(visitor);
    visitor.visit(m_script);
}

void SVGScriptElement::attribute_changed(FlyString const& name, Optional<String> const& old_value, Optional<String> const& value, Optional<FlyString> const& namespace_)
{
    Base::attribute_changed(name, old_value, value, namespace_);
    if (name == SVG::AttributeNames::href || name == SVG::AttributeNames::type) {
        process_the_script_element();
    }
}

void SVGScriptElement::inserted()
{
    Base::inserted();
    if (m_parser_inserted)
        return;

    process_the_script_element();
}

void SVGScriptElement::children_changed(ChildrenChangedMetadata const* metadata)
{
    Base::children_changed(metadata);
    if (m_parser_inserted)
        return;

    process_the_script_element();
}

// https://www.w3.org/TR/SVGMobile12/script.html#ScriptContentProcessing
void SVGScriptElement::process_the_script_element()
{
    // 1. If the 'script' element's "already processed" flag is true or if the element is not in the
    //    document tree, then no action is performed and these steps are ended.
    if (m_already_processed || !in_a_document_tree())
        return;

    // https://svgwg.org/svg2-draft/interact.html#ScriptElement
    // Before attempting to execute the ‘script’ element the resolved media type value for ‘type’ must be inspected.
    // If the SVG user agent does not support the scripting language then the ‘script’ element must not be executed.
    // FIXME: Support type="module" scripts
    auto maybe_script_type = attribute(SVG::AttributeNames::type);
    if (maybe_script_type.has_value() && !maybe_script_type->is_empty()) {
        auto script_type = MUST(maybe_script_type->to_ascii_lowercase().trim_ascii_whitespace());
        if (!MimeSniff::is_javascript_mime_type_essence_match(script_type)) {
            dbgln("SVGScriptElement: Unsupported script type: {}", *maybe_script_type);
            return;
        }
    }

    IGNORE_USE_IN_ESCAPING_LAMBDA String script_content;
    auto script_url = m_document->url();

    // 2. If the 'script' element references external script content, then the external script content
    //    using the current value of the 'xlink:href' attribute is fetched. Further processing of the
    //    'script' element is dependent on the external script content, and will block here until the
    //    resource has been fetched or is determined to be an invalid IRI reference.
    if (has_attribute(SVG::AttributeNames::href) || has_attribute_ns(Namespace::XLink.to_string(), SVG::AttributeNames::href)) {
        auto href_value = href()->base_val();

        auto maybe_script_url = document().encoding_parse_url(href_value);
        if (!maybe_script_url.has_value()) {
            dbgln("Invalid script URL: {}", href_value);
            return;
        }
        script_url = maybe_script_url.release_value();

        auto& vm = realm().vm();
        auto request = Fetch::Infrastructure::Request::create(vm);
        request->set_url(script_url);
        request->set_destination(Fetch::Infrastructure::Request::Destination::Script);
        // FIXME: Use CORS state specified by the ‘crossorigin’ attribute.
        request->set_mode(Fetch::Infrastructure::Request::Mode::NoCORS);
        request->set_credentials_mode(Fetch::Infrastructure::Request::CredentialsMode::SameOrigin);
        request->set_client(&document().relevant_settings_object());

        IGNORE_USE_IN_ESCAPING_LAMBDA bool fetch_done = false;

        Fetch::Infrastructure::FetchAlgorithms::Input fetch_algorithms_input {};
        fetch_algorithms_input.process_response = [this, &script_content, &fetch_done](GC::Ref<Fetch::Infrastructure::Response> response) {
            if (response->is_network_error()) {
                dbgln("Failed to fetch SVG external script.");
                fetch_done = true;
                return;
            }

            auto& realm = this->realm();
            auto& global = document().realm().global_object();

            auto on_data_read = GC::create_function(realm.heap(), [&script_content, &fetch_done](ByteBuffer data) {
                auto content_or_error = String::from_utf8(data);
                if (content_or_error.is_error()) {
                    dbgln("Failed to decode script content as UTF-8");
                } else {
                    script_content = content_or_error.release_value();
                }
                fetch_done = true;
            });

            auto on_error = GC::create_function(realm.heap(), [&fetch_done](JS::Value) {
                dbgln("Error occurred while reading script data.");
                fetch_done = true;
            });

            VERIFY(response->body());
            response->body()->fully_read(realm, on_data_read, on_error, GC::Ref { global });
        };

        auto fetch_promise = Fetch::Fetching::fetch(realm(), request, Fetch::Infrastructure::FetchAlgorithms::create(vm, move(fetch_algorithms_input)));

        // Block until the resource has been fetched or determined invalid
        HTML::main_thread_event_loop().spin_until(GC::create_function(heap(), [&] { return fetch_done; }));

        if (script_content.is_empty()) {
            // Failed to fetch or decode
            return;
        }

    } else {
        // Inline script content
        script_content = child_text_content().to_utf8_but_should_be_ported_to_utf16();
        if (script_content.is_empty())
            return;
    }

    // 3. The 'script' element's "already processed" flag is set to true.
    m_already_processed = true;

    // 4. If the script content is inline, or if it is external and was fetched successfully, then the
    //    script is executed. Note that at this point, these steps may be re-entrant if the execution
    //    of the script results in further 'script' elements being inserted into the document.

    // https://html.spec.whatwg.org/multipage/document-lifecycle.html#read-html
    // Before any script execution occurs, the user agent must wait for scripts may run for the newly-created document to be true for document.
    if (!m_document->ready_to_run_scripts())
        HTML::main_thread_event_loop().spin_until(GC::create_function(heap(), [&] { return m_document->ready_to_run_scripts(); }));

    m_script = HTML::ClassicScript::create(script_url.basename(), script_content, realm(), m_document->base_url(), m_source_line_number);

    // FIXME: Note that a load event is dispatched on a 'script' element once it has been processed,
    // unless it referenced external script content with an invalid IRI reference and 'externalResourcesRequired' was set to 'true'.

    (void)m_script->run();
}

}
