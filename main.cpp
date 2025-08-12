#include "SystemAudioSpectrum.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    SystemAudioSpectrum window;
    window.show();
    return app.exec();
}
