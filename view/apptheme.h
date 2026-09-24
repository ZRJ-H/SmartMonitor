#ifndef APPTHEME_H
#define APPTHEME_H

#include <QColor>
#include <QPalette>
#include <QString>
#include <QWidget>

// 同时设置调色板和 QSS：原生子控件、弹出列表及禁用状态也有明确的前景/背景。
namespace AppTheme {
inline QPalette mainPalette()
{
    QPalette palette;
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        const bool disabled = group == QPalette::Disabled;
        const QColor text(disabled ? "#a0aec0" : "#f1f5f9");
        palette.setColor(group, QPalette::Window, QColor("#20242a"));
        palette.setColor(group, QPalette::WindowText, text);
        palette.setColor(group, QPalette::Base, QColor(disabled ? "#252e3b" : "#151b24"));
        palette.setColor(group, QPalette::AlternateBase, QColor("#283240"));
        palette.setColor(group, QPalette::Text, text);
        palette.setColor(group, QPalette::Button, QColor(disabled ? "#252e3b" : "#334155"));
        palette.setColor(group, QPalette::ButtonText, text);
        palette.setColor(group, QPalette::Highlight, QColor(disabled ? "#344155" : "#24579a"));
        palette.setColor(group, QPalette::HighlightedText, text);
        palette.setColor(group, QPalette::PlaceholderText, QColor("#b5c2d3"));
        palette.setColor(group, QPalette::ToolTipBase, QColor("#f1f5f9"));
        palette.setColor(group, QPalette::ToolTipText, QColor("#172536"));
        palette.setColor(group, QPalette::Link, QColor("#93c5fd"));
        palette.setColor(group, QPalette::LinkVisited, QColor("#c4b5fd"));
        palette.setColor(group, QPalette::Light, QColor("#607087"));
        palette.setColor(group, QPalette::Mid, QColor("#475569"));
        palette.setColor(group, QPalette::Dark, QColor("#111820"));
        palette.setColor(group, QPalette::Shadow, QColor("#090d13"));
    }
    return palette;
}

