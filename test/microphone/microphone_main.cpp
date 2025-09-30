// Qt Library
#include <QApplication>

// Project headers
#include "microphone_widget.h"

/**
 * @brief Application entry point.
 * Initializes QApplication and shows the main MicrophoneWidget.
 */
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    MicrophoneWidget widget;
    widget.show();

    return app.exec();
}
