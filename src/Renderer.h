#pragma once

#include <QThread>
#include <QImage>
#include <QSize>
#include <QMutex>
#include <vector>
#include <atomic>

class Renderer : public QThread {
	Q_OBJECT
public:
	Renderer(QObject* parent = nullptr);
	~Renderer() override;

	void stop();
	void setBands(const std::vector<float>& bands);
	void setViewportSize(const QSize& size);
	bool getLatestFrame(QImage& out);

protected:
	void run() override;

private:
	void renderFrame();
	QColor colorForValue(float value, int barIndex, int numBars, int y, int height);

	QMutex m_mutex;
	std::vector<float> m_bands;
	std::vector<float> m_peaks;
	QSize m_viewSize{800, 400};
	QImage m_frame;
	std::atomic<bool> m_stop{false};
}; 