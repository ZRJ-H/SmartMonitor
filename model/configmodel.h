// 【阅读导引】
// 单键配置访问：findValue 区分不存在和查询失败；upsertValue 兼容 MySQL 与 SQLite。

// 声明 sys_config 表的数据访问对象，为业务层提供按键读取和写入配置的接口。
#ifndef CONFIGMODEL_H
#define CONFIGMODEL_H

#include <QString>

// sys_config 表的数据访问层，只负责按键读取和保存字符串配置。
class ConfigModel
{
public:
    // 按 key 查询配置；返回值表示查询是否成功，成功后再由 found 区分记录存在与否。
    bool findValue(const QString &key,
                   QString &value,
                   bool &found);
    // 新增或更新 key 对应的字符串值；成功返回 true。
    bool upsertValue(const QString &key, const QString &value);
    // 返回最近一次数据库操作失败时的错误文本。
    QString lastErrorText() const;

private:
    QString last_error_text_; // 最近一次失败的数据库错误，成功操作会将其清空。
};

#endif // CONFIGMODEL_H
