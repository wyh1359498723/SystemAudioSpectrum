#pragma once

#include <QtWidgets/QWidget>
#include <QTimer>
#include <QImage>
#include "ui_SystemAudioSpectrum.h"

class RingBuffer;
class AudioCapture;
class FFTProcessor;
class Renderer;

class SystemAudioSpectrum : public QWidget
{
    Q_OBJECT

public:
    SystemAudioSpectrum(QWidget *parent = nullptr);
    ~SystemAudioSpectrum();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    Ui::SystemAudioSpectrumClass ui;

    RingBuffer* m_ring{nullptr};
    AudioCapture* m_capture{nullptr};
    FFTProcessor* m_fft{nullptr};
    Renderer* m_renderer{nullptr};
    QTimer m_uiTimer;
    QImage m_lastFrame;
};

