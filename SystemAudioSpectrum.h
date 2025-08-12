#pragma once

#include <QtWidgets/QWidget>
#include "ui_SystemAudioSpectrum.h"

class SystemAudioSpectrum : public QWidget
{
    Q_OBJECT

public:
    SystemAudioSpectrum(QWidget *parent = nullptr);
    ~SystemAudioSpectrum();

private:
    Ui::SystemAudioSpectrumClass ui;
};

