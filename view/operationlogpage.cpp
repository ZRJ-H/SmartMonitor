// 【阅读导引】
// 日志界面：在操作日志与异常事件之间切换；只读详情窗口支持复制完整文本。
// 切换查询、隐藏或禁用页面时关闭详情窗口，避免保留过期或失去权限的数据。

#include "operationlogpage.h"
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QHideEvent>
#include <QSignalBlocker>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
OperationLogPage::OperationLogPage(const QSqlDatabase &db,QWidget *parent):QWidget(parent),service(db) {
    setObjectName("operationLogPage");
    auto root=new QVBoxLayout(this);auto filters=new QHBoxLayout;
    from=new QDateTimeEdit(QDateTime(QDate(2000,1,1),QTime(0,0)));
    to=new QDateTimeEdit(QDateTime(QDate(2100,1,1),QTime(0,0)));
    from->setDisplayFormat("yyyy-MM-dd HH:mm:ss");to->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    admin=new QSpinBox;admin->setRange(0,2147483647);admin->setSpecialValueText("全部管理员");
    function=new QLineEdit;function->setPlaceholderText("功能名称（留空为全部）");
    auto search=new QPushButton("查询");
    for(auto widget:QList<QWidget*>{from,to,admin,function,search})filters->addWidget(widget);
    channel=new QComboBox;channel->addItem("全部通道",0);for(int i=1;i<=4;++i)channel->addItem(QString("通道%1").arg(i),i);
    filters->addWidget(channel);root->addLayout(filters);
    table=new QTableWidget(0,5);table->setObjectName("operationLogTable");
    table->setHorizontalHeaderLabels({"编号","时间","管理员","功能","详情"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabs=new QTabWidget;
    events=new QTableWidget(0,5);events->setObjectName("eventLogTable");events->setHorizontalHeaderLabels({"事件","时间","通道","状态","录像"});
    events->setSelectionBehavior(QAbstractItemView::SelectRows);events->setEditTriggers(QAbstractItemView::NoEditTriggers);
    events->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tabs->addTab(table,"操作日志");tabs->addTab(events,"异常事件");root->addWidget(tabs);
    status=new QLabel;status->setWordWrap(true);root->addWidget(status);
    auto actions=new QHBoxLayout;auto previous=new QPushButton("上一页");auto next=new QPushButton("下一页");
    auto remove=new QPushButton("删除时间范围内的日志");
    auto removeSelected=new QPushButton("删除所选日志");auto images=new QPushButton("关联图片");auto play=new QPushButton("关联录像");
    detailsButton=new QPushButton("查看详情");
    detailsButton->setObjectName("viewLogDetailsButton");
    detailsButton->setEnabled(false);
    actions->addWidget(detailsButton);
    actions->addWidget(removeSelected);actions->addWidget(images);actions->addWidget(play);
    connect(detailsButton,&QPushButton::clicked,this,&OperationLogPage::showSelectedDetails);
    connect(table,&QTableWidget::itemSelectionChanged,this,&OperationLogPage::updateDetailsAction);
    connect(table,&QTableWidget::currentCellChanged,this,&OperationLogPage::updateDetailsAction);
    connect(table,&QTableWidget::cellDoubleClicked,this,[this](int row,int column){
        table->setCurrentCell(row,column);
        showSelectedDetails();
    });
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto shortcut=new QShortcut(QKeySequence(key),table);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut,&QShortcut::activated,this,&OperationLogPage::showSelectedDetails);
    }
    connect(tabs,&QTabWidget::currentChanged,this,[this,remove,removeSelected](int index){
        remove->setEnabled(index==0);removeSelected->setEnabled(index==0);page=1;reload();
    });
    connect(removeSelected,&QPushButton::clicked,this,[this]{
        if(!isEnabled()||tabs->currentIndex()!=0)return;
        QList<qint64> ids;for(const auto &index:table->selectionModel()->selectedRows())ids<<table->item(index.row(),0)->text().toLongLong();
        if(ids.isEmpty()||QMessageBox::question(this,"确认删除","删除选中的操作日志？")!=QMessageBox::Yes)return;
        QString error;if(!service.deleteIds(ids,true,error)){status->setText(error);return;}reload();
    });
    connect(images,&QPushButton::clicked,this,[this]{
        const int row=events->currentRow();if(tabs->currentIndex()==1&&row>=0&&row<currentEvents.size())emit eventImagesRequested(currentEvents[row].exceptionId);
    });
    connect(events,&QTableWidget::currentCellChanged,this,[this](int row,int,int,int){
        if(isEnabled() && tabs->currentIndex()==1 && row>=0 && row<currentEvents.size()
                && currentEvents[row].videoId>0) emit playbackRequested(currentEvents[row].videoId,0);
    });
    connect(play,&QPushButton::clicked,this,[this]{
        const int row=events->currentRow();if(tabs->currentIndex()==1&&row>=0&&row<currentEvents.size()&&currentEvents[row].videoId>0)emit playbackRequested(currentEvents[row].videoId,0);
    });
    actions->addWidget(previous);actions->addWidget(next);actions->addStretch();actions->addWidget(remove);root->addLayout(actions);
    connect(search,&QPushButton::clicked,this,[this]{page=1;reload();});
    connect(previous,&QPushButton::clicked,this,[this]{if(page>1){--page;reload();}});
    connect(next,&QPushButton::clicked,this,[this]{if((tabs->currentIndex()==0?table:events)->rowCount()==50){++page;reload();}});
    connect(remove,&QPushButton::clicked,this,[this]{
        if(!isEnabled())return;
        if(QMessageBox::question(this,"确认删除","将删除所选时间范围内全部操作日志，是否继续？")!=QMessageBox::Yes)return;
        QString error;
        if(!service.deleteRange(from->dateTime(),to->dateTime(),true,error)){status->setText(error);return;}
        page=1;reload();
    });
}
// 关闭旧详情并按当前标签查询日志或事件，重建表格避免使用过期选中行。
void OperationLogPage::reload() {
    if(detailsDialog)detailsDialog->close();
    table->clearSelection();
    table->setCurrentCell(-1,-1);
    updateDetailsAction();
    if(tabs->currentIndex()==1){
        QString error;
        if(!service.queryEvents(channel->currentData().toInt(),from->dateTime(),to->dateTime(),page,50,currentEvents,error)){
            status->setText(error);return;
        }
        const QSignalBlocker blocker(events);
        events->clearSelection();events->setCurrentCell(-1,-1);
        events->setRowCount(currentEvents.size());
        for(int i=0;i<currentEvents.size();++i){
            const auto &e=currentEvents[i];const QStringList values={QString::number(e.exceptionId),e.eventTime.toString("yyyy-MM-dd HH:mm:ss"),QString::number(e.channelId),e.status,QString::number(e.videoId)};
            for(int c=0;c<5;++c){events->setItem(i,c,new QTableWidgetItem(values[c]));events->item(i,c)->setToolTip(QString("事件 %1\n通道 %2\n时间 %3\n状态 %4\n选中后直接回放").arg(e.exceptionId).arg(e.channelId).arg(e.eventTime.toString("yyyy-MM-dd HH:mm:ss")).arg(e.status));}
        }
        status->setText(QString("事件第 %1 页，%2 条").arg(page).arg(currentEvents.size()));return;
    }
    OperationLogFilter filter;filter.from=from->dateTime();filter.to=to->dateTime();filter.adminId=admin->value();filter.operationFunc=function->text().trimmed();
    QList<OperationLogEntry> rows;QString error;
    if(!service.query(filter,rows,error,page,50)){status->setText(error);return;}
    table->setRowCount(rows.size());
    for(int i=0;i<rows.size();++i){
        const auto &entry=rows[i];QStringList values={QString::number(entry.id),entry.operationTime.toString("yyyy-MM-dd HH:mm:ss"),QString::number(entry.adminId),entry.operationFunc,entry.operationDesc};
        for(int c=0;c<values.size();++c)table->setItem(i,c,new QTableWidgetItem(values[c]));
        table->item(i,4)->setToolTip("双击日志行查看完整详情，也可选中后点击“查看详情”");
    }
    status->setText(QString("第 %1 页，%2 条").arg(page).arg(rows.size()));
}

