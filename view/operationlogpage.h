// 【阅读导引】
// 日志界面：在操作日志与异常事件之间切换；只读详情窗口支持复制完整文本。
// 切换查询、隐藏或禁用页面时关闭详情窗口，避免保留过期或失去权限的数据。

#ifndef OPERATIONLOGPAGE_H
#define OPERATIONLOGPAGE_H
#include <QWidget>
#include <QPointer>
#include "../service/operationlogservice.h"
class QDateTimeEdit;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QLabel;
class QComboBox;
class QTabWidget;
class QPushButton;
class QDialog;
// 日志页只负责用户输入与展示；所有 SQL 保留在 Model 中。
class OperationLogPage : public QWidget {
    Q_OBJECT
public:
    explicit OperationLogPage(const QSqlDatabase &database,QWidget *parent=nullptr);
public slots:
    void reload();
signals:
    void playbackRequested(qint64 videoId,qint64 offsetMs);
    void eventImagesRequested(qint64 exceptionId);
protected:
    void changeEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
private slots:
    void showSelectedDetails();
private:
    void updateDetailsAction();
    QPushButton *detailsButton = nullptr;
    QPointer<QDialog> detailsDialog;
    QTabWidget *tabs;
    QTableWidget *events;
    QComboBox *channel;
    QList<EventLogEntry> currentEvents;
    OperationLogService service;
    QDateTimeEdit *from,*to;
    QSpinBox *admin;
    QLineEdit *function;
    QTableWidget *table;
    QLabel *status;
    int page=1;
};
#endif
