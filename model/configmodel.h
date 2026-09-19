#ifndef CONFIGMODEL_H
#define CONFIGMODEL_H

#include <QString>

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
