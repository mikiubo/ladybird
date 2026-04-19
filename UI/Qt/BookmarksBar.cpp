/*
 * Copyright (c) 2026, Tim Flynn <trflynn89@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWebView/Application.h>
#include <LibWebView/BookmarkStore.h>
#include <UI/Qt/BookmarksBar.h>
#include <UI/Qt/Icon.h>
#include <UI/Qt/Menu.h>
#include <UI/Qt/StringUtils.h>

#include <QAction>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QToolButton>
#include <qapplication.h>

namespace Ladybird {

static constexpr int BOOKMARK_BUTTON_MAX_WIDTH = 150;
static constexpr int BOOKMARK_BUTTON_ICON_SIZE = 16;

static void install_menu_event_filter(QObject* filter, QMenu* menu)
{
    menu->installEventFilter(filter);

    for (auto* action : menu->actions()) {
        if (auto* submenu = action->menu())
            install_menu_event_filter(filter, submenu);
    }
}

BookmarksBar::BookmarksBar(QWidget* parent)
    : QToolBar(parent)
{
    setIconSize({ BOOKMARK_BUTTON_ICON_SIZE, BOOKMARK_BUTTON_ICON_SIZE });
    setVisible(WebView::Application::settings().show_bookmarks_bar());
    setMovable(false);
    setAcceptDrops(true);

    installEventFilter(this);

    rebuild();
}

void BookmarksBar::rebuild()
{
    for (auto* action : actions()) {
        if (auto* menu = action->menu())
            menu->close();
    }

    clear();

    auto set_button_properties = [&](QToolButton* button, QString const& title) {
        button->setText(button->fontMetrics().elidedText(title, Qt::ElideRight, BOOKMARK_BUTTON_MAX_WIDTH - 28));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setMaximumWidth(BOOKMARK_BUTTON_MAX_WIDTH);
        button->installEventFilter(this);
    };

    for (auto const& item : WebView::Application::the().bookmarks_menu().items()) {
        item.visit(
            [&](NonnullRefPtr<WebView::Action> const& bookmark) {
                if (bookmark->id() != WebView::ActionID::BookmarkItem)
                    return;

                auto* action = create_application_action(*this, *bookmark);
                addAction(action);

                if (auto* button = as_if<QToolButton>(widgetForAction(action)))
                    set_button_properties(button, qstring_from_ak_string(bookmark->text()));
            },
            [&](NonnullRefPtr<WebView::Menu> const& folder) {
                auto title = qstring_from_ak_string(folder->title());

                auto* submenu = create_application_menu(*this, *folder);
                install_menu_event_filter(this, submenu);

                auto* action = new QAction(title, this);
                action->setIcon(create_tvg_icon_with_theme_colors("folder", palette()));
                action->setProperty("id", submenu->property("id"));
                action->setProperty("type", submenu->property("type"));
                action->setProperty("target_folder_id", submenu->property("target_folder_id"));
                action->setMenu(submenu);
                addAction(action);

                if (auto* button = as_if<QToolButton>(widgetForAction(action))) {
                    button->setPopupMode(QToolButton::InstantPopup);
                    set_button_properties(button, title);
                }
            },
            [](WebView::Separator) {
            });
    }
}

void BookmarksBar::show_context_menu(QPoint position, Optional<WebView::BookmarkItem const&> item, Optional<String const&> target_folder_id)
{
    if (item.has_value()) {
        m_selected_bookmark_menu_item_id = item->id;
        m_selected_bookmark_menu_target_folder_id = target_folder_id.copy();

        if (item->is_bookmark())
            bookmark_context_menu().exec(position);
        else if (item->is_folder())
            bookmark_folder_context_menu().exec(position);
    } else {
        m_selected_bookmark_menu_item_id = {};
        m_selected_bookmark_menu_target_folder_id = {};

        bookmarks_bar_context_menu().exec(position);
    }
}

bool BookmarksBar::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto& mouse_event = as<QMouseEvent>(*event);

        if (mouse_event.button() == Qt::LeftButton) {
            if (auto* button = as_if<QToolButton>(object)) {
                auto* action = button->defaultAction();
                if (action && is_bookmark_bar_action(*action)) {
                    m_drag_source_button = button;
                    m_drag_start_position = mouse_event.pos();
                }
            }
        }

        if (mouse_event.button() == Qt::LeftButton)
            return handle_left_mouse_click(&mouse_event, object);
        if (mouse_event.button() == Qt::MiddleButton)
            return handle_middle_mouse_click(&mouse_event, object);
        if (mouse_event.button() == Qt::RightButton)
            return handle_right_mouse_click(&mouse_event, object);
    }

    if (event->type() == QEvent::MouseMove) {
        maybe_start_drag(&as<QMouseEvent>(*event), object);
        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        m_drag_source_button.clear();
    }

    return QToolBar::eventFilter(object, event);
}

void BookmarksBar::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-ladybird-bookmark-id")) {
        m_drop_target_index = drop_target_index(event->position().toPoint());
        update();

        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }

    QToolBar::dragEnterEvent(event);
}

void BookmarksBar::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_drop_target_index.clear();
    update();
    QToolBar::dragLeaveEvent(event);
}

void BookmarksBar::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-ladybird-bookmark-id")) {
        auto target_index = drop_target_index(event->position().toPoint());
        if (!m_drop_target_index.has_value() || *m_drop_target_index != target_index) {
            m_drop_target_index = target_index;
            update();
        }

        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }

    QToolBar::dragMoveEvent(event);
}

void BookmarksBar::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasFormat("application/x-ladybird-bookmark-id")) {
        m_drop_target_index.clear();
        update();
        QToolBar::dropEvent(event);
        return;
    }

    auto id = ak_string_from_qstring(QString::fromUtf8(event->mimeData()->data("application/x-ladybird-bookmark-id")));
    if (id.is_empty()) {
        m_drop_target_index.clear();
        update();
        QToolBar::dropEvent(event);
        return;
    }

    auto target_index = drop_target_index(event->position().toPoint());
    WebView::Application::bookmark_store().move_item(id, {}, target_index);

    m_drop_target_index.clear();
    update();

    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void BookmarksBar::paintEvent(QPaintEvent* event)
{
    QToolBar::paintEvent(event);

    if (!m_drop_target_index.has_value())
        return;

    auto indicator_x = drop_indicator_x_position(*m_drop_target_index);
    if (!indicator_x.has_value())
        return;

    QPainter painter(this);
    auto color = palette().highlight().color();
    color.setAlpha(230);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(*indicator_x - 1.5, 4, 3, height() - 8), 1.5, 1.5);
}

bool BookmarksBar::handle_left_mouse_click(QMouseEvent* event, QObject* item)
{
    if (event->modifiers().testFlag(Qt::ControlModifier))
        return handle_middle_mouse_click(event, item);
    return false;
}

bool BookmarksBar::handle_middle_mouse_click(QMouseEvent* event, QObject* item)
{
    auto activate_tab = event->modifiers().testFlag(Qt::ShiftModifier) ? Web::HTML::ActivateTab::No : Web::HTML::ActivateTab::Yes;

    if (auto* button = as_if<QToolButton>(item)) {
        auto* action = button->defaultAction();
        extract_item_properties(action);

        if (m_selected_bookmark_menu_item_type == "bookmark")
            WebView::Application::the().open_bookmark_in_new_tab(m_selected_bookmark_menu_item_id, activate_tab);
    } else if (auto* menu = as_if<QMenu>(item)) {
        if (auto* action = menu->actionAt(event->pos())) {
            extract_item_properties(action);

            if (m_selected_bookmark_menu_item_type == "bookmark")
                WebView::Application::the().open_bookmark_in_new_tab(m_selected_bookmark_menu_item_id, activate_tab);
        }
    }

    return true;
}

bool BookmarksBar::handle_right_mouse_click(QMouseEvent* event, QObject* item)
{
    if (is<BookmarksBar>(item)) {
        m_selected_bookmark_menu_item_id = {};
        m_selected_bookmark_menu_target_folder_id = {};

        bookmarks_bar_context_menu().exec(event->globalPosition().toPoint());
    } else if (auto* button = as_if<QToolButton>(item)) {
        auto* action = button->defaultAction();
        extract_item_properties(action);

        if (m_selected_bookmark_menu_item_type == "bookmark")
            bookmark_context_menu().exec(event->globalPosition().toPoint());
        else if (m_selected_bookmark_menu_item_type == "folder")
            bookmark_folder_context_menu().exec(event->globalPosition().toPoint());
    } else if (auto* menu = as_if<QMenu>(item)) {
        if (auto* action = menu->actionAt(event->pos())) {
            QObject* submenu = action->menu();
            extract_item_properties(submenu ?: action);
        }

        if (m_selected_bookmark_menu_item_type.isEmpty())
            extract_item_properties(menu);

        // FIXME: We create a temporary context menu parented to the dropdown. Otherwise, Qt complains that the context
        //        menu's parent does not match the current topmost popup. It would be nice if we could figure out a way
        //        to avoid this duplicated menu.
        QMenu context_menu(menu);

        if (m_selected_bookmark_menu_item_type == "bookmark")
            repopulate_application_menu(context_menu, context_menu, WebView::Application::the().bookmark_context_menu());
        else if (m_selected_bookmark_menu_item_type == "folder")
            repopulate_application_menu(context_menu, context_menu, WebView::Application::the().bookmark_folder_context_menu());

        if (!context_menu.isEmpty() && context_menu.exec(event->globalPosition().toPoint()))
            menu->close();
    }

    return true;
}

void BookmarksBar::maybe_start_drag(QMouseEvent* event, QObject* item)
{
    if (!m_drag_source_button || item != m_drag_source_button)
        return;

    if ((event->buttons() & Qt::LeftButton) == 0)
        return;

    if ((event->pos() - m_drag_start_position).manhattanLength() < QApplication::startDragDistance())
        return;

    auto* action = m_drag_source_button->defaultAction();
    if (!action || !is_bookmark_bar_action(*action))
        return;

    auto id = action->property("id").toString();
    if (id.isEmpty())
        return;

    auto* drag = new QDrag(m_drag_source_button);
    auto* mime_data = new QMimeData();
    mime_data->setData("application/x-ladybird-bookmark-id", id.toUtf8());
    drag->setMimeData(mime_data);

    auto source_pixmap = m_drag_source_button->grab();
    QPixmap drag_pixmap(source_pixmap.size());
    drag_pixmap.fill(Qt::transparent);

    QPainter painter(&drag_pixmap);
    painter.setOpacity(0.8);
    painter.drawPixmap(0, 0, source_pixmap);
    painter.setPen(QPen(palette().highlight().color(), 2));
    painter.drawRoundedRect(QRect(1, 1, drag_pixmap.width() - 2, drag_pixmap.height() - 2), 4, 4);

    drag->setPixmap(drag_pixmap);
    drag->setHotSpot(event->pos());

    m_drag_source_button.clear();
    drag->exec(Qt::MoveAction);
}

void BookmarksBar::extract_item_properties(QObject* item)
{
    m_selected_bookmark_menu_item_id = ak_string_from_qstring(item->property("id").toString());
    m_selected_bookmark_menu_item_type = item->property("type").toString();

    if (auto value = ak_string_from_qstring(item->property("target_folder_id").toString()); !value.is_empty())
        m_selected_bookmark_menu_target_folder_id = AK::move(value);
}

bool BookmarksBar::is_bookmark_bar_action(QAction const& action) const
{
    auto id = action.property("id").toString();
    auto type = action.property("type").toString();
    return !id.isEmpty() && type == "bookmark";
}

size_t BookmarksBar::drop_target_index(QPoint const& position) const
{
    size_t index = 0;

    for (auto* action : actions()) {
        if (!is_bookmark_bar_action(*action))
            continue;

        auto* widget = widgetForAction(action);
        if (!widget)
            continue;

        auto geometry = widget->geometry();
        if (position.x() < geometry.center().x())
            return index;

        ++index;
    }

    return index;
}

Optional<int> BookmarksBar::drop_indicator_x_position(size_t index) const
{
    Vector<QWidget*> bookmark_widgets;

    for (auto* action : actions()) {
        if (!is_bookmark_bar_action(*action))
            continue;

        if (auto* widget = widgetForAction(action); widget && widget->isVisible())
            bookmark_widgets.append(widget);
    }

    if (bookmark_widgets.is_empty())
        return {};

    if (index == 0)
        return bookmark_widgets.first()->geometry().left();
    if (index >= bookmark_widgets.size())
        return bookmark_widgets.last()->geometry().right() + 1;
    return bookmark_widgets[index]->geometry().left();
}

QMenu& BookmarksBar::bookmarks_bar_context_menu()
{
    if (!m_bookmarks_bar_context_menu)
        m_bookmarks_bar_context_menu = create_application_menu(*this, WebView::Application::the().bookmarks_bar_context_menu());
    return *m_bookmarks_bar_context_menu;
}

QMenu& BookmarksBar::bookmark_context_menu()
{
    if (!m_bookmark_context_menu)
        m_bookmark_context_menu = create_application_menu(*this, WebView::Application::the().bookmark_context_menu());
    return *m_bookmark_context_menu;
}

QMenu& BookmarksBar::bookmark_folder_context_menu()
{
    if (!m_bookmark_folder_context_menu)
        m_bookmark_folder_context_menu = create_application_menu(*this, WebView::Application::the().bookmark_folder_context_menu());
    return *m_bookmark_folder_context_menu;
}

}
