/*
 * Copyright (c) 2026, Tim Flynn <trflynn89@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ScopeGuard.h>
#include <LibWebView/Application.h>
#include <LibWebView/BookmarkStore.h>
#include <UI/Qt/BookmarksBar.h>
#include <UI/Qt/ChromeStyle.h>
#include <UI/Qt/Icon.h>
#include <UI/Qt/Menu.h>
#include <UI/Qt/StringUtils.h>

#include <QAction>
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QIcon>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointer>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolButton>

namespace Ladybird {

static constexpr int BOOKMARK_BUTTON_MAX_WIDTH = 150;
static constexpr int BOOKMARK_BUTTON_ICON_SIZE = 16;
static constexpr int BOOKMARK_BUTTON_MIN_HEIGHT = 24;
static constexpr int BOOKMARK_BUTTON_VERTICAL_PADDING = 8;
static constexpr int BOOKMARK_BUTTON_HORIZONTAL_PADDING = 7;
static constexpr int BOOKMARK_BUTTON_ICON_TEXT_SPACING = 6;
static constexpr int BOOKMARK_BUTTON_TEXT_ELISION_PADDING = 2;

static constexpr char const* BOOKMARK_ITEM_PROPERTY = "bookmark_item";
static constexpr char const* BOOKMARK_CONTEXT_MENU_OPEN_PROPERTY = "bookmark_context_menu_open";

static constexpr auto LADYBIRD_BOOKMARK_MIME_TYPE = "application/x-ladybird-bookmark";

static QPointer<BookmarksBar> s_active_bookmark_drag_source;
static QString s_active_bookmark_dragged_id;

static QStyleOptionToolButton bookmark_button_style_option(QToolButton const& button)
{
    QStyleOptionToolButton option;
    option.initFrom(&button);

    option.rect = button.rect();
    option.icon = button.icon();
    option.iconSize = button.iconSize();
    option.text = button.text();
    option.toolButtonStyle = button.toolButtonStyle();
    option.arrowType = button.arrowType();
    option.subControls = QStyle::SC_ToolButton;

    if (button.arrowType() != Qt::NoArrow)
        option.features |= QStyleOptionToolButton::Arrow;

    switch (button.popupMode()) {
    case QToolButton::DelayedPopup:
        option.features |= QStyleOptionToolButton::PopupDelay;
        break;
    case QToolButton::MenuButtonPopup:
        option.features |= QStyleOptionToolButton::MenuButtonPopup;
        option.subControls |= QStyle::SC_ToolButtonMenu;
        break;
    case QToolButton::InstantPopup:
        option.features |= QStyleOptionToolButton::Menu;
        break;
    }

    if (button.autoRaise())
        option.state |= QStyle::State_AutoRaise;
    if (button.menu())
        option.features |= QStyleOptionToolButton::HasMenu;
    if (button.isDown())
        option.state |= QStyle::State_Sunken;
    if (button.property(BOOKMARK_CONTEXT_MENU_OPEN_PROPERTY).toBool())
        option.state |= QStyle::State_MouseOver;

    return option;
}

struct BookmarkButtonLayout {
    QRect content_rect;
    QRect icon_rect;
    QRect text_rect;
    int menu_indicator_width { 0 };
    int icon_width { 0 };
    int icon_text_spacing { 0 };
    int available_text_width { 0 };
    int preferred_width { 0 };
};
static BookmarkButtonLayout bookmark_button_layout(QToolButton const& button, QStyleOptionToolButton const& option, QString const& text, QRect const& button_rect)
{
    BookmarkButtonLayout layout;

    if (button.menu())
        layout.menu_indicator_width = button.style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &option, &button);

    layout.icon_width = button.icon().isNull() ? 0 : button.iconSize().width();
    layout.icon_text_spacing = layout.icon_width > 0 && !text.isEmpty() ? BOOKMARK_BUTTON_ICON_TEXT_SPACING : 0;

    layout.content_rect = button_rect;
    layout.content_rect.adjust(BOOKMARK_BUTTON_HORIZONTAL_PADDING, 0, -BOOKMARK_BUTTON_HORIZONTAL_PADDING, 0);
    layout.content_rect.adjust(0, 0, -layout.menu_indicator_width, 0);

    auto text_left = layout.content_rect.left() + layout.icon_width + layout.icon_text_spacing;
    layout.available_text_width = max(layout.content_rect.right() - text_left + 1, 0);
    layout.text_rect = QRect { text_left, layout.content_rect.top(), layout.available_text_width, layout.content_rect.height() };

    if (layout.icon_width > 0) {
        auto icon_size = option.iconSize;
        layout.icon_rect = QRect {
            layout.content_rect.left(),
            layout.content_rect.top() + ((layout.content_rect.height() - icon_size.height()) / 2),
            icon_size.width(),
            icon_size.height(),
        };
    }

    auto preferred_width = (BOOKMARK_BUTTON_HORIZONTAL_PADDING * 2)
        + layout.icon_width
        + layout.icon_text_spacing
        + button.fontMetrics().horizontalAdvance(text)
        + BOOKMARK_BUTTON_TEXT_ELISION_PADDING
        + layout.menu_indicator_width;
    layout.preferred_width = min(preferred_width, button_rect.width());

    return layout;
}

static void paint_bookmark_button(QToolButton& button)
{
    QStylePainter painter(&button);

    auto option = bookmark_button_style_option(button);
    auto layout = bookmark_button_layout(button, option, option.text, button.rect());

    auto frame_option = option;
    frame_option.icon = {};
    frame_option.text.clear();
    painter.drawComplexControl(QStyle::CC_ToolButton, frame_option);

    if (layout.icon_width > 0) {
        auto mode = button.isEnabled() ? QIcon::Normal : QIcon::Disabled;
        if (button.isEnabled() && (option.state & QStyle::State_MouseOver))
            mode = QIcon::Active;

        option.icon.paint(&painter, layout.icon_rect, Qt::AlignCenter, mode, button.isChecked() ? QIcon::On : QIcon::Off);
    }

    auto elided_text = button.fontMetrics().elidedText(option.text, Qt::ElideRight, layout.available_text_width);

    button.style()->drawItemText(
        &painter,
        layout.text_rect,
        Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
        option.palette,
        button.isEnabled(),
        elided_text,
        QPalette::ButtonText);
}

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
    setObjectName("LadybirdBookmarksBar");
    setIconSize({ BOOKMARK_BUTTON_ICON_SIZE, BOOKMARK_BUTTON_ICON_SIZE });
    setVisible(WebView::Application::settings().show_bookmarks_bar());
    setMovable(false);
    setFloatable(false);
    setAcceptDrops(true);
    update_chrome_style();

    installEventFilter(this);

    rebuild();
}

bool BookmarksBar::event(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        update_chrome_style();

    return QToolBar::event(event);
}

void BookmarksBar::update_chrome_style()
{
    if (m_is_updating_chrome_style)
        return;

    m_is_updating_chrome_style = true;
    setStyleSheet(ChromeStyle::bookmarks_bar_style_sheet(palette()));
    m_is_updating_chrome_style = false;
}

void BookmarksBar::rebuild()
{
    for (auto* action : actions()) {
        if (auto* menu = action->menu())
            menu->close();
    }

    clear();

    auto set_button_properties = [&](QToolButton* button, QString const& title) {
        button->setProperty(BOOKMARK_ITEM_PROPERTY, true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->ensurePolished();

        auto option = bookmark_button_style_option(*button);

        auto bookmark_button_height = max(BOOKMARK_BUTTON_MIN_HEIGHT, max(button->fontMetrics().height(), button->iconSize().height()) + BOOKMARK_BUTTON_VERTICAL_PADDING);
        auto max_size_rect = QRect { 0, 0, BOOKMARK_BUTTON_MAX_WIDTH, bookmark_button_height };

        auto layout = bookmark_button_layout(*button, option, title, max_size_rect);

        auto available_title_width = max(layout.available_text_width - BOOKMARK_BUTTON_TEXT_ELISION_PADDING, 0);
        auto text = button->fontMetrics().elidedText(title, Qt::ElideRight, available_title_width);
        button->setText(text);

        layout = bookmark_button_layout(*button, option, text, max_size_rect);

        button->setFixedWidth(layout.preferred_width);
        button->setMaximumWidth(BOOKMARK_BUTTON_MAX_WIDTH);
        button->setFixedHeight(bookmark_button_height);

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
    if (event->type() == QEvent::Paint) {
        if (auto* button = as_if<QToolButton>(object); button && button->property(BOOKMARK_ITEM_PROPERTY).toBool()) {
            paint_bookmark_button(*button);
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto& mouse_event = as<QMouseEvent>(*event);

        if (mouse_event.button() == Qt::LeftButton) {
            if (auto* button = as_if<QToolButton>(object); button && button->property(BOOKMARK_ITEM_PROPERTY).toBool()) {
                m_pressed_button = button;
                m_drag_start_position = mouse_event.pos();
                m_position_in_pressed_button = mouse_event.pos();
            }
            return handle_left_mouse_click(&mouse_event, object);
        }
        if (mouse_event.button() == Qt::MiddleButton)
            return handle_middle_mouse_click(&mouse_event, object);
        if (mouse_event.button() == Qt::RightButton)
            return handle_right_mouse_click(&mouse_event, object);
    }

    if (event->type() == QEvent::MouseMove) {
        auto& mouse_event = as<QMouseEvent>(*event);
        if (handle_mouse_move(&mouse_event, object))
            return true;
    }

    if (event->type() == QEvent::MouseButtonRelease)
        m_pressed_button = nullptr;

    return QToolBar::eventFilter(object, event);
}

bool BookmarksBar::handle_mouse_move(QMouseEvent* event, QObject* object)
{
    if (!m_pressed_button)
        return false;
    if (!event->buttons().testFlag(Qt::LeftButton))
        return false;

    auto* button = as_if<QToolButton>(object);
    if (!button || button != m_pressed_button)
        return false;

    if ((event->pos() - m_drag_start_position).manhattanLength() < QApplication::startDragDistance())
        return false;

    start_bookmark_drag(*button);
    return true;
}

void BookmarksBar::start_bookmark_drag(QToolButton& button)
{
    auto* action = button.defaultAction();
    if (!action)
        return;

    auto bookmark_id = action->property("id").toString();
    if (bookmark_id.isEmpty())
        return;

    QPointer<BookmarksBar> source { this };

    auto* drag = new QDrag(this);
    auto* mime_data = new QMimeData;
    mime_data->setData(LADYBIRD_BOOKMARK_MIME_TYPE, bookmark_id.toUtf8());
    drag->setMimeData(mime_data);

    auto button_rect = button.rect();
    QPixmap pixmap(button_rect.size() * devicePixelRatioF());
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setOpacity(0.75);
        button.render(&painter, QPoint(), QRegion(button_rect), QWidget::DrawChildren);
    }
    drag->setPixmap(pixmap);
    drag->setHotSpot(m_position_in_pressed_button);

    s_active_bookmark_drag_source = source;
    s_active_bookmark_dragged_id = bookmark_id;

    drag->exec(Qt::MoveAction, Qt::MoveAction);

    s_active_bookmark_drag_source = nullptr;
    s_active_bookmark_dragged_id.clear();
    m_pressed_button = nullptr;
    m_drop_indicator_index = -1;
    m_drop_indicator_folder_button = nullptr;
    update();
}

void BookmarksBar::dragEnterEvent(QDragEnterEvent* event)
{
    if (!s_active_bookmark_drag_source || !event->mimeData()->hasFormat(LADYBIRD_BOOKMARK_MIME_TYPE)) {
        event->ignore();
        return;
    }

    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void BookmarksBar::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_drop_indicator_index = -1;
    m_drop_indicator_folder_button = nullptr;
    update();
    QToolBar::dragLeaveEvent(event);
}

void BookmarksBar::dragMoveEvent(QDragMoveEvent* event)
{
    if (!s_active_bookmark_drag_source || !event->mimeData()->hasFormat(LADYBIRD_BOOKMARK_MIME_TYPE)) {
        event->ignore();
        return;
    }

    auto position = event->position().toPoint();

    QPointer<QToolButton> folder_button = folder_button_at(position);
    int insertion_index = folder_button ? -1 : insertion_index_at(position);

    if (folder_button != m_drop_indicator_folder_button || insertion_index != m_drop_indicator_index) {
        m_drop_indicator_folder_button = folder_button;
        m_drop_indicator_index = insertion_index;
        update();
    }

    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void BookmarksBar::dropEvent(QDropEvent* event)
{
    if (!s_active_bookmark_drag_source || !event->mimeData()->hasFormat(LADYBIRD_BOOKMARK_MIME_TYPE)) {
        event->ignore();
        return;
    }

    auto dragged_id_qstring = QString::fromUtf8(event->mimeData()->data(LADYBIRD_BOOKMARK_MIME_TYPE));
    auto dragged_id = ak_string_from_qstring(dragged_id_qstring);

    auto position = event->position().toPoint();
    auto* folder_button = folder_button_at(position);

    Optional<String> target_folder_id;
    size_t target_index = 0;

    if (folder_button && folder_button->defaultAction()) {
        auto folder_id = ak_string_from_qstring(folder_button->defaultAction()->property("id").toString());

        // Prevent dropping a folder into itself.
        if (folder_id == dragged_id) {
            event->ignore();
            m_drop_indicator_index = -1;
            m_drop_indicator_folder_button = nullptr;
            update();
            return;
        }

        target_folder_id = folder_id;
        if (auto item = WebView::Application::bookmark_store().find_item_by_id(folder_id); item.has_value() && item->is_folder())
            target_index = item->folder().children.size();
    } else {
        target_folder_id = {};
        target_index = static_cast<size_t>(max(0, insertion_index_at(position)));
    }

    m_drop_indicator_index = -1;
    m_drop_indicator_folder_button = nullptr;
    update();

    WebView::Application::bookmark_store().move_item(dragged_id, target_folder_id, target_index);

    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void BookmarksBar::paintEvent(QPaintEvent* event)
{
    QToolBar::paintEvent(event);

    if (m_drop_indicator_folder_button) {
        QPainter painter(this);
        auto rect = m_drop_indicator_folder_button->geometry();
        auto indicator_color = palette().color(QPalette::Highlight);
        indicator_color.setAlpha(80);
        painter.fillRect(rect, indicator_color);
        return;
    }

    if (m_drop_indicator_index < 0)
        return;

    auto bookmark_actions = actions();
    if (bookmark_actions.isEmpty())
        return;

    int indicator_x = 0;
    if (m_drop_indicator_index >= bookmark_actions.size()) {
        auto last_rect = actionGeometry(bookmark_actions.last());
        indicator_x = last_rect.right() + 2;
    } else {
        auto target_rect = actionGeometry(bookmark_actions.at(m_drop_indicator_index));
        indicator_x = target_rect.left() - 1;
    }
    indicator_x = max(1, min(width() - 2, indicator_x));

    QPainter painter(this);
    auto indicator_color = palette().color(QPalette::Highlight);
    indicator_color.setAlpha(220);
    painter.setPen(QPen(indicator_color, 2, Qt::SolidLine, Qt::RoundCap));

    auto vertical_inset = 3;
    painter.drawLine(QPointF(indicator_x, vertical_inset), QPointF(indicator_x, height() - vertical_inset));
}

int BookmarksBar::insertion_index_at(QPoint const& position) const
{
    auto bookmark_actions = actions();
    if (bookmark_actions.isEmpty())
        return 0;

    for (int i = 0; i < bookmark_actions.size(); ++i) {
        auto rect = actionGeometry(bookmark_actions.at(i));
        if (rect.isNull())
            continue;
        if (position.x() < rect.center().x())
            return i;
    }

    return bookmark_actions.size();
}

QToolButton* BookmarksBar::folder_button_at(QPoint const& position) const
{
    for (auto* action : actions()) {
        if (!action->menu())
            continue;
        auto rect = actionGeometry(action);
        if (rect.isNull() || !rect.contains(position))
            continue;
        if (auto* button = as_if<QToolButton>(widgetForAction(action)))
            return button;
    }
    return nullptr;
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

        auto set_button_context_menu_property = [button = QPointer { button }](bool open) {
            if (button) {
                button->setProperty(BOOKMARK_CONTEXT_MENU_OPEN_PROPERTY, open);
                button->update();
            }
        };

        ScopeGuard guard { [&]() { set_button_context_menu_property(false); } };
        set_button_context_menu_property(true);

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

void BookmarksBar::extract_item_properties(QObject* item)
{
    m_selected_bookmark_menu_item_id = ak_string_from_qstring(item->property("id").toString());
    m_selected_bookmark_menu_item_type = item->property("type").toString();

    if (auto value = ak_string_from_qstring(item->property("target_folder_id").toString()); !value.is_empty())
        m_selected_bookmark_menu_target_folder_id = AK::move(value);
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
