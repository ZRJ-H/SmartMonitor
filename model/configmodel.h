#ifndef CONFIGMODEL_H
#define CONFIGMODEL_H

#include <QString>

// sys_config 表的数据访问层，只负责按键读取和保存字符串配置。
class ConfigModel
{
public:
    bool findValue(const QString &key,
                   QString &value,
                   bool &found);
    bool upsertValue(const QString &key, const QString &value);
    QString lastErrorText() const;

private:
    QString last_error_text_;
};

#endif // CONFIGMODEL_H
