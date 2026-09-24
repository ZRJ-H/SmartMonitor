// 【阅读导引】
// 录像元数据值对象：保存路径、通道、时间和大小，不持有视频帧或编码器。
// 可通过信号跨线程传递；构造出的对象不代表已经成功写入数据库。

#include "video.h"

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
Video::Video(const QString &videoName,
             const QString &videoPath,
             int channelId,
             const QDateTime &startTime,
             const QDateTime &endTime,
             qint64 fileSize,
             int id)
    : id_(id),
      video_name_(videoName),
      video_path_(videoPath),
      channel_id_(channelId),
      start_time_(startTime),
      end_time_(endTime),
      file_size_(fileSize)
{
}

// 返回数据库录像 ID；未入库时可能为零。
int Video::getId() const { return id_; }
// 返回文件名，不包括目录。
QString Video::getVideoName() const { return video_name_; }
// 返回录像完整路径，不在此打开文件。
QString Video::getVideoPath() const { return video_path_; }
// 返回业务通道编号，不是数组下标。
int Video::getChannelId() const { return channel_id_; }
// 返回本段开始的业务时间。
QDateTime Video::getStartTime() const { return start_time_; }
// 返回本段结束时间。
QDateTime Video::getEndTime() const { return end_time_; }
// 返回文件大小，单位字节。
qint64 Video::getFileSize() const { return file_size_; }
