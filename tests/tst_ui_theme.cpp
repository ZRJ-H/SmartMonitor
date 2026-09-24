#include <QtTest>
#include "../view/apptheme.h"
#include "../view/loginwidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QFontDatabase>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollBar>
#include <QSqlDatabase>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>
#include <memory>

namespace {
double luminance(const QColor &color)
{
    const auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}
double contrast(const QColor &a, const QColor &b)
{
    const double x = luminance(a), y = luminance(b);
    return (qMax(x, y) + 0.05) / (qMin(x, y) + 0.05);
}
QColor dominantBackground(QWidget *widget)
{
    QWidget *window = widget->window();
    const QPixmap rendered = window->grab();
    const qreal scale = rendered.devicePixelRatio();
    const QPoint origin = widget->mapTo(window, QPoint());
    const QRect region(qRound(origin.x() * scale), qRound(origin.y() * scale),
                       qRound(widget->width() * scale), qRound(widget->height() * scale));
    const QImage pixels = rendered.toImage().copy(region).convertToFormat(QImage::Format_RGB32);
    QHash<QRgb, int> counts;
    for (int y = 4; y < pixels.height() - 4; ++y)
        for (int x = 4; x < pixels.width() - 4; ++x)
            ++counts[pixels.pixel(x, y)];
    QRgb most = 0; int count = 0;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        if (it.value() > count) { most = it.key(); count = it.value(); }
    return QColor::fromRgb(most);
}
QString contrastFailure(const QString &control, const QColor &foreground, const QColor &background)
{
    return QString("%1 foreground %2 / rendered background %3 contrast %4")
            .arg(control, foreground.name(), background.name()).arg(contrast(foreground, background), 0, 'f', 2);
}
QWidget *makeControl(const QString &kind, QWidget *parent)
{
    if (kind == "label") return new QLabel("置信度阈值 / Confidence", parent);
    if (kind == "checkbox") return new QCheckBox("启用智能检测", parent);
    if (kind == "edit") return new QLineEdit("C:/Monitoring/records", parent);
    if (kind == "spin") return new QSpinBox(parent);
    if (kind == "doubleSpin") return new QDoubleSpinBox(parent);
    if (kind == "date") return new QDateTimeEdit(QDateTime::currentDateTime(), parent);
    if (kind == "combo") { auto *combo = new QComboBox(parent); combo->addItem("通道 1"); return combo; }
    if (kind == "list") { auto *list = new QListWidget(parent); list->addItem("录像 2026-09-24"); return list; }
    if (kind == "table") { auto *table = new QTableWidget(1, 1, parent); table->setItem(0, 0, new QTableWidgetItem("告警事件")); return table; }
    auto *button = new QPushButton("保存设置 / Save", parent);
    if (kind != "button") button->setObjectName(kind);
    return button;
}
}

class UiThemeTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
#ifdef Q_OS_WIN
        // Windows offscreen 插件不自动加载系统中文字库；只影响离线截图，不改产品字体。
        if (QGuiApplication::platformName() == "offscreen")
            QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
