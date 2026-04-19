/*
 * Copyright (c) 2026, Tim Flynn <trflynn89@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/String.h>
#include <LibWebView/Forward.h>

#include <QPoint>
#include <QPointer>
#include <QToolBar>

class QAction;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QPaintEvent;
class QToolButton;

namespace Ladybird {

class BookmarksBar final : public QToolBar {
    Q_OBJECT

public:
    explicit BookmarksBar(QWidget* parent = nullptr);

    void rebuild();

    String const& selected_bookmark_menu_item_id() const { return m_selected_bookmark_menu_item_id; }
    Optional<String> const& selected_bookmark_menu_target_folder_id() const { return m_selected_bookmark_menu_target_folder_id; }

    void show_context_menu(QPoint, Optional<WebView::BookmarkItem const&>, Optional<String const&> target_folder_id);

private:
    virtual bool eventFilter(QObject* object, QEvent* event) override;
    virtual void dragEnterEvent(QDragEnterEvent*) override;
    virtual void dragLeaveEvent(QDragLeaveEvent*) override;
    virtual void dragMoveEvent(QDragMoveEvent*) override;
    virtual void dropEvent(QDropEvent*) override;
    virtual void paintEvent(QPaintEvent*) override;

    bool handle_left_mouse_click(QMouseEvent*, QObject*);
    bool handle_middle_mouse_click(QMouseEvent*, QObject*);
    bool handle_right_mouse_click(QMouseEvent*, QObject*);
    void maybe_start_drag(QMouseEvent*, QObject*);
    void extract_item_properties(QObject*);
    bool is_bookmark_bar_action(QAction const&) const;
    size_t drop_target_index(QPoint const&) const;
    Optional<int> drop_indicator_x_position(size_t) const;

    QMenu& bookmarks_bar_context_menu();
    QMenu& bookmark_context_menu();
    QMenu& bookmark_folder_context_menu();

    QMenu* m_bookmarks_bar_context_menu { nullptr };
    QMenu* m_bookmark_context_menu { nullptr };
    QMenu* m_bookmark_folder_context_menu { nullptr };

    String m_selected_bookmark_menu_item_id;
    QString m_selected_bookmark_menu_item_type;
    Optional<String> m_selected_bookmark_menu_target_folder_id;

    QPointer<QToolButton> m_drag_source_button;
    QPoint m_drag_start_position;
    Optional<size_t> m_drop_target_index;
};

}
