// Qt Library
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QDebug>
#include <QFont>

// Project headers
#include "microphone_widget.h"

// === Constructor ===
MicrophoneWidget::MicrophoneWidget(QWidget* parent)
    : QWidget(parent)
{
    ip_edit = new QLineEdit(this);
    ip_edit->setPlaceholderText("Enter IP address");

    port_edit = new QLineEdit(this);
    port_edit->setPlaceholderText("Enter port number");

    start_button = new QPushButton("Start", this);
    stop_button = new QPushButton("Stop", this);

    status_label = new QLabel("Status: Idle", this);
    status_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QFont font = status_label->font();
    font.setPointSize(10);
    status_label->setFont(font);

    QHBoxLayout* conn_layout = new QHBoxLayout();
    conn_layout->addWidget(new QLabel("IP:", this));
    conn_layout->addWidget(ip_edit);
    conn_layout->addWidget(new QLabel("Port:", this));
    conn_layout->addWidget(port_edit);

    QHBoxLayout* btn_layout = new QHBoxLayout();
    btn_layout->addWidget(start_button);
    btn_layout->addWidget(stop_button);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(status_label);
    layout->addLayout(conn_layout);
    layout->addLayout(btn_layout);

    setLayout(layout);
    setWindowTitle("Microphone Widget");
    resize(400, 150);

    connect(start_button, &QPushButton::clicked, this, &MicrophoneWidget::startStreaming);
    connect(stop_button, &QPushButton::clicked, this, &MicrophoneWidget::stopStreaming);
}

// === Destructor ===
MicrophoneWidget::~MicrophoneWidget()
{
    stopStreaming();
}

// === Attempts to start audio streaming to the provided IP/port ===
void MicrophoneWidget::startStreaming()
{
    if (microphone)
        return;

    const QString ip = ip_edit->text().trimmed();
    if (ip.isEmpty())
    {
        status_label->setText("Status: IP address is empty");
        return;
    }

    QHostAddress address;
    if (!address.setAddress(ip))
    {
        status_label->setText("Status: Invalid IP address");
        return;
    }

    bool ok = false;
    const quint16 port = port_edit->text().toUShort(&ok);
    if (!ok || port == 0)
    {
        status_label->setText("Status: Invalid port");
        return;
    }

    microphone = new Microphone(this);
    if (microphone->start(ip, port))
    {
        status_label->setText("Status: Streaming...");
        qDebug() << "[UI] Microphone streaming started.";
    }
    else
    {
        status_label->setText("Status: Failed to start streaming");
        delete microphone;
        microphone = nullptr;
    }
}

// === Stops the audio streaming and disconnects ===
void MicrophoneWidget::stopStreaming()
{
    if (!microphone) return;

    microphone->stop();
    delete microphone;
    microphone = nullptr;

    status_label->setText("Status: Stopped");
    qDebug() << "[UI] Microphone streaming stopped.";
}
