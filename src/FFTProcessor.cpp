#include "FFTProcessor.h"
#include "Renderer.h"

#include <kiss_fftr.h>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>

FFTProcessor::FFTProcessor(RingBuffer* ringBuffer, Renderer* renderer, double sampleRate, QObject* parent)
	: QThread(parent), m_ringBuffer(ringBuffer), m_renderer(renderer), m_sampleRate(sampleRate) {
	computeHannWindow();
}

FFTProcessor::~FFTProcessor() {
	stop();
	wait();
}

void FFTProcessor::stop() {
	m_stop.store(true, std::memory_order_relaxed);
}

void FFTProcessor::computeHannWindow() {
	m_window.resize(m_fftSize);
	for (int n = 0; n < m_fftSize; ++n) {
		m_window[n] = 0.5f * (1.0f - std::cos(2.0 * 3.14159265358979323846 * n / (m_fftSize - 1)));
	}
}

void FFTProcessor::computeBands(const std::vector<float>& magnitudes, std::vector<float>& bandsOut) {
	const int numBands = 20;
	bandsOut.assign(numBands, 0.0f);

	int numBins = static_cast<int>(magnitudes.size());
	double binHz = m_sampleRate / static_cast<double>(m_fftSize);

	// Log-spaced bands from ~20Hz to 20kHz, include DC in first band
	double fMin = 20.0;
	double fMax = 20000.0;
	for (int b = 0; b < numBands; ++b) {
		double t0 = static_cast<double>(b) / numBands;
		double t1 = static_cast<double>(b + 1) / numBands;
		double f0 = (b == 0) ? 0.0 : fMin * std::pow(fMax / fMin, t0);
		double f1 = fMin * std::pow(fMax / fMin, t1);
		int i0 = static_cast<int>(std::floor(f0 / binHz));
		int i1 = static_cast<int>(std::ceil(f1 / binHz));
		i0 = std::max(0, std::min(i0, numBins - 1));
		i1 = std::max(i0 + 1, std::min(i1, numBins));

		float sum = 0.0f;
		for (int i = i0; i < i1; ++i) sum += magnitudes[i];
		float bandVal = sum / static_cast<float>(i1 - i0);
		bandsOut[b] = bandVal;
	}

	// Normalize to 0..1 with soft companding
	float maxVal = 1e-6f;
	for (float v : bandsOut) maxVal = std::max(maxVal, v);
	for (float& v : bandsOut) {
		float n = v / maxVal;
		n = std::pow(n, 0.5f); // gamma correction
		v = std::clamp(n, 0.0f, 1.0f);
	}
}

void FFTProcessor::run() {
	std::vector<float> input(m_fftSize, 0.0f);
	std::vector<float> windowed(m_fftSize, 0.0f);
	std::vector<float> mags(m_fftSize / 2, 0.0f);

	kiss_fftr_cfg cfg = kiss_fftr_alloc(m_fftSize, 0, nullptr, nullptr);
	if (!cfg) return;

	std::vector<kiss_fft_cpx> out(m_fftSize / 2 + 1);

	std::vector<float> bands;

	while (!m_stop.load(std::memory_order_relaxed)) {
		// Read hopSize samples into a temporary buffer
		std::vector<float> hop(static_cast<size_t>(m_hopSize));
		size_t got = m_ringBuffer->read(hop.data(), m_hopSize);
		if (got < static_cast<size_t>(m_hopSize)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}

		// Shift buffer and append new hop at the end
		std::rotate(input.begin(), input.begin() + m_hopSize, input.end());
		std::copy(hop.begin(), hop.end(), input.end() - m_hopSize);
		// window and FFT
		for (int i = 0; i < m_fftSize; ++i) windowed[i] = input[i] * m_window[i];
		kiss_fftr(cfg, windowed.data(), out.data());

		for (int i = 0; i < m_fftSize / 2; ++i) {
			float re = out[i].r;
			float im = out[i].i;
			mags[i] = std::sqrt(re * re + im * im);
		}

		computeBands(mags, bands);
		if (m_renderer) m_renderer->setBands(bands);
	}

	kiss_fftr_free(cfg);
} 