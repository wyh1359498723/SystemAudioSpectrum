#pragma once

#define NOMINMAX
#include <QThread>
#include <atomic>
#include <memory>
#include "RingBuffer.h"

class AudioCapture : public QThread {
	Q_OBJECT
public:
	explicit AudioCapture(RingBuffer* ringBuffer, QObject* parent = nullptr);
	~AudioCapture() override;

	void stop();
	double sampleRate() const { return m_sampleRate; }

protected:
	void run() override;

private:
	bool initializeWasapi();
	void teardownWasapi();
	bool captureLoop();

	RingBuffer* m_ringBuffer;
	std::atomic<bool> m_stop{false};
	double m_sampleRate{48000.0};

	// WASAPI interfaces (forward-declared to avoid windows headers in header file)
	struct Impl;
	Impl* m_impl;
}; 