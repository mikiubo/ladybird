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
    virtual bool event(QEvent*) override;
    virtual bool eventFilter(QObject* object, QEvent* event) override;

    virtual void paintEvent(QPaintEvent*) override;
    virtual void dragEnterEvent(QDragEnterEvent*) override;
    virtual void dragMoveEvent(QDragMoveEvent*) override;
    virtual void dragLeaveEvent(QDragLeaveEvent*) override;
    virtual void dropEvent(QDropEvent*) override;

    bool handle_left_mouse_click(QMouseEvent*, QObject*);
    bool handle_middle_mouse_click(QMouseEvent*, QObject*);
    bool handle_right_mouse_click(QMouseEvent*, QObject*);
    bool handle_mouse_move(QMouseEvent*, QObject*);
    void start_bookmark_drag(QToolButton& button);
    void extract_item_properties(QObject*);
    void update_chrome_style();

    int insertion_index_at(QPoint const& position) const;
    QToolButton* folder_button_at(QPoint const& position) const;

    QMenu& bookmarks_bar_context_menu();
    QMenu& bookmark_context_menu();
    QMenu& bookmark_folder_context_menu();

    QMenu* m_bookmarks_bar_context_menu { nullptr };
    QMenu* m_bookmark_context_menu { nullptr };
    QMenu* m_bookmark_folder_context_menu { nullptr };

    String m_selected_bookmark_menu_item_id;
    QString m_selected_bookmark_menu_item_type;
    Optional<String> m_selected_bookmark_menu_target_folder_id;
    bool m_is_updating_chrome_style { false };

    QPointer<QToolButton> m_pressed_button;
    QPoint m_drag_start_position;
    QPoint m_position_in_pressed_button;
    int m_drop_indicator_index { -1 };
    QPointer<QToolButton> m_drop_indicator_folder_button;
};

}
