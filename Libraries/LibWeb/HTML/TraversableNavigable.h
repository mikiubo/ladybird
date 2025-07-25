/*
 * Copyright (c) 2022, Andreas Kling <andreas@ladybird.org>
 * Copyright (c) 2025, Jelle Raaijmakers <jelle@ladybird.org>
 * Copyright (c) 2023-2025, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Vector.h>
#include <LibWeb/Geolocation/Geolocation.h>
#include <LibWeb/HTML/Navigable.h>
#include <LibWeb/HTML/NavigationType.h>
#include <LibWeb/HTML/SessionHistoryTraversalQueue.h>
#include <LibWeb/HTML/VisibilityState.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/StorageAPI/StorageShed.h>

#ifdef AK_OS_MACOS
#    include <LibGfx/MetalContext.h>
#endif

#ifdef USE_VULKAN
#    include <LibGfx/VulkanContext.h>
#endif

namespace Web::HTML {

// https://html.spec.whatwg.org/multipage/document-sequences.html#traversable-navigable
class TraversableNavigable final : public Navigable {
    GC_CELL(TraversableNavigable, Navigable);
    GC_DECLARE_ALLOCATOR(TraversableNavigable);

public:
    static WebIDL::ExceptionOr<GC::Ref<TraversableNavigable>> create_a_new_top_level_traversable(GC::Ref<Page>, GC::Ptr<BrowsingContext> opener, String target_name);
    static WebIDL::ExceptionOr<GC::Ref<TraversableNavigable>> create_a_fresh_top_level_traversable(GC::Ref<Page>, URL::URL const& initial_navigation_url, Variant<Empty, String, POSTResource> = Empty {});

    virtual ~TraversableNavigable() override;

    virtual bool is_top_level_traversable() const override;

    int current_session_history_step() const { return m_current_session_history_step; }
    Vector<GC::Ref<SessionHistoryEntry>>& session_history_entries() { return m_session_history_entries; }
    Vector<GC::Ref<SessionHistoryEntry>> const& session_history_entries() const { return m_session_history_entries; }
    bool running_nested_apply_history_step() const { return m_running_nested_apply_history_step; }

    VisibilityState system_visibility_state() const { return m_system_visibility_state; }
    void set_system_visibility_state(VisibilityState);

    bool is_created_by_web_content() const { return m_is_created_by_web_content; }
    void set_is_created_by_web_content(bool value) { m_is_created_by_web_content = value; }

    struct HistoryObjectLengthAndIndex {
        u64 script_history_length;
        u64 script_history_index;
    };
    HistoryObjectLengthAndIndex get_the_history_object_length_and_index(int) const;

    enum class HistoryStepResult {
        InitiatorDisallowed,
        CanceledByBeforeUnload,
        CanceledByNavigate,
        Applied,
    };

    HistoryStepResult apply_the_traverse_history_step(int, GC::Ptr<SourceSnapshotParams>, GC::Ptr<Navigable>, UserNavigationInvolvement);
    HistoryStepResult apply_the_reload_history_step(UserNavigationInvolvement);
    enum class SynchronousNavigation : bool {
        Yes,
        No,
    };
    HistoryStepResult apply_the_push_or_replace_history_step(int step, HistoryHandlingBehavior history_handling, UserNavigationInvolvement, SynchronousNavigation);
    HistoryStepResult update_for_navigable_creation_or_destruction();

    int get_the_used_step(int step) const;
    Vector<GC::Root<Navigable>> get_all_navigables_whose_current_session_history_entry_will_change_or_reload(int) const;
    Vector<GC::Root<Navigable>> get_all_navigables_that_only_need_history_object_length_index_update(int) const;
    Vector<GC::Root<Navigable>> get_all_navigables_that_might_experience_a_cross_document_traversal(int) const;

    Vector<int> get_all_used_history_steps() const;
    void clear_the_forward_session_history();
    void traverse_the_history_by_delta(int delta, GC::Ptr<DOM::Document> source_document = {});

    void close_top_level_traversable();
    void definitely_close_top_level_traversable();
    void destroy_top_level_traversable();

    void append_session_history_traversal_steps(GC::Ref<GC::Function<void()>> steps)
    {
        m_session_history_traversal_queue->append(steps);
    }

    void append_session_history_synchronous_navigation_steps(GC::Ref<Navigable> target_navigable, GC::Ref<GC::Function<void()>> steps)
    {
        m_session_history_traversal_queue->append_sync(steps, target_navigable);
    }

    String window_handle() const { return m_window_handle; }
    void set_window_handle(String window_handle) { m_window_handle = move(window_handle); }

    [[nodiscard]] GC::Ptr<DOM::Node> currently_focused_area();

    enum class CheckIfUnloadingIsCanceledResult {
        CanceledByBeforeUnload,
        CanceledByNavigate,
        Continue,
    };
    CheckIfUnloadingIsCanceledResult check_if_unloading_is_canceled(Vector<GC::Root<Navigable>> navigables_that_need_before_unload);

    StorageAPI::StorageShed& storage_shed() { return m_storage_shed; }
    StorageAPI::StorageShed const& storage_shed() const { return m_storage_shed; }

    // https://w3c.github.io/geolocation/#dfn-emulated-position-data
    Geolocation::EmulatedPositionData const& emulated_position_data() const;
    void set_emulated_position_data(Geolocation::EmulatedPositionData data);

    void process_screenshot_requests();
    void queue_screenshot_task(Optional<UniqueNodeID> node_id)
    {
        m_screenshot_tasks.enqueue({ node_id });
        set_needs_repaint();
    }

private:
    TraversableNavigable(GC::Ref<Page>);

    virtual bool is_traversable() const override { return true; }

    virtual void visit_edges(Cell::Visitor&) override;

    // FIXME: Fix spec typo cancelation --> cancellation
    HistoryStepResult apply_the_history_step(
        int step,
        bool check_for_cancelation,
        GC::Ptr<SourceSnapshotParams>,
        GC::Ptr<Navigable> initiator_to_check,
        UserNavigationInvolvement user_involvement,
        Optional<Bindings::NavigationType> navigation_type,
        SynchronousNavigation);

    CheckIfUnloadingIsCanceledResult check_if_unloading_is_canceled(Vector<GC::Root<Navigable>> navigables_that_need_before_unload, GC::Ptr<TraversableNavigable> traversable, Optional<int> target_step, Optional<UserNavigationInvolvement> user_involvement_for_navigate_events);

    Vector<GC::Ref<SessionHistoryEntry>> get_session_history_entries_for_the_navigation_api(GC::Ref<Navigable>, int);

    [[nodiscard]] bool can_go_forward() const;

    // https://html.spec.whatwg.org/multipage/document-sequences.html#tn-current-session-history-step
    int m_current_session_history_step { 0 };

    // https://html.spec.whatwg.org/multipage/document-sequences.html#tn-session-history-entries
    Vector<GC::Ref<SessionHistoryEntry>> m_session_history_entries;

    // FIXME: https://html.spec.whatwg.org/multipage/document-sequences.html#tn-session-history-traversal-queue

    // https://html.spec.whatwg.org/multipage/document-sequences.html#tn-running-nested-apply-history-step
    bool m_running_nested_apply_history_step { false };

    // https://html.spec.whatwg.org/multipage/document-sequences.html#system-visibility-state
    VisibilityState m_system_visibility_state { VisibilityState::Hidden };

    // https://html.spec.whatwg.org/multipage/document-sequences.html#is-created-by-web-content
    bool m_is_created_by_web_content { false };

    // https://storage.spec.whatwg.org/#traversable-navigable-storage-shed
    // A traversable navigable holds a storage shed, which is a storage shed. A traversable navigable’s storage shed holds all session storage data.
    GC::Ref<StorageAPI::StorageShed> m_storage_shed;

    GC::Ref<SessionHistoryTraversalQueue> m_session_history_traversal_queue;

    String m_window_handle;

    // https://w3c.github.io/geolocation/#dfn-emulated-position-data
    Geolocation::EmulatedPositionData m_emulated_position_data;

    struct ScreenshotTask {
        Optional<Web::UniqueNodeID> node_id;
    };
    Queue<ScreenshotTask> m_screenshot_tasks;
};

struct BrowsingContextAndDocument {
    GC::Ref<HTML::BrowsingContext> browsing_context;
    GC::Ref<DOM::Document> document;
};

WebIDL::ExceptionOr<BrowsingContextAndDocument> create_a_new_top_level_browsing_context_and_document(GC::Ref<Page> page);
void finalize_a_same_document_navigation(GC::Ref<TraversableNavigable> traversable, GC::Ref<Navigable> target_navigable, GC::Ref<SessionHistoryEntry> target_entry, GC::Ptr<SessionHistoryEntry> entry_to_replace, HistoryHandlingBehavior, UserNavigationInvolvement);

template<>
inline bool Navigable::fast_is<TraversableNavigable>() const { return is_traversable(); }

}
