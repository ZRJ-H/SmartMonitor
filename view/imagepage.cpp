// 【阅读导引】
// 图片管理界面：维护筛选、九宫格、选中状态和大图查看器；文件/SQL 操作委托 ImageService。
// 关联回放仅发出 videoId 与偏移，播放器由主窗口持有。

#include "imagepage.h"
#include "../service/imageservice.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QImageReader>
#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QSignalBlocker>


namespace {
constexpr int kPageSize = 9;
}

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
ImagePage::ImagePage(const QSqlDatabase &database,
                     const QString &imageDirectory,
                     QWidget *parent)
    : QWidget(parent),
      db(database),
      imageDirectory(imageDirectory),
      imageService(new ImageService(database, imageDirectory)),
      currentPage(1),
      fromEdit(nullptr),
      toEdit(nullptr),
      channelCombo(nullptr),
      sourceCombo(nullptr),
      table(nullptr),
      previewLabel(nullptr),
      pageLabel(nullptr),
      previousButton(nullptr),
      nextButton(nullptr),
      exportButton(nullptr),
      deleteButton(nullptr),
      playbackButton(nullptr)
{
    setObjectName("imagePage");
    initUi();
}

// 对象销毁时释放本类负责的资源；依赖的外部对象需按约定保持有效。
ImagePage::~ImagePage() = default;