inline QString mainWindowStyle()
{
    return QStringLiteral(R"QSS(
QWidget { color: #f1f5f9; font: 14px 'Microsoft YaHei'; }
QWidget:disabled { color: #a0aec0; }
QWidget#mainWidget, QStackedWidget#pageStack, QFrame#contentArea,
QFrame#playbackPage, QFrame#settingPage, QDialog, QMessageBox { background-color: #20242a; }
QFrame#topBar { background-color: #252a31; border-bottom: 1px solid #536174; }
QFrame#devicePanel, QFrame#pageCard, QFrame#monitorPanel {
    background-color: #292e36; border: 1px solid #536174; border-radius: 6px;
}
QLabel, QCheckBox, QRadioButton, QGroupBox { color: #f1f5f9; background-color: transparent; }
QLabel:disabled, QCheckBox:disabled, QRadioButton:disabled, QGroupBox:disabled { color: #a0aec0; }
QLabel#titleLabel { font-size: 30px; }
QLabel#timeLabel { font: 18px Consolas; }
QLabel#loginLabel { color: #cbd5e1; }
QLabel#pageTitle { font-size: 26px; }
QLabel#sectionTitle { font-size: 21px; }
QLabel#settingText { font-size: 16px; }
QFrame#separator { background-color: #65758a; max-height: 1px; }
QFrame#videoArea { background-color: #11151a; }
QLabel#videoWindow, QLabel#playbackWindow {
    background-color: #090b0e; color: #cbd5e1; font-size: 20px; border: 1px solid #536174;
}
QPushButton, QToolButton {
    color: #f1f5f9; background-color: #334155; border: 1px solid #64748b;
    border-radius: 5px; padding: 6px 12px;
}
QPushButton:hover, QToolButton:hover { background-color: #405570; border-color: #93c5fd; }
QPushButton:pressed, QToolButton:pressed { background-color: #1e416e; }
QPushButton:checked, QToolButton:checked { color: #ffffff; background-color: #24579a; border-color: #93c5fd; }
QPushButton:focus, QToolButton:focus { border: 2px solid #93c5fd; }
QPushButton#navButton, QPushButton#imagesNavigation, QPushButton#logsNavigation {
    background-color: #151b24; font-size: 16px; border-radius: 8px;
}
QPushButton#navButton:hover, QPushButton#imagesNavigation:hover, QPushButton#logsNavigation:hover {
    background-color: #405570;
}
QPushButton#navButton:checked { background-color: #24579a; border-color: #93c5fd; }
QPushButton#loginButton { background-color: #24579a; border: 2px solid #6fa5ee; border-radius: 33px; font-weight: bold; }
QPushButton#loginButton:hover { background-color: #2e64a9; }
QPushButton#videoRecordRefreshButton, QPushButton#refreshCamerasButton,
QPushButton#saveChannelConfigButton, QPushButton#saveRecordingSettingsButton,
QPushButton#saveDetectionSettingsButton { color: #ffffff; background-color: #24579a; border-color: #6fa5ee; }
QPushButton#videoRecordRefreshButton:hover, QPushButton#refreshCamerasButton:hover,
QPushButton#saveChannelConfigButton:hover, QPushButton#saveRecordingSettingsButton:hover,
QPushButton#saveDetectionSettingsButton:hover { background-color: #2e64a9; }
QPushButton:disabled, QToolButton:disabled, QPushButton#navButton:disabled,
QPushButton#imagesNavigation:disabled, QPushButton#logsNavigation:disabled,
QPushButton#loginButton:disabled, QPushButton#videoRecordRefreshButton:disabled,
QPushButton#refreshCamerasButton:disabled, QPushButton#saveChannelConfigButton:disabled,
QPushButton#saveRecordingSettingsButton:disabled, QPushButton#saveDetectionSettingsButton:disabled {
    color: #a0aec0; background-color: #252e3b; border-color: #475569;
}
QLineEdit, QComboBox, QAbstractSpinBox, QTextEdit, QPlainTextEdit {
    color: #f1f5f9; background-color: #151b24; border: 1px solid #64748b;
    border-radius: 4px; padding: 5px; selection-background-color: #24579a; selection-color: #ffffff;
}
QLineEdit { placeholder-text-color: #b5c2d3; }
QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border: 1px solid #93c5fd;
}
QLineEdit:disabled, QComboBox:disabled, QAbstractSpinBox:disabled, QTextEdit:disabled, QPlainTextEdit:disabled {
    color: #a0aec0; background-color: #252e3b; border-color: #475569;
}
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
    width: 18px; background-color: #334155; border: 1px solid #536174;
}
QAbstractSpinBox::up-arrow { width: 12px; height: 12px; image: url(:/theme/up.png); }
QAbstractSpinBox::down-arrow, QComboBox::down-arrow { width: 12px; height: 12px; image: url(:/theme/down.png); }
QComboBox::drop-down { width: 22px; background-color: #334155; border-left: 1px solid #64748b; }
QAbstractSpinBox::up-button:disabled, QAbstractSpinBox::down-button:disabled,
QComboBox::drop-down:disabled { background-color: #252e3b; }
QAbstractItemView {
    color: #f1f5f9; background-color: #151b24; alternate-background-color: #283240;
    border: 1px solid #536174; selection-background-color: #24579a; selection-color: #ffffff;
}
QAbstractItemView:disabled { color: #a0aec0; background-color: #252e3b; }
QAbstractItemView::item:selected { color: #ffffff; background-color: #24579a; }
QAbstractItemView::item:hover:!selected { background-color: #35465d; }
QAbstractItemView::item:selected:disabled { color: #a0aec0; background-color: #344155; }
QListWidget#deviceList, QListWidget#recordList { font-size: 16px; outline: none; }
QListWidget#deviceList::item, QListWidget#recordList::item { min-height: 40px; padding-left: 8px; }
QTableView { gridline-color: #536174; }
QHeaderView::section, QTableCornerButton::section {
    color: #f1f5f9; background-color: #334155; border: 1px solid #536174; padding: 7px;
}
QHeaderView::section:disabled { color: #a0aec0; background-color: #252e3b; }
QMenu { color: #f1f5f9; background-color: #202c3a; border: 1px solid #64748b; selection-background-color: #24579a; selection-color: #ffffff; }
QMenu::item { padding: 6px 24px; }
QMenu::item:selected { color: #ffffff; background-color: #24579a; }
QMenu::item:disabled { color: #a0aec0; }
QMenu::separator { height: 1px; background-color: #64748b; margin: 4px 8px; }
QCheckBox, QRadioButton { spacing: 7px; }
QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #b5c2d3; border-radius: 3px; background-color: #151b24; }
QCheckBox::indicator:checked { image: url(:/theme/check.png); background-color: #93c5fd; border-color: #93c5fd; }
QCheckBox::indicator:disabled { border-color: #64748b; background-color: #252e3b; }
QCheckBox::indicator:checked:disabled { image: url(:/theme/check.png); background-color: #a0aec0; border-color: #a0aec0; }
QScrollBar:vertical { background-color: #151b24; width: 18px; margin: 18px 0; border: none; }
QScrollBar::handle:vertical { background-color: #64748b; min-height: 28px; border-radius: 4px; margin: 0 3px; }
QScrollBar::handle:vertical:hover { background-color: #93c5fd; }
QScrollBar::handle:vertical:pressed { background-color: #b5d7ff; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    background-color: #334155; height: 18px; border: none; subcontrol-origin: margin;
}
QScrollBar::add-line:vertical { subcontrol-position: bottom; }
QScrollBar::sub-line:vertical { subcontrol-position: top; }
QScrollBar::up-arrow:vertical { width: 12px; height: 12px; image: url(:/theme/up.png); }
QScrollBar::down-arrow:vertical { width: 12px; height: 12px; image: url(:/theme/down.png); }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background-color: #151b24; }
QSlider::groove:horizontal { height: 6px; background-color: #64748b; border-radius: 3px; }
QSlider::sub-page:horizontal { background-color: #93c5fd; border-radius: 3px; }
QSlider::handle:horizontal { width: 14px; margin: -5px 0; border-radius: 7px; background-color: #e2efff; }
QSlider::handle:horizontal:disabled { background-color: #a0aec0; }
QToolTip { color: #172536; background-color: #f1f5f9; border: 1px solid #64748b; padding: 4px; }
)QSS");
}

inline QPalette loginPalette()
{
    QPalette palette;
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        const QColor text(group == QPalette::Disabled ? "#64748b" : "#172536");
        palette.setColor(group, QPalette::Window, QColor("#f8fafc"));
        palette.setColor(group, QPalette::WindowText, text);
        palette.setColor(group, QPalette::Base, QColor("#ffffff"));
        palette.setColor(group, QPalette::Text, text);
        palette.setColor(group, QPalette::Button, QColor("#e2e8f0"));
        palette.setColor(group, QPalette::ButtonText, text);
        palette.setColor(group, QPalette::PlaceholderText, QColor("#536579"));
        palette.setColor(group, QPalette::Highlight, QColor("#24579a"));
        palette.setColor(group, QPalette::HighlightedText, QColor("#ffffff"));
        palette.setColor(group, QPalette::ToolTipBase, QColor("#ffffff"));
        palette.setColor(group, QPalette::ToolTipText, QColor("#172536"));
    }
    return palette;
}

inline QString loginStyle()
{
    return QStringLiteral(R"QSS(
QWidget { color: #172536; font: 15px 'Microsoft YaHei'; }
QFrame#formPanel { background-color: #f8fafc; border: 1px solid #94a3b8; border-radius: 14px; }
QLabel { color: #172536; background-color: transparent; border: none; }
QLabel#loginTitle { color: #172536; font-size: 28px; font-weight: bold; }
QLabel#tipLabel { color: #45566b; font-size: 13px; }
QLineEdit { min-height: 38px; border: 1px solid #64748b; border-radius: 6px; padding: 0 10px; background-color: #ffffff; color: #172536; selection-background-color: #24579a; selection-color: #ffffff; }
QLineEdit { placeholder-text-color: #536579; }
QLineEdit:focus { border: 2px solid #24579a; }
QPushButton { min-height: 38px; border: 1px solid #64748b; border-radius: 6px; padding: 0 8px; background-color: #e2e8f0; color: #172536; }
QPushButton:hover { background-color: #cbd5e1; }
QPushButton:focus { border: 2px solid #24579a; }
QPushButton:checked { color: #ffffff; background-color: #24579a; }
QPushButton#primaryButton { color: #ffffff; background-color: #24579a; border-color: #24579a; }
QPushButton#primaryButton:hover { background-color: #1e477e; }
QWidget:disabled, QLabel:disabled { color: #64748b; }
QLineEdit:disabled, QPushButton:disabled, QPushButton#primaryButton:disabled { color: #64748b; background-color: #e2e8f0; border-color: #94a3b8; }
QDialog, QMessageBox { background-color: #f8fafc; }
QToolTip { color: #172536; background-color: #ffffff; border: 1px solid #64748b; padding: 4px; }
)QSS");
}

inline void applyMain(QWidget *widget)
{
    widget->setPalette(mainPalette());
    widget->setAutoFillBackground(true);
    widget->setStyleSheet(mainWindowStyle());
}

inline void applyLogin(QWidget *widget)
{
    // 登录背景可能是图片，保留 Window 画刷，只统一控件的其它颜色角色。
    QPalette palette = loginPalette();
    palette.setBrush(QPalette::Window, widget->palette().brush(QPalette::Window));
    widget->setPalette(palette);
    widget->setStyleSheet(loginStyle());
}
}
#endif // APPTHEME_H