// #include "signupwindow.h"
// #include <QFormLayout>
// #include <QHBoxLayout>
// #include <QVBoxLayout>
// #include <QMessageBox>

// SignupWindow::SignupWindow(QWidget *parent)
//     : QDialog(parent)
//     , idLineEdit(new QLineEdit(this))
//     , pwLineEdit(new QLineEdit(this))
//     , pwConfirmLineEdit(new QLineEdit(this))
//     , camera1XLineEdit(new QLineEdit(this))
//     , camera1YLineEdit(new QLineEdit(this))
//     , camera2XLineEdit(new QLineEdit(this))
//     , camera2YLineEdit(new QLineEdit(this))
//     , camera3XLineEdit(new QLineEdit(this))
//     , camera3YLineEdit(new QLineEdit(this))
//     , registerButton(new QPushButton(tr("회원가입"), this))
//     , cancelButton(new QPushButton(tr("취소"), this))
// {
//     setWindowTitle(tr("회원가입"));
//     setModal(true);

//     // 패스워드 입력 마스킹
//     pwLineEdit->setEchoMode(QLineEdit::Password);
//     pwConfirmLineEdit->setEchoMode(QLineEdit::Password);

//     // placeholder
//     idLineEdit->setPlaceholderText(tr("아이디 입력"));
//     pwLineEdit->setPlaceholderText(tr("비밀번호 입력"));
//     pwConfirmLineEdit->setPlaceholderText(tr("비밀번호 확인"));

//     // 카메라 좌표 입력 placeholder 지정 (선택)
//     camera1XLineEdit->setPlaceholderText(tr("카메라1 X"));
//     camera1YLineEdit->setPlaceholderText(tr("카메라1 Y"));
//     camera2XLineEdit->setPlaceholderText(tr("카메라2 X"));
//     camera2YLineEdit->setPlaceholderText(tr("카메라2 Y"));
//     camera3XLineEdit->setPlaceholderText(tr("카메라3 X"));
//     camera3YLineEdit->setPlaceholderText(tr("카메라3 Y"));

//     // form 레이아웃
//     auto *formLayout = new QFormLayout;
//     formLayout->addRow(tr("아이디:"), idLineEdit);
//     formLayout->addRow(tr("비밀번호:"), pwLineEdit);
//     formLayout->addRow(tr("비밀번호 확인:"), pwConfirmLineEdit);

//     // 카메라1~3 좌표 입력 추가
//     formLayout->addRow(tr("카메라1 X:"), camera1XLineEdit);
//     formLayout->addRow(tr("카메라1 Y:"), camera1YLineEdit);
//     formLayout->addRow(tr("카메라2 X:"), camera2XLineEdit);
//     formLayout->addRow(tr("카메라2 Y:"), camera2YLineEdit);
//     formLayout->addRow(tr("카메라3 X:"), camera3XLineEdit);
//     formLayout->addRow(tr("카메라3 Y:"), camera3YLineEdit);

//     // 버튼 레이아웃
//     auto *buttonLayout = new QHBoxLayout;
//     buttonLayout->addStretch();
//     buttonLayout->addWidget(registerButton);
//     buttonLayout->addWidget(cancelButton);

//     // 전체 레이아웃
//     auto *mainLayout = new QVBoxLayout;
//     mainLayout->addLayout(formLayout);
//     mainLayout->addLayout(buttonLayout);
//     setLayout(mainLayout);

//     // 시그널 연결
//     connect(registerButton, &QPushButton::clicked, this, &SignupWindow::onRegisterClicked);
//     connect(cancelButton,   &QPushButton::clicked, this, &SignupWindow::onCancelClicked);
// }

// void SignupWindow::onRegisterClicked()
// {
//     const QString id    = idLineEdit->text().trimmed();
//     const QString pw    = pwLineEdit->text();
//     const QString pwCon = pwConfirmLineEdit->text();

//     bool ok1x, ok1y, ok2x, ok2y, ok3x, ok3y;
//     double cam1x = camera1XLineEdit->text().toDouble(&ok1x);
//     double cam1y = camera1YLineEdit->text().toDouble(&ok1y);
//     double cam2x = camera2XLineEdit->text().toDouble(&ok2x);
//     double cam2y = camera2YLineEdit->text().toDouble(&ok2y);
//     double cam3x = camera3XLineEdit->text().toDouble(&ok3x);
//     double cam3y = camera3YLineEdit->text().toDouble(&ok3y);