// 创建筛选、九宫格、隐藏元数据表和操作按钮，并建立局部交互连接。
void ImagePage::initUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    QHBoxLayout *filters = new QHBoxLayout;

    fromEdit = new QDateTimeEdit(QDateTime(QDate(2000, 1, 1), QTime(0, 0)));
    toEdit = new QDateTimeEdit(QDateTime(QDate(2100, 1, 1), QTime(0, 0)));
    fromEdit->setObjectName("imageFromEdit");
    toEdit->setObjectName("imageToEdit");
    fromEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    toEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");

    channelCombo = new QComboBox;
    channelCombo->setObjectName("imageChannelCombo");
    channelCombo->addItem(QString::fromUtf8("全部通道"), 0);
    for (int channel = 1; channel <= 4; ++channel) {
        channelCombo->addItem(QString::fromUtf8("通道%1").arg(channel), channel);
    }

    sourceCombo = new QComboBox;
    sourceCombo->setObjectName("imageSourceCombo");
    sourceCombo->addItem(QString::fromUtf8("全部来源"), QString());
    sourceCombo->addItem(QString::fromUtf8("手动截图"), QStringLiteral("manual"));
    sourceCombo->addItem(QString::fromUtf8("异常事件"), QStringLiteral("event"));

    QPushButton *queryButton = new QPushButton(QString::fromUtf8("查询"));
    queryButton->setObjectName("queryImageButton");
    filters->addWidget(new QLabel(QString::fromUtf8("从")));
    filters->addWidget(fromEdit);
    filters->addWidget(new QLabel(QString::fromUtf8("到")));
    filters->addWidget(toEdit);
    filters->addWidget(channelCombo);
    filters->addWidget(sourceCombo);
    filters->addWidget(queryButton);
    root->addLayout(filters);

    QHBoxLayout *content = new QHBoxLayout;
    table = new QTableWidget(0, 5, this);
    table->setObjectName("imageTable");
    table->setHorizontalHeaderLabels({QString::fromUtf8("名称"),
                                      QString::fromUtf8("通道"),
                                      QString::fromUtf8("时间"),
                                      QString::fromUtf8("来源"),
                                      QString::fromUtf8("录像编号")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    // 隐藏的记录表保留元数据映射；用户通过九宫格选择图片。
    table->hide();
    thumbnails=new QListWidget;
    thumbnails->setObjectName("imageThumbnails");
    thumbnails->setViewMode(QListView::IconMode);
    thumbnails->setResizeMode(QListView::Adjust);
    thumbnails->setMovement(QListView::Static);
    thumbnails->setSelectionMode(QAbstractItemView::ExtendedSelection);
    thumbnails->setIconSize(QSize(160,100));
    thumbnails->setGridSize(QSize(180,140));
    thumbnails->setMinimumWidth(0);
    thumbnails->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    content->addWidget(thumbnails,3);
    // 以可见缩略图的选择为准，取消最后一项时同时清除隐藏记录索引。
    const auto syncThumbnailSelection = [this]{
        const auto selected=thumbnails->selectedItems();
        if(selected.isEmpty()) {
            const QSignalBlocker blocker(table);
            table->clearSelection();
            table->setCurrentCell(-1,-1);
            clearSelectionState();
            return;
        }
        auto current=thumbnails->currentItem();
        if(!current || !current->isSelected()) current=selected.first();
        const int row=thumbnails->row(current);
        {
            const QSignalBlocker blocker(table);
            table->setCurrentCell(row,0);
        }
        updatePreview(row);
    };
    connect(thumbnails,&QListWidget::itemSelectionChanged,this,syncThumbnailSelection);
    connect(thumbnails,&QListWidget::currentItemChanged,this,syncThumbnailSelection);
    connect(thumbnails,&QListWidget::itemDoubleClicked,this,[this]{openViewer();});

    QHBoxLayout *previewLayout = new QHBoxLayout;
    previewLabel = new QLabel(QString::fromUtf8("请选择图片"));
    previewLabel->setObjectName("imageSelectionStatus");
    previewLabel->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    previewLayout->addWidget(previewLabel, 1);

    playbackButton = new QPushButton(QString::fromUtf8("跳转录像"));
    playbackButton->setObjectName("playbackImageButton");
    exportButton = new QPushButton(QString::fromUtf8("导出"));
    exportButton->setObjectName("exportImageButton");
    deleteButton = new QPushButton(QString::fromUtf8("删除"));
    deleteButton->setObjectName("deleteImageButton");
    previewLayout->addWidget(playbackButton);
    previewLayout->addWidget(exportButton);
    previewLayout->addWidget(deleteButton);
    root->addLayout(content, 1);
    root->addLayout(previewLayout);

    QHBoxLayout *paging = new QHBoxLayout;
    previousButton = new QPushButton(QString::fromUtf8("上一页"));
    nextButton = new QPushButton(QString::fromUtf8("下一页"));
    previousButton->setObjectName("imagePreviousButton");
    nextButton->setObjectName("imageNextButton");
    pageLabel = new QLabel(QString::fromUtf8("第 1 页"));
    pageLabel->setObjectName("imagePageLabel");
    paging->addStretch();
    paging->addWidget(previousButton);
    paging->addWidget(pageLabel);
    paging->addWidget(nextButton);
    paging->addStretch();
    root->addLayout(paging);

    connect(queryButton, &QPushButton::clicked, this, [this]() {
        currentPage = 1;
        exceptionFilterId = 0;
        reload();
    });
    connect(previousButton, &QPushButton::clicked, this, &ImagePage::previousPage);
    connect(nextButton, &QPushButton::clicked, this, &ImagePage::nextPage);
    connect(table, &QTableWidget::currentCellChanged, this,
            [this](int currentRow, int, int, int) { updatePreview(currentRow); });
    connect(playbackButton, &QPushButton::clicked,
            this, &ImagePage::requestSelectedPlayback);
    connect(exportButton, &QPushButton::clicked, this, &ImagePage::exportSelected);
    connect(deleteButton, &QPushButton::clicked, this, &ImagePage::deleteSelected);
    clearSelectionState();
}

// 按筛选查一页并重新建立缩略图/元数据索引；空尾页回退后重查。
void ImagePage::reload()
{
    ImageFilter filter;
    filter.exceptionId=exceptionFilterId;
    filter.from = fromEdit->dateTime();
    filter.to = toEdit->dateTime();
    filter.channelId = channelCombo->currentData().toInt();
    filter.captureSource = sourceCombo->currentData().toString();

    QList<FeatureImage> images;
    QString error;
    if (!imageService->query(filter, currentPage, kPageSize, images, error)) {
        pageLabel->setText(error);
        return;
    }

    if(images.isEmpty() && currentPage>1){--currentPage;reload();return;}
    {
        const QSignalBlocker blocker(table);
        table->clearSelection();
        table->setCurrentCell(-1,-1);
    }
    currentImages = images;
    thumbnails->clear();
    table->setRowCount(currentImages.size());
    for (int row = 0; row < currentImages.size(); ++row) {
        const FeatureImage &image = currentImages.at(row);
        QImageReader reader(image.imagePath);
        reader.setScaledSize(QSize(160,100));
        auto item=new QListWidgetItem(QIcon(QPixmap::fromImage(reader.read())),
            image.imageName+"\n"+image.captureTime.toString("MM-dd HH:mm:ss"),thumbnails);
        item->setData(Qt::UserRole,row);
        QTableWidgetItem *nameItem = new QTableWidgetItem(image.imageName);
        // 表格排序和筛选后仍通过索引回到本页数据，避免依赖可变的显示文本。
        nameItem->setData(Qt::UserRole, row);
        table->setItem(row, 0, nameItem);
        table->setItem(row, 1, new QTableWidgetItem(QString::number(image.channelId)));
        table->setItem(row, 2, new QTableWidgetItem(
                           image.captureTime.toString("yyyy-MM-dd HH:mm:ss")));
        table->setItem(row, 3, new QTableWidgetItem(image.captureSource));
        table->setItem(row, 4, new QTableWidgetItem(
                           image.videoId > 0 ? QString::number(image.videoId) : QString()));
    }

    pageLabel->setText(QString::fromUtf8("第 %1 页").arg(currentPage));
    previousButton->setEnabled(currentPage > 1);
    nextButton->setEnabled(currentImages.size() == kPageSize);
    clearSelectionState();
}

// 仅在非首页时减少页码后重新查询。
void ImagePage::previousPage()
{
    if (currentPage <= 1) return;
    --currentPage;
    reload();
}

// 请求下一页，空结果由 reload 的页码修正处理。
void ImagePage::nextPage()
{
    if (currentImages.size() < kPageSize) return;
    ++currentPage;
    reload();
}

// 从行元数据取当前图片数组下标，不能把行号当作数据库 ID。
int ImagePage::imageIndexForRow(int row) const
{
    if (row < 0 || row >= table->rowCount() || table->item(row, 0) == nullptr) return -1;
    return table->item(row, 0)->data(Qt::UserRole).toInt();
}

// 选择时只更新状态和操作，不加载原图；双击后在弹窗中读取原图。
void ImagePage::updatePreview(int currentRow)
{
    const int index = imageIndexForRow(currentRow);
    if (index < 0 || index >= currentImages.size()) {
        clearSelectionState();
        return;
    }

    const FeatureImage image = currentImages.at(index);
    if (!QFileInfo::exists(image.imagePath)) {
        // 文件缺失不代表数据库记录不可管理，仍允许管理员删除失效记录。
        previewLabel->setPixmap(QPixmap());
        previewLabel->setText(QString::fromUtf8("图片文件不存在"));
    } else {
        previewLabel->setText(QString("已选择：%1（双击缩略图查看大图）").arg(image.imageName));
    }
    exportButton->setEnabled(true);
    deleteButton->setEnabled(true);
    playbackButton->setEnabled(image.videoId > 0);
}

// 清空预览并禁用依赖选中的按钮，避免操作上一次查询的图片。
void ImagePage::clearSelectionState()
{
    previewLabel->setPixmap(QPixmap());
    previewLabel->setText(QString::fromUtf8("请选择图片"));
    exportButton->setEnabled(false);
    deleteButton->setEnabled(false);
    playbackButton->setEnabled(false);
}

// 把选中的缩略图/元数据行转换成数据库 ID 列表。
QList<qint64> ImagePage::selectedIds() const
{
    QList<qint64> ids;
    QSet<int> rows;
    for(auto item:thumbnails->selectedItems()){
        const int index=item->data(Qt::UserRole).toInt();
        if(index>=0&&index<currentImages.size())ids<<currentImages.at(index).id;
    }
    if(!ids.isEmpty())return ids;
    for (const QModelIndex &index : table->selectionModel()->selectedRows()) {
        rows.insert(index.row());
    }
    for (int row : rows) {
        const int imageIndex = imageIndexForRow(row);
        if (imageIndex >= 0 && imageIndex < currentImages.size()) {
            ids.append(currentImages.at(imageIndex).id);
        }
    }
    return ids;
}

// 检查当前图的关联录像 ID，再发出定位请求；不直接创建播放器。
void ImagePage::requestSelectedPlayback()
{
    const int index = imageIndexForRow(table->currentRow());
    if (index < 0 || index >= currentImages.size()) return;
    const FeatureImage &image = currentImages.at(index);
    if (image.videoId > 0) emit playbackRequested(image.videoId, image.videoOffsetMs);
}

// 选择目标目录后调用服务复制图片，成功发出导出审计所需数量。
void ImagePage::exportSelected()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty()) return;
    const QString target = QFileDialog::getExistingDirectory(
                this, QString::fromUtf8("选择导出目录"));
    if (target.isEmpty()) return;

    QString error;
    if (!imageService->exportImages(ids, target, error)) {
        QMessageBox::warning(this, QString::fromUtf8("导出失败"), error);
        return;
    }
    emit imagesExported(ids.size());
}

