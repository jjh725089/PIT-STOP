#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QSqlDatabase>

class DatabaseManager
{
public:
    DatabaseManager(const QString& path = "users.db");
    ~DatabaseManager();

    bool registerUser(const QString& id, const QString& password, double cam1x, double cam1y,
                                       double cam2x, double cam2y,
                                       double cam3x, double cam3y);
    bool loginUser(const QString& id, const QString& password);
    bool deleteUser(const QString& id);

    struct CameraCoordinates {
        double cam1x, cam1y;
        double cam2x, cam2y;
        double cam3x, cam3y;
    };

    bool getCameraCoordinates(const QString &id, CameraCoordinates &coords);

private:
    QSqlDatabase m_db;
    void initialize();
};

#endif // DATABASEMANAGER_H
