// 【阅读导引】
// 图片管理界面：维护筛选、九宫格、选中状态和大图查看器；文件/SQL 操作委托 ImageService。
// 关联回放仅发出 videoId 与偏移，播放器由主窗口持有。

#ifndef IMAGEPAGE_H
#define IMAGEPAGE_H

#include "../model/featureimage.h"

#include <QSqlDatabase>
#include <QWidget>
#include <memory>

class ImageService;
class QComboBox;
class QDateTimeEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class QListWidget;

// 图片管理页只负责筛选、展示和触发用户操作，文件与数据库一致性由 ImageService 保证。
class ImagePage : public QWidget
{
    Q_OBJECT

public:
    ImagePage(const QSqlDatabase &database, const QString &imageDirectory,
              QWidget *parent = nullptr);
    ~ImagePage() override;

public slots:
    void reload();
    void showEventImages(qint64 exceptionId);
    void requestSelectedPlayback();
    void setImageDirectory(const QString &directory);

signals:
    void playbackRequested(qint64 videoId, qint64 offsetMs);
    void imagesExported(int count);
    void imagesDeleted(int count);

private slots:
    void openViewer();
    void previousPage();
    void nextPage();
    void updatePreview(int currentRow);
    void exportSelected();
    void deleteSelected();

private:
    void initUi();
    QList<qint64> selectedIds() const;
    int imageIndexForRow(int row) const;
    void clearSelectionState();

    QSqlDatabase db;
    QString imageDirectory;
    std::unique_ptr<ImageService> imageService;
    QList<FeatureImage> currentImages;
    int currentPage;
    qint64 exceptionFilterId = 0;

    QDateTimeEdit *fromEdit;
    QDateTimeEdit *toEdit;
    QComboBox *channelCombo;
    QComboBox *sourceCombo;
    QTableWidget *table;
    QListWidget *thumbnails = nullptr;
    QLabel *previewLabel;
    QLabel *pageLabel;
    QPushButton *previousButton;
    QPushButton *nextButton;
    QPushButton *exportButton;
    QPushButton *deleteButton;
    QPushButton *playbackButton;
};

#endif // IMAGEPAGE_H