// 先二次确认再调用删除服务，成功后重查当前页。
void ImagePage::deleteSelected()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty()) return;
    if (QMessageBox::question(this, QString::fromUtf8("确认删除"),
                              QString::fromUtf8("确定删除选中的 %1 张图片吗？").arg(ids.size()))
            != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!imageService->deleteImages(ids, true, error)) {
        QMessageBox::warning(this, QString::fromUtf8("删除失败"), error);
        return;
    }
    emit imagesDeleted(ids.size());
    reload();
}

// 替换保存目录并重建服务对象，unique_ptr 自动释放旧服务。
void ImagePage::setImageDirectory(const QString &directory)
{
    imageDirectory = directory;
    imageService.reset(new ImageService(db, imageDirectory));
    currentPage = 1;
    reload();
}

// 创建随关闭自动释放的查看器；共享索引让多个按钮操作同一当前图片。
void ImagePage::openViewer()
{
    int row=table->currentRow();
    if(row<0||row>=currentImages.size())return;
    auto dialog=new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->resize(1000,720);
    auto root=new QVBoxLayout(dialog);auto view=new QGraphicsView(dialog);
    view->setObjectName("largeImageView");view->setDragMode(QGraphicsView::ScrollHandDrag);
    auto scene=new QGraphicsScene(view);view->setScene(scene);root->addWidget(view);
    auto buttons=new QHBoxLayout;
    auto previous=new QPushButton("上一张");auto next=new QPushButton("下一张");
    auto zoomIn=new QPushButton("放大");auto zoomOut=new QPushButton("缩小");auto fit=new QPushButton("适合窗口");
    zoomIn->setObjectName("zoomImageInButton");
    for(auto b:{previous,next,zoomIn,zoomOut,fit}) buttons->addWidget(b);
    root->addLayout(buttons);
    const auto images=currentImages;
    auto index=std::make_shared<int>(row);
    auto show=[dialog,view,scene,images,index]{
        const auto &image=images.at(*index);dialog->setWindowTitle(image.imageName);scene->clear();view->resetTransform();
        QPixmap pixmap(image.imagePath);
        if(pixmap.isNull()){scene->addText("图片文件不存在");return;}
        scene->addPixmap(pixmap);scene->setSceneRect(pixmap.rect());view->fitInView(scene->sceneRect(),Qt::KeepAspectRatio);
    };
    connect(previous,&QPushButton::clicked,dialog,[index,show]{if(*index>0){--*index;show();}});
    connect(next,&QPushButton::clicked,dialog,[images,index,show]{if(*index+1<images.size()){++*index;show();}});
    connect(zoomIn,&QPushButton::clicked,dialog,[view]{if(view->transform().m11()<20)view->scale(1.25,1.25);});
    connect(zoomOut,&QPushButton::clicked,dialog,[view]{if(view->transform().m11()>.02)view->scale(.8,.8);});
    connect(fit,&QPushButton::clicked,dialog,[view,scene]{view->fitInView(scene->sceneRect(),Qt::KeepAspectRatio);});
    dialog->show();show();
}

// 清除其他筛选限制，固定事件 ID 后查询，避免旧时间范围挡住关联图片。
void ImagePage::showEventImages(qint64 id) { exceptionFilterId=id;fromEdit->setDateTime(QDateTime(QDate(2000,1,1),QTime(0,0)));toEdit->setDateTime(QDateTime(QDate(2100,1,1),QTime(0,0)));currentPage=1;channelCombo->setCurrentIndex(0);sourceCombo->setCurrentIndex(0);reload(); }
