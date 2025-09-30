#ifndef MICROPHONE_INPUT_H
#define MICROPHONE_INPUT_H

// Qt Library
#include <QObject>
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>

/**
 * @brief Handles real-time microphone input and converts stereo float samples to mono int16.
 */
class MicrophoneInput : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a MicrophoneInput instance.
     * @param The QObject parent.
     */
    explicit MicrophoneInput(QObject* parent = nullptr);

    /**
     * @brief Destroys the MicrophoneInput instance and stops the input.
     */
    ~MicrophoneInput() override;

    /**
     * @brief Starts capturing audio using the specified format.
     * @param The desired QAudioFormat.
     * @return true if started successfully, false otherwise.
     */
    bool start(const QAudioFormat& format);

    /**
     * @brief Stops audio capture and releases internal resources.
     */
    void stop();

signals:
    /**
     * @brief Emitted when a block of mono PCM audio data is available.
     * @param The raw 16-bit mono audio data.
     */
    void audioCaptured(const QByteArray& pcm);

private slots:
    /**
     * @brief Slot called when audio data is available for reading.
     */
    void handleReadyRead();

private:
    QAudioSource* audio_source = nullptr;
    QIODevice* audio_io_device = nullptr;
    QAudioFormat audio_format;
};

#endif // MICROPHONE_INPUT_H
