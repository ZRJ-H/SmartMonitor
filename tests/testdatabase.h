#ifndef TESTDATABASE_H
#define TESTDATABASE_H
#include "../util/dbconn.h"
#include <QSqlQuery>
#include <QSqlError>
// 全部窗口和通道测试使用内存库，不连接或恢复用户的真实数据库。
inline bool initializeTestDatabase() {
    qputenv("SMART_MONITOR_DB_DRIVER","QSQLITE");
    qputenv("SMART_MONITOR_DB_NAME",":memory:");
    qputenv("SMART_MONITOR_AUTOSTART","0");
    auto db = DbConn::getInstance()->database();
    if (!db.isOpen() || db.driverName() != "QSQLITE") return false;
    const QStringList sql = {
        "CREATE TABLE IF NOT EXISTS sys_config(config_key TEXT PRIMARY KEY,config_value TEXT)",
        "CREATE TABLE IF NOT EXISTS video(id INTEGER PRIMARY KEY,video_name TEXT,video_path TEXT,channel_id INTEGER,start_time TEXT,end_time TEXT,file_size INTEGER)",
        "CREATE TABLE IF NOT EXISTS monitor_event(id INTEGER PRIMARY KEY,event_key TEXT UNIQUE,channel_id INTEGER,event_time TEXT,video_path TEXT,confidence REAL)",
        "CREATE TABLE IF NOT EXISTS operation_log(id INTEGER PRIMARY KEY,admin_id INTEGER,operation_time TEXT,operation_func TEXT,operation_desc TEXT)",
        "CREATE TABLE IF NOT EXISTS feature_image(id INTEGER PRIMARY KEY,image_name TEXT,image_path TEXT UNIQUE,channel_id INTEGER,capture_time TEXT,capture_source TEXT,exception_id INTEGER,video_id INTEGER,video_offset_ms INTEGER)"
    };
    for (const auto &statement : sql) { QSqlQuery q(db); if (!q.exec(statement)) { qWarning()<<q.lastError();return false; } }
    return true;
}
#endif
