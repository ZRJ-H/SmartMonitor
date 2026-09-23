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
    events=new QTableWidget(0,5);events->setHorizontalHeaderLabels({"事件","时间","通道","状态","录像"});
    events->setSelectionBehavior(QAbstractItemView::SelectRows);events->setEditTriggers(QAbstractItemView::NoEditTriggers);
    events->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tabs->addTab(table,"操作日志");tabs->addTab(events,"异常事件");root->addWidget(tabs);
    status=new QLabel;status->setWordWrap(true);root->addWidget(status);
    auto actions=new QHBoxLayout;auto previous=new QPushButton("上一页");auto next=new QPushButton("下一页");
    auto remove=new QPushButton("删除时间范围内的日志");
    auto removeSelected=new QPushButton("删除所选日志");auto images=new QPushButton("关联图片");auto play=new QPushButton("关联录像");
    actions->addWidget(removeSelected);actions->addWidget(images);actions->addWidget(play);
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
void OperationLogPage::reload() {
    if(tabs->currentIndex()==1){
        QString error;
        if(!service.queryEvents(channel->currentData().toInt(),from->dateTime(),to->dateTime(),page,50,currentEvents,error)){
            status->setText(error);return;
        }
        events->setRowCount(currentEvents.size());
        for(int i=0;i<currentEvents.size();++i){
            const auto &e=currentEvents[i];const QStringList values={QString::number(e.exceptionId),e.eventTime.toString("yyyy-MM-dd HH:mm:ss"),QString::number(e.channelId),e.status,QString::number(e.videoId)};
            for(int c=0;c<5;++c)events->setItem(i,c,new QTableWidgetItem(values[c]));
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
    }
    status->setText(QString("第 %1 页，%2 条").arg(page).arg(rows.size()));
}
