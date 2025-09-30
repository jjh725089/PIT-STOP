// #ifndef MAINWINDOW_H
// #define MAINWINDOW_H

// #include <QMainWindow>
// #include <QLineEdit>
// #include <QPushButton>
// #include <QFormLayout>
// #include <QVBoxLayout>
// #include <QFrame>
// #include "databasemanager.h"
// #include "mqtt.h"

// class MainWindow : public QMainWindow
// {
//     Q_OBJECT

// public:
//     explicit MainWindow(QWidget *parent = nullptr);
//     ~MainWindow() override = default;

// private slots:
//     void onLoginClicked();
//     void onSignupClicked();

// private:
//     // 로그인 성공 후 좌표를 발행하는 별도 함수
//     void publishCameraCoordinates(const QString &userId);

//     DatabaseManager   m_dbManager;
//     Mqtt *m_mqttClient;

//     // 로그인 폼 위젯
//     QLineEdit        *idLineEdit;
//     QLineEdit        *passwordLineEdit;
//     QPushButton      *loginButton;
//     QPushButton      *signupButton;
//     QFrame           *loginFrame;
// };

// #endif // MAINWINDOW_H
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QTabWidget>
#include "databasemanager.h"
#include "mqtt.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onLoginClicked();
    void onSignupClicked();
    void onDeleteAccountClicked();

private:
    // 로그인 성공 후 좌표를 발행하는 별도 함수
    void publishCameraCoordinates(const QString &userId);

    // UI 생성 함수들
    QWidget* createLoginTab();
    QWidget* createSignupTab();
    QWidget* createDeleteAccountTab();

    DatabaseManager   m_dbManager;
    Mqtt *m_mqttClient;

    // 탭 위젯
    QTabWidget       *tabWidget;

    // 로그인 탭 위젯들
    QLineEdit        *loginIdLineEdit;
    QLineEdit        *loginPasswordLineEdit;
    QPushButton      *loginButton;

    // 회원가입 탭 위젯들
    QLineEdit        *signupIdLineEdit;
    QLineEdit        *signupPasswordLineEdit;
    QLineEdit        *signupPasswordConfirmLineEdit;
    QLineEdit        *camera1XLineEdit;
    QLineEdit        *camera1YLineEdit;
    QLineEdit        *camera2XLineEdit;
    QLineEdit        *camera2YLineEdit;
    QLineEdit        *camera3XLineEdit;
    QLineEdit        *camera3YLineEdit;
    QPushButton      *signupButton;

    // 회원탈퇴 탭 위젯들
    QLineEdit        *deleteIdLineEdit;
    QLineEdit        *deletePasswordLineEdit;
    QPushButton      *deleteAccountButton;
};

#endif // MAINWINDOW_H
