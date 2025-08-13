#pragma once

#include <vector>
#include <cstddef>
#include <atomic>
#include <mutex>

class RingBuffer {
public:
	explicit RingBuffer(size_t capacitySamples)
		: m_buffer(capacitySamples + 1), m_capacity(capacitySamples + 1), m_head(0), m_tail(0) {}

	size_t capacity() const { return m_capacity - 1; }

	void clear() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_head = m_tail = 0;
	}

	size_t availableToRead() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return distance_nolock(m_tail, m_head);
	}

	size_t availableToWrite() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_capacity - 1 - distance_nolock(m_tail, m_head);
	}

	size_t write(const float* data, size_t count) {
		if (count == 0) return 0;
		std::lock_guard<std::mutex> lock(m_mutex);
		size_t writable = m_capacity - 1 - distance_nolock(m_tail, m_head);
		size_t toWrite = count < writable ? count : writable;
		for (size_t i = 0; i < toWrite; ++i) {
			m_buffer[m_head] = data[i];
			m_head = (m_head + 1) % m_capacity;
		}
		return toWrite;
	}

	size_t read(float* out, size_t maxCount) {
		if (maxCount == 0) return 0;
		std::lock_guard<std::mutex> lock(m_mutex);
		size_t readable = distance_nolock(m_tail, m_head);
		size_t toRead = maxCount < readable ? maxCount : readable;
		for (size_t i = 0; i < toRead; ++i) {
			out[i] = m_buffer[m_tail];
			m_tail = (m_tail + 1) % m_capacity;
		}
		return toRead;
	}

private:
	size_t distance_nolock(size_t from, size_t to) const {
		return (to + m_capacity - from) % m_capacity;
	}

	std::vector<float> m_buffer;
	size_t m_capacity;
	mutable std::mutex m_mutex;
	size_t m_head;
	size_t m_tail;
}; 