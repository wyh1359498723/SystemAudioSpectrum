#define NOMINMAX
#include "AudioCapture.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys.h>
#include <avrt.h>

#include <vector>
#include <cmath>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "avrt.lib")

struct AudioCapture::Impl {
	IMMDeviceEnumerator* enumerator = nullptr;
	IMMDevice* device = nullptr;
	IAudioClient* audioClient = nullptr;
	IAudioCaptureClient* captureClient = nullptr;
	HANDLE hTask = nullptr;
	WAVEFORMATEX* mixFormat = nullptr;
};

AudioCapture::AudioCapture(RingBuffer* ringBuffer, QObject* parent)
	: QThread(parent), m_ringBuffer(ringBuffer), m_impl(new Impl) {}

AudioCapture::~AudioCapture() {
	stop();
	wait();
	delete m_impl;
}

void AudioCapture::stop() {
	m_stop.store(true, std::memory_order_relaxed);
}

void AudioCapture::run() {
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		return;
	}

	if (!initializeWasapi()) {
		teardownWasapi();
		CoUninitialize();
		return;
	}

	captureLoop();

	teardownWasapi();
	CoUninitialize();
}

bool AudioCapture::initializeWasapi() {
	HRESULT hr = S_OK;

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator), (void**)&m_impl->enumerator);
	if (FAILED(hr)) return false;

	hr = m_impl->enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_impl->device);
	if (FAILED(hr)) return false;

	hr = m_impl->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_impl->audioClient);
	if (FAILED(hr)) return false;

	hr = m_impl->audioClient->GetMixFormat(&m_impl->mixFormat);
	if (FAILED(hr) || !m_impl->mixFormat) return false;

	// Prefer float32
	WAVEFORMATEX* fmt = m_impl->mixFormat;
	m_sampleRate = static_cast<double>(fmt->nSamplesPerSec);

	hr = m_impl->audioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_LOOPBACK,
		0, 0, fmt, nullptr);
	if (FAILED(hr)) return false;

	hr = m_impl->audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_impl->captureClient);
	if (FAILED(hr)) return false;

	DWORD taskIndex = 0;
	m_impl->hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

	hr = m_impl->audioClient->Start();
	if (FAILED(hr)) return false;

	return true;
}

void AudioCapture::teardownWasapi() {
	if (m_impl->audioClient) {
		m_impl->audioClient->Stop();
	}
	if (m_impl->hTask) {
		AvRevertMmThreadCharacteristics(m_impl->hTask);
		m_impl->hTask = nullptr;
	}
	if (m_impl->mixFormat) {
		CoTaskMemFree(m_impl->mixFormat);
		m_impl->mixFormat = nullptr;
	}
	if (m_impl->captureClient) {
		m_impl->captureClient->Release();
		m_impl->captureClient = nullptr;
	}
	if (m_impl->audioClient) {
		m_impl->audioClient->Release();
		m_impl->audioClient = nullptr;
	}
	if (m_impl->device) {
		m_impl->device->Release();
		m_impl->device = nullptr;
	}
	if (m_impl->enumerator) {
		m_impl->enumerator->Release();
		m_impl->enumerator = nullptr;
	}
}

static void convertToMonoFloat(const BYTE* src, UINT32 numFrames, const WAVEFORMATEX* fmt, std::vector<float>& out) {
	out.clear();
	out.reserve(numFrames);

	WORD channels = fmt->nChannels;
	WORD bits = fmt->wBitsPerSample;

	if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
		(fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE && bits == 32)) {
		const float* f = reinterpret_cast<const float*>(src);
		for (UINT32 i = 0; i < numFrames; ++i) {
			float sum = 0.0f;
			for (UINT32 c = 0; c < channels; ++c) sum += f[i * channels + c];
			out.push_back(sum / static_cast<float>(channels));
		}
		return;
	}

	if (bits == 16) {
		const int16_t* s = reinterpret_cast<const int16_t*>(src);
		for (UINT32 i = 0; i < numFrames; ++i) {
			int sum = 0;
			for (UINT32 c = 0; c < channels; ++c) sum += s[i * channels + c];
			float v = static_cast<float>(sum / static_cast<int>(channels)) / 32768.0f;
			out.push_back(v);
		}
		return;
	}

	// Fallback: zero
	out.insert(out.end(), numFrames, 0.0f);
}

bool AudioCapture::captureLoop() {
	BYTE* data = nullptr;
	UINT32 numFrames = 0;
	DWORD flags = 0;

	const UINT32 bytesPerFrame = m_impl->mixFormat->nBlockAlign;
	std::vector<float> mono;

	while (!m_stop.load(std::memory_order_relaxed)) {
		UINT32 packetSize = 0;
		HRESULT hr = m_impl->captureClient->GetNextPacketSize(&packetSize);
		if (FAILED(hr)) break;
		if (packetSize == 0) {
			Sleep(5);
			continue;
		}

		hr = m_impl->captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
		if (FAILED(hr)) break;

		if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
			mono.assign(numFrames, 0.0f);
		} else {
			convertToMonoFloat(data, numFrames, m_impl->mixFormat, mono);
		}

		if (!mono.empty()) {
			m_ringBuffer->write(mono.data(), mono.size());
		}

		hr = m_impl->captureClient->ReleaseBuffer(numFrames);
		if (FAILED(hr)) break;
	}

	return true;
} 