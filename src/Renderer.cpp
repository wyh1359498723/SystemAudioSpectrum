#include "Renderer.h"

#include <QPainter>
#include <QColor>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <algorithm>
#include <chrono>
#include <thread>

Renderer::Renderer(QObject* parent) : QThread(parent) {
	m_bands.assign(20, 0.0f);
	m_peaks.assign(20, 0.0f);
	m_frame = QImage(m_viewSize, QImage::Format_RGBA8888);
	m_frame.fill(Qt::black);
}

Renderer::~Renderer() {
	stop();
	wait();
}

void Renderer::stop() {
	m_stop.store(true, std::memory_order_relaxed);
}

void Renderer::setBands(const std::vector<float>& bands) {
	QMutexLocker locker(&m_mutex);
	m_bands = bands;
	if (m_peaks.size() != m_bands.size()) m_peaks.assign(m_bands.size(), 0.0f);
}

void Renderer::setViewportSize(const QSize& size) {
	QMutexLocker locker(&m_mutex);
	if (size != m_viewSize && size.width() > 0 && size.height() > 0) {
		m_viewSize = size;
		m_frame = QImage(m_viewSize, QImage::Format_RGBA8888);
		m_frame.fill(Qt::black);
	}
}

bool Renderer::getLatestFrame(QImage& out) {
	QMutexLocker locker(&m_mutex);
	if (m_frame.isNull()) return false;
	out = m_frame.copy();
	return true;
}

QColor Renderer::colorForValue(float value, int barIndex, int numBars, int y, int height) {
	// Hue based on bar index, brightness based on height level
	float hue = (static_cast<float>(barIndex) / std::max(1, numBars)) * 360.0f;
	float sat = 1.0f;
	float val = 0.3f + 0.7f * (1.0f - static_cast<float>(y) / std::max(1, height));
	QColor c = QColor::fromHsvF(hue / 360.0f, sat, val);
	// emphasize with magnitude
	c.setHsvF(c.hueF(), c.saturationF(), std::min(1.0, c.valueF() * (0.5 + value)));
	return c;
}

void Renderer::renderFrame() {
	QImage frame(m_viewSize, QImage::Format_RGBA8888);
	frame.fill(Qt::black);
	QPainter p(&frame);
	p.setRenderHint(QPainter::Antialiasing, true);

	std::vector<float> bands;
	{
		QMutexLocker locker(&m_mutex);
		bands = m_bands;
	}
	int numBars = static_cast<int>(bands.size());
	if (numBars <= 0) {
		QMutexLocker locker(&m_mutex);
		m_frame = frame;
		return;
	}

	int w = frame.width();
	int h = frame.height();
	int gap = std::max(1, w / (numBars * 20));
	int barWidth = std::max(2, (w - (numBars + 1) * gap) / numBars);

	// update peaks with decay
	{
		QMutexLocker locker(&m_mutex);
		float decay = 0.02f; // per frame
		if (m_peaks.size() != bands.size()) m_peaks.assign(bands.size(), 0.0f);
		for (int i = 0; i < numBars; ++i) {
			m_peaks[i] = std::max(m_peaks[i] - decay, bands[i]);
		}
	}

	for (int i = 0; i < numBars; ++i) {
		float v;
		float peak;
		{
			QMutexLocker locker(&m_mutex);
			v = bands[i];
			peak = m_peaks[i];
		}
		int barHeight = static_cast<int>(v * (h - 10));
		int peakY = h - static_cast<int>(peak * (h - 10));

		int x = gap + i * (barWidth + gap);
		int y = h - barHeight;

		// gradient-like fill by drawing horizontal lines
		for (int yy = y; yy < h; ++yy) {
			QColor c = colorForValue(v, i, numBars, yy - y, barHeight);
			p.setPen(c);
			p.drawLine(x, yy, x + barWidth, yy);
		}

		// draw peak indicator
		p.setPen(QColor(255, 255, 255, 200));
		p.drawLine(x, peakY, x + barWidth, peakY);
	}

	p.end();

	QMutexLocker locker(&m_mutex);
	m_frame = frame;
}

void Renderer::run() {
	using namespace std::chrono;
	auto next = steady_clock::now();
	while (!m_stop.load(std::memory_order_relaxed)) {
		renderFrame();
		next += milliseconds(30);
		std::this_thread::sleep_until(next);
	}
} 