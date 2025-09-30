#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVariant>

DatabaseManager::DatabaseManager(const QString& path)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        qFatal("Cannot open database: %s", qPrintable(m_db.lastError().text()));
    }
    initialize();
}

DatabaseManager::~DatabaseManager()
{
    m_db.close();
}

void DatabaseManager::initialize()
{
    QSqlQuery query;
    // users 테이블이 없으면 생성
    const QString createTable =
        "CREATE TABLE IF NOT EXISTS users ("
        " id   TEXT PRIMARY KEY,"
        " pass TEXT NOT NULL,"
        " cam1x REAL NOT NULL,"
        " cam1y REAL NOT NULL,"
        " cam2x REAL NOT NULL,"
        " cam2y REAL NOT NULL,"
        " cam3x REAL NOT NULL,"
        " cam3y REAL NOT NULL"
        ");";
    if (!query.exec(createTable)) {
        qFatal("Failed to create table: %s", qPrintable(query.lastError().text()));
    }

    // 로그 테이블 생성
    const QString createLogTable =
        "CREATE TABLE IF NOT EXISTS logs ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " user_id TEXT NOT NULL,"
        " timestamp TEXT,"
        " source TEXT,"
        " event TEXT,"
        " status TEXT,"
        " FOREIGN KEY(user_id) REFERENCES users(id)"
        ");";

    if (!query.exec(createLogTable)) {
        qFatal("Failed to create logs table: %s", qPrintable(query.lastError().text()));
    }

    const QString createEventTable = R"(
    CREATE TABLE IF NOT EXISTS fall_events (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id TEXT NOT NULL,
        event_time TEXT UNIQUE,
        image_path TEXT,
        json_data TEXT,
        FOREIGN KEY(user_id) REFERENCES users(id)
    );)";
    query.exec(createEventTable);
}

bool DatabaseManager::registerUser(const QString& id, const QString& password, double cam1x, double cam1y,
                                   double cam2x, double cam2y,
                                   double cam3x, double cam3y)
{
    // 이미 존재하는 ID인지 체크
    QSqlQuery check;
    check.prepare("SELECT id FROM users WHERE id = :id");
    check.bindValue(":id", id);
    if (!check.exec()) return false;
    if (check.next()) {
        // 이미 존재
        return false;
    }
    // 삽입
    QSqlQuery insert;
    // insert.prepare("INSERT INTO users (id, pass) VALUES (:id, :pass)");
    // insert.bindValue(":id", id);
    // insert.bindValue(":pass", password);
    insert.prepare("INSERT INTO users (id, pass, cam1x, cam1y, cam2x, cam2y, cam3x, cam3y)"
                   "VALUES (:id, :pass, :cam1x, :cam1y, :cam2x, :cam2y, :cam3x, :cam3y)");
    insert.bindValue(":id", id);
    insert.bindValue(":pass", password);
    insert.bindValue(":cam1x", cam1x);
    insert.bindValue(":cam1y", cam1y);
    insert.bindValue(":cam2x", cam2x);
    insert.bindValue(":cam2y", cam2y);
    insert.bindValue(":cam3x", cam3x);
    insert.bindValue(":cam3y", cam3y);
    return insert.exec();
}

bool DatabaseManager::loginUser(const QString& id, const QString& password)
{
    QSqlQuery query;
    query.prepare("SELECT pass FROM users WHERE id = :id");
    query.bindValue(":id", id);
    if (!query.exec() || !query.next()) {
        // ID가 없음
        return false;
    }
    const QString stored = query.value(0).toString();
    return (stored == password);
}

bool DatabaseManager::getCameraCoordinates(const QString &id, CameraCoordinates &coords)
{
    QSqlQuery query;
    query.prepare("SELECT cam1x, cam1y, cam2x, cam2y, cam3x, cam3y FROM users WHERE id = :id");
    query.bindValue(":id", id);
    if (!query.exec() || !query.next()) {
        return false;
    }
    coords.cam1x = query.value(0).toDouble();
    coords.cam1y = query.value(1).toDouble();
    coords.cam2x = query.value(2).toDouble();
    coords.cam2y = query.value(3).toDouble();
    coords.cam3x = query.value(4).toDouble();
    coords.cam3y = query.value(5).toDouble();
    qDebug() << "[getCameraCoordinates] Loaded cam1x:" << coords.cam1x;
    return true;
}

bool DatabaseManager::deleteUser(const QString& id) {
    QSqlQuery query;
    query.prepare("DELETE FROM users WHERE id = :id");
    query.bindValue(":id", id);
    return query.exec();
}
