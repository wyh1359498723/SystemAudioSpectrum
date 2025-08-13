#pragma once

#include <QThread>
#include <vector>
#include <atomic>
#include <memory>
#include "RingBuffer.h"

struct kiss_fftr_state;

class Renderer;

class FFTProcessor : public QThread {
	Q_OBJECT
public:
	FFTProcessor(RingBuffer* ringBuffer, Renderer* renderer, double sampleRate, QObject* parent = nullptr);
	~FFTProcessor() override;

	void stop();

protected:
	void run() override;

private:
	void computeHannWindow();
	void computeBands(const std::vector<float>& magnitudes, std::vector<float>& bandsOut);

	RingBuffer* m_ringBuffer;
	Renderer* m_renderer;
	std::atomic<bool> m_stop{false};
	double m_sampleRate;

	int m_fftSize = 2048;
	int m_hopSize = 512;
	std::vector<float> m_window;
}; 