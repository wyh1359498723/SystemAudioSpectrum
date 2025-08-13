#include "SystemAudioSpectrum.h"

#include <QPainter>
#include <QResizeEvent>
#include <QDebug>

#include "RingBuffer.h"
#include "AudioCapture.h"
#include "FFTProcessor.h"
#include "Renderer.h"

SystemAudioSpectrum::SystemAudioSpectrum(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    setMinimumSize(800, 400);

    m_ring = new RingBuffer(48000 * 10);
    m_capture = new AudioCapture(m_ring, this);
    m_renderer = new Renderer(this);

    // Start capture first to know sample rate, then FFT
    m_capture->start();
    m_renderer->start();

    // Wait briefly to get sample rate; fallback to 48000
    double sr = 48000.0;
    if (m_capture) sr = m_capture->sampleRate();
    m_fft = new FFTProcessor(m_ring, m_renderer, sr, this);
    m_fft->start();

    connect(&m_uiTimer, &QTimer::timeout, this, [this]() {
        if (m_renderer && m_renderer->getLatestFrame(m_lastFrame)) {
            update();
        }
    });
    m_uiTimer.start(30);
}

SystemAudioSpectrum::~SystemAudioSpectrum()
{
    if (m_fft) { m_fft->stop(); m_fft->wait(); }
    if (m_capture) { m_capture->stop(); m_capture->wait(); }
    if (m_renderer) { m_renderer->stop(); m_renderer->wait(); }

    delete m_fft;
    m_fft = nullptr;

    delete m_capture;
    m_capture = nullptr;

    delete m_ring;
    m_ring = nullptr;
}

void SystemAudioSpectrum::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (!m_lastFrame.isNull()) {
        p.drawImage(rect(), m_lastFrame);
    }
}

void SystemAudioSpectrum::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_renderer) m_renderer->setViewportSize(event->size());
}

