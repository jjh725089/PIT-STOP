#ifndef SIGNUPWINDOW_H
#define SIGNUPWINDOW_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class SignupWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SignupWindow(QWidget *parent = nullptr);
    ~SignupWindow() override = default;

    QString userId() const   { return m_id; }
    QString userPassword() const { return m_password; }

    double userCamera1X() const { return m_camera1X; }
    double userCamera1Y() const { return m_camera1Y; }

    double userCamera2X() const { return m_camera2X; }
    double userCamera2Y() const { return m_camera2Y; }

    double userCamera3X() const { return m_camera3X; }
    double userCamera3Y() const { return m_camera3Y; }

private slots:
    void onRegisterClicked();
    void onCancelClicked();

private:
    // 입력 위젯
    QLineEdit   *idLineEdit;
    QLineEdit   *pwLineEdit;
    QLineEdit   *pwConfirmLineEdit;
    // 버튼
    QPushButton *registerButton;
    QPushButton *cancelButton;
    // 최종 저장할 값
    QString      m_id;
    QString      m_password;
    // 카메라 3대 좌표용 멤버
    QLineEdit *camera1XLineEdit;
    QLineEdit *camera1YLineEdit;
    QLineEdit *camera2XLineEdit;
    QLineEdit *camera2YLineEdit;
    QLineEdit *camera3XLineEdit;
    QLineEdit *camera3YLineEdit;
    // 좌표
    double m_camera1X, m_camera1Y;
    double m_camera2X, m_camera2Y;
    double m_camera3X, m_camera3Y;
};

#endif // SIGNUPWINDOW_H