//     if (id.isEmpty() || pw.isEmpty() || pwCon.isEmpty()) {
//         QMessageBox::warning(this, tr("입력 오류"), tr("모든 항목을 입력하세요."));
//         return;
//     }
//     if (pw != pwCon) {
//         QMessageBox::warning(this, tr("비밀번호 불일치"), tr("비밀번호가 일치하지 않습니다."));
//         return;
//     }

//     if (!ok1x || !ok1y || !ok2x || !ok2y || !ok3x || !ok3y) {
//         QMessageBox::warning(this, tr("입력오류"), tr("카메라 좌표를 모두 정확히 입력하세요."));
//         return;
//     }

//     m_id       = id;
//     m_password = pw;
//     m_camera1X = cam1x;
//     m_camera1Y = cam1y;
//     m_camera2X = cam2x;
//     m_camera2Y = cam2y;
//     m_camera3X = cam3x;
//     m_camera3Y = cam3y;
//     accept();
// }

// void SignupWindow::onCancelClicked()
// {
//     reject();
// }

#include "signupwindow.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QLabel>
#include <QScrollArea>

SignupWindow::SignupWindow(QWidget *parent)
    : QDialog(parent)
    , idLineEdit(new QLineEdit(this))
    , pwLineEdit(new QLineEdit(this))
    , pwConfirmLineEdit(new QLineEdit(this))
    , camera1XLineEdit(new QLineEdit(this))
    , camera1YLineEdit(new QLineEdit(this))
    , camera2XLineEdit(new QLineEdit(this))
    , camera2YLineEdit(new QLineEdit(this))
    , camera3XLineEdit(new QLineEdit(this))
    , camera3YLineEdit(new QLineEdit(this))
    , registerButton(new QPushButton(tr("회원가입"), this))
    , cancelButton(new QPushButton(tr("취소"), this))
{
    setWindowTitle(tr("회원가입"));
    setModal(true);
    setFixedSize(450, 650);

    // 다크 테마 설정
    setStyleSheet(
        "QDialog {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "                stop:0 #1a1a2e, stop:1 #16213e);"
        "}"
        );

    // 타이틀 라벨
    QLabel *titleLabel = new QLabel("회원가입");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 24px;"
        "    font-weight: bold;"
        "    margin: 20px 0px;"
        "}"
        );

    // 입력 필드 공통 스타일
    QString lineEditStyle =
        "QLineEdit {"
        "    background-color: #2a2a3e;"
        "    border: 1px solid #3a3a4e;"
        "    border-radius: 8px;"
        "    padding: 10px 12px;"
        "    color: white;"
        "    font-size: 13px;"
        "    margin: 4px 0px;"
        "}"
        "QLineEdit:focus {"
        "    border: 1px solid #4a9eff;"
        "    background-color: #353550;"
        "}"
        "QLineEdit::placeholder {"
        "    color: #8a8a9a;"
        "}";

    // 라벨 스타일
    QString labelStyle =
        "QLabel {"
        "    color: #cccccc;"
        "    font-size: 13px;"
        "    font-weight: 500;"
        "    margin: 8px 0px 4px 0px;"
        "}";

    // 패스워드 입력 마스킹
    pwLineEdit->setEchoMode(QLineEdit::Password);
    pwConfirmLineEdit->setEchoMode(QLineEdit::Password);

    // placeholder 설정
    idLineEdit->setPlaceholderText("사용할 아이디를 입력하세요");
    pwLineEdit->setPlaceholderText("비밀번호를 입력하세요");
    pwConfirmLineEdit->setPlaceholderText("비밀번호를 다시 입력하세요");

    camera1XLineEdit->setPlaceholderText("X 좌표");
    camera1YLineEdit->setPlaceholderText("Y 좌표");
    camera2XLineEdit->setPlaceholderText("X 좌표");
    camera2YLineEdit->setPlaceholderText("Y 좌표");
    camera3XLineEdit->setPlaceholderText("X 좌표");
    camera3YLineEdit->setPlaceholderText("Y 좌표");

    // 모든 입력 필드에 스타일 적용
    idLineEdit->setStyleSheet(lineEditStyle);
    pwLineEdit->setStyleSheet(lineEditStyle);
    pwConfirmLineEdit->setStyleSheet(lineEditStyle);
    camera1XLineEdit->setStyleSheet(lineEditStyle);
    camera1YLineEdit->setStyleSheet(lineEditStyle);
    camera2XLineEdit->setStyleSheet(lineEditStyle);
    camera2YLineEdit->setStyleSheet(lineEditStyle);
    camera3XLineEdit->setStyleSheet(lineEditStyle);
    camera3YLineEdit->setStyleSheet(lineEditStyle);

    // 회원가입 버튼 스타일
    QString registerButtonStyle =
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #4a9eff, stop:1 #0f7cff);"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px;"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    margin: 16px 4px 8px 4px;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #5aa9ff, stop:1 #1f8cff);"
        "}"
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #3a8eef, stop:1 #0a6cef);"
        "}";

    // 취소 버튼 스타일
    QString cancelButtonStyle =
        "QPushButton {"
        "    background-color: transparent;"
        "    border: 1px solid #4a4a5e;"
        "    border-radius: 8px;"
        "    padding: 12px;"
        "    color: #8a8a9a;"
        "    font-size: 14px;"
        "    margin: 16px 4px 8px 4px;"
        "}"
        "QPushButton:hover {"
        "    border-color: #6a6a7e;"
        "    color: #aaaaaa;"
        "    background-color: #2a2a3e;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1a1a2e;"
        "}";

    registerButton->setStyleSheet(registerButtonStyle);
    cancelButton->setStyleSheet(cancelButtonStyle);

    // 메인 컨테이너 프레임
    QFrame *mainFrame = new QFrame;
    mainFrame->setStyleSheet(
        "QFrame {"
        "    background-color: rgba(42, 42, 62, 0.8);"
        "    border-radius: 12px;"
        "}"
        );

    // 스크롤 영역 (많은 입력 필드를 위해)
    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet(
        "QScrollArea {"
        "    background-color: transparent;"
        "    border: none;"
        "}"
        "QScrollBar:vertical {"
        "    background-color: #2a2a3e;"
        "    width: 8px;"
        "    border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background-color: #4a4a5e;"
        "    border-radius: 4px;"
        "    min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background-color: #5a5a6e;"
        "}"
        );

    // 입력 폼 레이아웃
    QVBoxLayout *formLayout = new QVBoxLayout;
    formLayout->setSpacing(12);
    formLayout->setContentsMargins(30, 30, 30, 20);

    // 계정 정보 섹션
    QLabel *accountLabel = new QLabel("계정 정보");
    accountLabel->setStyleSheet(labelStyle + "font-size: 15px; font-weight: bold; color: #4a9eff; margin-top: 0px;");
    formLayout->addWidget(accountLabel);

    QLabel *idLabel = new QLabel("아이디");
    idLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(idLabel);
    formLayout->addWidget(idLineEdit);

    QLabel *pwLabel = new QLabel("비밀번호");
    pwLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(pwLabel);
    formLayout->addWidget(pwLineEdit);

    QLabel *pwConfirmLabel = new QLabel("비밀번호 확인");
    pwConfirmLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(pwConfirmLabel);
    formLayout->addWidget(pwConfirmLineEdit);

    // 카메라 설정 섹션
    QLabel *cameraLabel = new QLabel("카메라 좌표 설정");
    cameraLabel->setStyleSheet(labelStyle + "font-size: 15px; font-weight: bold; color: #4a9eff; margin-top: 20px;");
    formLayout->addWidget(cameraLabel);

    // 카메라 1
    QLabel *cam1Label = new QLabel("카메라 1");
    cam1Label->setStyleSheet(labelStyle + "margin-top: 16px;");
    formLayout->addWidget(cam1Label);

    QHBoxLayout *cam1Layout = new QHBoxLayout;
    cam1Layout->addWidget(camera1XLineEdit);
    cam1Layout->addWidget(camera1YLineEdit);
    formLayout->addLayout(cam1Layout);

    // 카메라 2
    QLabel *cam2Label = new QLabel("카메라 2");
    cam2Label->setStyleSheet(labelStyle);
    formLayout->addWidget(cam2Label);

    QHBoxLayout *cam2Layout = new QHBoxLayout;
    cam2Layout->addWidget(camera2XLineEdit);
    cam2Layout->addWidget(camera2YLineEdit);
    formLayout->addLayout(cam2Layout);

    // 카메라 3
    QLabel *cam3Label = new QLabel("카메라 3");
    cam3Label->setStyleSheet(labelStyle);
    formLayout->addWidget(cam3Label);

    QHBoxLayout *cam3Layout = new QHBoxLayout;
    cam3Layout->addWidget(camera3XLineEdit);
    cam3Layout->addWidget(camera3YLineEdit);
    formLayout->addLayout(cam3Layout);

    // 스크롤 위젯 설정
    QWidget *scrollWidget = new QWidget;
    scrollWidget->setLayout(formLayout);
    scrollArea->setWidget(scrollWidget);

    // 버튼 레이아웃
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(12);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(registerButton);

    // 메인 레이아웃
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(scrollArea, 1);
    mainLayout->addLayout(buttonLayout);

    QVBoxLayout *frameLayout = new QVBoxLayout(mainFrame);
    frameLayout->setContentsMargins(0, 0, 0, 20);
    frameLayout->addLayout(mainLayout);

    // 최종 레이아웃
    QVBoxLayout *dialogLayout = new QVBoxLayout;
    dialogLayout->setContentsMargins(20, 20, 20, 20);
    dialogLayout->addWidget(mainFrame);
    setLayout(dialogLayout);

    // 시그널 연결
    connect(registerButton, &QPushButton::clicked, this, &SignupWindow::onRegisterClicked);
    connect(cancelButton, &QPushButton::clicked, this, &SignupWindow::onCancelClicked);
}