// 只有启用状态、操作日志标签且有效行被选中时才允许查看详情。
void OperationLogPage::updateDetailsAction()
{
    if(!detailsButton)return;
    const auto item=table->item(table->currentRow(),0);
    detailsButton->setEnabled(isEnabled() && tabs->currentIndex()==0 && item && item->isSelected());
}

// 用只读纯文本展示完整字段，复制回调按值保存文本，不依赖稍后变化的表格。
void OperationLogPage::showSelectedDetails()
{
    updateDetailsAction();
    if(!detailsButton->isEnabled())return;
    if(detailsDialog && detailsDialog->isVisible()) {
        detailsDialog->raise();detailsDialog->activateWindow();return;
    }
    const int row=table->currentRow();
    const QString description=table->item(row,4)->text();
    auto dialog=new QDialog(this);
    detailsDialog=dialog;
    dialog->setObjectName("operationLogDetailsDialog");
    dialog->setWindowTitle("操作日志详情");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(760,500);
    dialog->setMinimumSize(520,360);
    auto layout=new QVBoxLayout(dialog);
    layout->setContentsMargins(20,20,20,20);
    layout->setSpacing(12);
    auto metadata=new QFormLayout;
    const QStringList names={"operationLogId","operationLogTime","operationLogAdmin","operationLogFunction"};
    for(int column=0;column<4;++column) {
        auto value=new QLabel(table->item(row,column)->text());
        value->setObjectName(names[column]);
        value->setTextFormat(Qt::PlainText);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
        value->setWordWrap(true);
        metadata->addRow(table->horizontalHeaderItem(column)->text()+"：",value);
    }
    layout->addLayout(metadata);
    layout->addWidget(new QLabel("完整详情："));
    auto text=new QPlainTextEdit;
    text->setObjectName("operationLogDetailsText");
    text->setReadOnly(true);
    text->setPlainText(description);
    text->setPlaceholderText("这条日志没有附加详情");
    text->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout->addWidget(text,1);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->button(QDialogButtonBox::Close)->setText("关闭");
    auto copy=buttons->addButton("复制详情",QDialogButtonBox::ActionRole);
    copy->setObjectName("copyLogDetailsButton");
    copy->setEnabled(!description.isEmpty());
    connect(copy,&QPushButton::clicked,dialog,[description]{QApplication::clipboard()->setText(description);});
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    layout->addWidget(buttons);
    dialog->open();
}

// 页面被禁用时同步关闭详情窗并更新按钮，防止退出后继续查看旧窗口。
void OperationLogPage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if(event->type()==QEvent::EnabledChange) {
        if(!isEnabled() && detailsDialog)detailsDialog->close();
        updateDetailsAction();
    }
}

// 页面隐藏时关闭详情对话框，再调用 QWidget 基类处理。
void OperationLogPage::hideEvent(QHideEvent *event)
{
    if(detailsDialog)detailsDialog->close();
    QWidget::hideEvent(event);
}