#endif
    }
    void controlsHaveReadableNormalAndDisabledText_data()
    {
        QTest::addColumn<QString>("kind");
        for (const QString kind : {"label", "checkbox", "edit", "spin", "doubleSpin", "date", "combo", "list", "table", "button", "navButton", "controlButton", "previewSnapshotButton", "backupDatabaseButton", "videoRecordRefreshButton", "refreshCamerasButton", "imagesNavigation", "logsNavigation", "saveRecordingSettingsButton", "saveDetectionSettingsButton", "saveChannelConfigButton", "loginButton"})
            QTest::newRow(qPrintable(kind)) << kind;
    }
    void controlsHaveReadableNormalAndDisabledText()
    {
        QFETCH(QString, kind);
        QWidget root; root.setObjectName("mainWidget");
        auto *layout = new QVBoxLayout(&root);
        auto *card = new QFrame; card->setObjectName("pageCard"); layout->addWidget(card);
        auto *cardLayout = new QVBoxLayout(card);
        QWidget *control = makeControl(kind, card); cardLayout->addWidget(control);
        root.resize(480, 200); AppTheme::applyMain(&root);
        root.show(); QTest::qWait(10); control->ensurePolished();
        const auto role = qobject_cast<QAbstractButton *>(control) ? QPalette::ButtonText
                : (qobject_cast<QLabel *>(control) ? QPalette::WindowText : QPalette::Text);
        const QColor normal = control->palette().color(QPalette::Active, role);
        QWidget *painted = control;
        if (auto *view = qobject_cast<QAbstractItemView *>(control)) painted = view->viewport();
        const QColor background = dominantBackground(painted);
        QVERIFY2(contrast(normal, background) >= 4.5, qPrintable(contrastFailure(kind, normal, background)));
        const QColor inactive = control->palette().color(QPalette::Inactive, role);
        QVERIFY2(contrast(inactive, background) >= 4.5, qPrintable(contrastFailure(kind + " inactive", inactive, background)));
        control->setEnabled(false); QCoreApplication::processEvents();
        const QColor disabled = control->palette().color(QPalette::Disabled, role);
        const QColor disabledBackground = dominantBackground(painted);
        QVERIFY2(contrast(disabled, disabledBackground) >= 3.0, qPrintable(contrastFailure(kind + " disabled", disabled, disabledBackground)));
        QVERIFY2(disabled != normal, "Disabled text must be visibly distinct from enabled text");
    }
    void placeholdersUseReadableOpaqueColors_data()
    {
        QTest::addColumn<bool>("loginTheme"); QTest::addColumn<bool>("disabled");
        QTest::newRow("main") << false << false;
        QTest::newRow("main disabled") << false << true;
        QTest::newRow("login") << true << false;
        QTest::newRow("login disabled") << true << true;
    }
    void placeholdersUseReadableOpaqueColors()
    {
        QFETCH(bool, loginTheme); QFETCH(bool, disabled);
        QWidget root; auto *layout = new QVBoxLayout(&root);
        auto *edit = new QLineEdit; edit->setPlaceholderText("MMMMMMMM / 请选择文件");
        edit->setMinimumHeight(36); edit->setDisabled(disabled); layout->addWidget(edit);
        if (loginTheme) AppTheme::applyLogin(&root); else AppTheme::applyMain(&root);
        root.resize(360, 90); root.show(); QTest::qWait(10);
        const QColor expected(loginTheme ? "#536579" : "#b5c2d3");
        const auto pixels = edit->grab().toImage(); int opaqueTextPixels = 0;
        for (int y = 5; y < pixels.height() - 5; ++y)
            for (int x = 10; x < pixels.width() - 10; ++x) {
                const QColor actual = pixels.pixelColor(x, y);
                if (qAbs(actual.red() - expected.red()) <= 3 && qAbs(actual.green() - expected.green()) <= 3
                        && qAbs(actual.blue() - expected.blue()) <= 3) ++opaqueTextPixels;
            }
        QVERIFY2(opaqueTextPixels >= 8, "Placeholder was faded by the style instead of using its opaque readable color");
        QVERIFY(contrast(expected, dominantBackground(edit)) >= 4.5);
    }
    void verticalScrollBarUsesVisibleDarkTrackAndHandle()
    {
        QWidget root; auto *layout = new QVBoxLayout(&root);
        auto *bar = new QScrollBar(Qt::Vertical); bar->setRange(0, 100); bar->setPageStep(10);
        bar->setFixedSize(18, 240); layout->addWidget(bar); AppTheme::applyMain(&root);
        root.show(); QTest::qWait(10); const auto pixels = bar->grab().toImage();
        int trackPixels = 0, handlePixels = 0;
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x) {
                if (pixels.pixelColor(x, y) == QColor("#151b24")) ++trackPixels;
                if (pixels.pixelColor(x, y) == QColor("#64748b")) ++handlePixels;
            }
        QVERIFY2(trackPixels > 100, "Vertical scrollbar kept its bright system track");
        QVERIFY2(handlePixels > 100, "Vertical scrollbar handle is not visible on its track");
        QVERIFY(contrast(QColor("#64748b"), QColor("#151b24")) >= 3.0);
    }
    void selectedRowsAndPopupPaletteRemainReadable()
    {
        QWidget root; AppTheme::applyMain(&root);
        QListWidget list(&root); list.addItem("已选中录像"); list.setCurrentRow(0); list.ensurePolished();
        QTableWidget table(1, 1, &root); table.setItem(0, 0, new QTableWidgetItem("事件")); table.ensurePolished();
        QComboBox combo(&root); combo.addItem("通道 1"); combo.view()->ensurePolished();
        QMenu menu(&root); menu.addAction("导出图片"); menu.ensurePolished();
        for (QWidget *widget : QList<QWidget *>{&list, &table, combo.view(), &menu}) {
            for (const auto group : {QPalette::Active, QPalette::Inactive}) {
                const auto p = widget->palette();
                QVERIFY2(contrast(p.color(group, QPalette::HighlightedText), p.color(group, QPalette::Highlight)) >= 4.5,
                         qPrintable(QString("Selection contrast fails for %1").arg(widget->metaObject()->className())));
            }
        }
        table.horizontalHeader()->ensurePolished();
        const QPalette p = table.horizontalHeader()->palette();
        QVERIFY(contrast(p.color(QPalette::ButtonText), p.color(QPalette::Button)) >= 4.5);
    }
    void childDialogsInheritReadableTheme()
    {
        QWidget root; AppTheme::applyMain(&root);
        QDialog dialog(&root); auto *layout = new QVBoxLayout(&dialog);
        auto *label = new QLabel("确定删除选中的图片？", &dialog); layout->addWidget(label);
        layout->addWidget(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog));
        dialog.show(); QTest::qWait(10);
        const auto fg = label->palette().color(QPalette::WindowText), bg = dominantBackground(label);
        QVERIFY2(contrast(fg, bg) >= 4.5, qPrintable(contrastFailure("dialog label", fg, bg)));
    }
    void renderRepresentativeTheme()
    {
        const QString screenshot = qEnvironmentVariable("SMART_MONITOR_UI_SCREENSHOT");
        if (screenshot.isEmpty()) return;
        QWidget root; root.setObjectName("mainWidget"); root.resize(1120, 710);
        auto *layout = new QVBoxLayout(&root); layout->setContentsMargins(24, 24, 24, 24); layout->setSpacing(18);
        auto *title = new QLabel("安防监控系统管理平台"); title->setObjectName("titleLabel"); layout->addWidget(title);
        auto *navigation = new QHBoxLayout;
        for (const QString text : {"监控", "回放", "设置", "图片", "日志"}) {
            auto *button = new QPushButton(text); button->setObjectName("navButton"); button->setCheckable(true);
            button->setChecked(text == "设置"); navigation->addWidget(button);
        }
        layout->addLayout(navigation);
        auto *card = new QFrame; card->setObjectName("pageCard"); layout->addWidget(card);
        auto *form = new QFormLayout(card); form->setContentsMargins(22, 22, 22, 22); form->setSpacing(14);
        form->addRow("模型文件", new QLineEdit("C:/QtProjects/day20_smart_monitor/models/yolov8n.onnx"));
        auto *confidence = new QDoubleSpinBox; confidence->setRange(0, 1); confidence->setValue(0.5);
        form->addRow("置信度阈值", confidence);
        auto *enabled = new QCheckBox("启用智能检测"); enabled->setChecked(true); form->addRow("检测设置", enabled);
        auto *disabled = new QCheckBox("此功能暂不可用"); disabled->setEnabled(false); form->addRow("禁用状态", disabled);
        form->addRow("开始时间", new QDateTimeEdit(QDateTime(QDate(2026, 9, 24), QTime(9, 0))));
        auto *actions = new QHBoxLayout;
        auto *save = new QPushButton("保存检测设置"); save->setObjectName("saveDetectionSettingsButton"); actions->addWidget(save);
        auto *inactive = new QPushButton("保存录像设置"); inactive->setObjectName("saveRecordingSettingsButton"); inactive->setEnabled(false); actions->addWidget(inactive);
        form->addRow(actions);
        auto *table = new QTableWidget(3, 3); table->setHorizontalHeaderLabels({"通道", "时间", "状态"});
        for (int row = 0; row < 3; ++row) {
            table->setItem(row, 0, new QTableWidgetItem(QString("通道 %1").arg(row + 1)));
            table->setItem(row, 1, new QTableWidgetItem("2026-09-24 09:00:00"));
            table->setItem(row, 2, new QTableWidgetItem(row == 0 ? "录像已保存" : "等待预览"));
        }
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setSelectionBehavior(QAbstractItemView::SelectRows); table->selectRow(0);
        table->setAlternatingRowColors(true); layout->addWidget(table);
        AppTheme::applyMain(&root); root.show(); QTest::qWait(10);
        QVERIFY(root.grab().save(QFileInfo(screenshot).dir().filePath("main-theme-after.png")));
    }
    void loginTextStaysOnOpaqueCard()
    {
        const auto connections = QSqlDatabase::connectionNames();
        LoginWidget login; login.show(); QTest::qWait(10);
        auto *card = login.findChild<QFrame *>("formPanel"); QVERIFY(card);
        auto *title = login.findChild<QLabel *>("loginTitle"); QVERIFY(title);
        QVERIFY2(card->isAncestorOf(title), "Login title must also be on the opaque card, never over a variable photo");
        const QColor cardColor = card->palette().color(QPalette::Window);
        QCOMPARE(cardColor.alpha(), 255);
        for (auto *label : card->findChildren<QLabel *>()) {
            if (label->text().isEmpty()) continue; // CAPTCHA paints its own image.
            const auto fg = label->palette().color(QPalette::WindowText), bg = dominantBackground(label);
            QVERIFY2(contrast(fg, bg) >= 4.5, qPrintable(contrastFailure(label->text(), fg, bg)));
        }
        for (auto *edit : login.findChildren<QLineEdit *>()) {
            const auto p = edit->palette();
            QVERIFY(contrast(p.color(QPalette::Text), p.color(QPalette::Base)) >= 4.5);
            QVERIFY(contrast(p.color(QPalette::PlaceholderText), p.color(QPalette::Base)) >= 4.5);
        }
        QCOMPARE(QSqlDatabase::connectionNames(), connections); // Constructing and rendering must not access data.
        const QString artifact = qEnvironmentVariable("SMART_MONITOR_UI_SCREENSHOT");
        if (!artifact.isEmpty()) QVERIFY(login.grab().save(artifact));
    }
};
QTEST_MAIN(UiThemeTests)
#include "tst_ui_theme.moc"