void SignupWindow::onRegisterClicked()
{
    const QString id = idLineEdit->text().trimmed();
    const QString pw = pwLineEdit->text();
    const QString pwCon = pwConfirmLineEdit->text();

    bool ok1x, ok1y, ok2x, ok2y, ok3x, ok3y;
    double cam1x = camera1XLineEdit->text().toDouble(&ok1x);
    double cam1y = camera1YLineEdit->text().toDouble(&ok1y);
    double cam2x = camera2XLineEdit->text().toDouble(&ok2x);
    double cam2y = camera2YLineEdit->text().toDouble(&ok2y);
    double cam3x = camera3XLineEdit->text().toDouble(&ok3x);
    double cam3y = camera3YLineEdit->text().toDouble(&ok3y);

    QString messageBoxStyle =
        "QMessageBox {"
        "    background-color: #2a2a3e;"
        "    color: white;"
        "}"
        "QMessageBox QPushButton {"
        "    background-color: #4a9eff;"
        "    border: none;"
        "    border-radius: 4px;"
        "    padding: 8px 16px;"
        "    color: white;"
        "    font-weight: bold;"
        "}"
        "QMessageBox QPushButton:hover {"
        "    background-color: #5aa9ff;"
        "}";

    if (id.isEmpty() || pw.isEmpty() || pwCon.isEmpty()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("입력 오류");
        msgBox.setText("모든 항목을 입력하세요.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(messageBoxStyle);
        msgBox.exec();
        return;
    }

    if (pw != pwCon) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("비밀번호 불일치");
        msgBox.setText("비밀번호가 일치하지 않습니다.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(messageBoxStyle);
        msgBox.exec();
        return;
    }

    if (!ok1x || !ok1y || !ok2x || !ok2y || !ok3x || !ok3y) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("입력 오류");
        msgBox.setText("카메라 좌표를 모두 정확히 입력하세요.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(messageBoxStyle);
        msgBox.exec();
        return;
    }

    m_id = id;
    m_password = pw;
    m_camera1X = cam1x;
    m_camera1Y = cam1y;
    m_camera2X = cam2x;
    m_camera2Y = cam2y;
    m_camera3X = cam3x;
    m_camera3Y = cam3y;
    accept();
}

void SignupWindow::onCancelClicked()
{
    reject();
}

