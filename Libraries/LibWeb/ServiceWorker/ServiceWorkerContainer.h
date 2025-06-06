/*
 * Copyright (c) 2024, Tim Ledbetter <tim.ledbetter@ladybird.org>
 * Copyright (c) 2024, Jamie Mansfield <jmansfield@cadixdev.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Bindings/ServiceWorkerRegistrationPrototype.h>
#include <LibWeb/Bindings/WorkerPrototype.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/WebIDL/ExceptionOr.h>
#include <LibWeb/WebIDL/Promise.h>

#define ENUMERATE_SERVICE_WORKER_CONTAINER_EVENT_HANDLERS(E)  \
    E(oncontrollerchange, HTML::EventNames::controllerchange) \
    E(onmessage, HTML::EventNames::message)                   \
    E(onmessageerror, HTML::EventNames::messageerror)

namespace Web::ServiceWorker {

struct RegistrationOptions {
    Optional<String> scope;
    Bindings::WorkerType type = Bindings::WorkerType::Classic;
    Bindings::ServiceWorkerUpdateViaCache update_via_cache = Bindings::ServiceWorkerUpdateViaCache::Imports;
};

class ServiceWorkerContainer : public DOM::EventTarget {
    WEB_PLATFORM_OBJECT(ServiceWorkerContainer, DOM::EventTarget);
    GC_DECLARE_ALLOCATOR(ServiceWorkerContainer);

public:
    [[nodiscard]] static GC::Ref<ServiceWorkerContainer> create(JS::Realm& realm);
    virtual ~ServiceWorkerContainer() override;

    GC::Ref<WebIDL::Promise> register_(String script_url, RegistrationOptions const& options);

    GC::Ref<WebIDL::Promise> get_registration(String const& client_url);

#undef __ENUMERATE
#define __ENUMERATE(attribute_name, event_name)       \
    void set_##attribute_name(WebIDL::CallbackType*); \
    WebIDL::CallbackType* attribute_name();
    ENUMERATE_SERVICE_WORKER_CONTAINER_EVENT_HANDLERS(__ENUMERATE)
#undef __ENUMERATE

private:
    explicit ServiceWorkerContainer(JS::Realm&);

    virtual void initialize(JS::Realm&) override;
    virtual void visit_edges(Cell::Visitor&) override;

    void start_register(Optional<URL::URL> scope_url, Optional<URL::URL> script_url, GC::Ref<WebIDL::Promise>, HTML::EnvironmentSettingsObject&, URL::URL referrer, Bindings::WorkerType, Bindings::ServiceWorkerUpdateViaCache);

    GC::Ref<HTML::EnvironmentSettingsObject> m_service_worker_client;
};

}
