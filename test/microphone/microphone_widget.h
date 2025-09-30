#ifndef MICROPHONE_WIDGET_H
#define MICROPHONE_WIDGET_H

// Qt Library
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

// Project headers
#include "microphone.h"

/**
 * @brief UI widget for controlling microphone audio streaming.
 *        Allows user to enter IP and port, and start/stop streaming.
 */
class MicrophoneWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the MicrophoneWidget UI.
     * @param parent Parent QWidget.
     */
    explicit MicrophoneWidget(QWidget* parent = nullptr);

    /**
     * @brief Stops streaming and cleans up.
     */
    ~MicrophoneWidget() override;

private slots:
    /**
     * @brief Attempts to start audio streaming to the provided IP/port.
     */
    void startStreaming();

    /**
     * @brief Stops the audio streaming and disconnects.
     */
    void stopStreaming();

private:
    QLineEdit* ip_edit = nullptr;
    QLineEdit* port_edit = nullptr;
    QPushButton* start_button = nullptr;
    QPushButton* stop_button = nullptr;
    QLabel* status_label = nullptr;

    Microphone* microphone = nullptr;
};

#endif // MICROPHONE_WIDGET_H
