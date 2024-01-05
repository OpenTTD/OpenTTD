/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file http_shared.h Shared functions for implementations of HTTP requests.
 */

#ifndef NETWORK_CORE_HTTP_SHARED_H
#define NETWORK_CORE_HTTP_SHARED_H

#include "http.h"

#include <condition_variable>
#include <mutex>
#include <vector>

/** Converts a HTTPCallback to a Thread-Safe variant. */
class HTTPThreadSafeCallback {
private:
	/** Entries on the queue for later handling. */
	class Callback {
	public:
		Callback(std::unique_ptr<char[]> data, size_t length) : data(std::move(data)), length(length), failure(false) {}
		Callback() : data(nullptr), length(0), failure(true) {}

		std::unique_ptr<char[]> data;
		size_t length;
		bool failure;
	};

public:
	/**
	 * Similar to HTTPCallback::OnFailure, but thread-safe.
	 */
	void OnFailure()
	{
		std::lock_guard<std::mutex> lock(this->mutex);
		this->queue.emplace_back();
	}

	/**
	 * Similar to HTTPCallback::OnReceiveData, but thread-safe.
	 */
	void OnReceiveData(std::unique_ptr<char[]> data, size_t length)
	{
		std::lock_guard<std::mutex> lock(this->mutex);
		this->queue.emplace_back(std::move(data), length);
	}

	/**
	 * Process everything on the queue.
	 *
	 * Should be called from the Game Thread.
	 */
	void HandleQueue()
	{
		this->cancelled = callback->IsCancelled();

		std::lock_guard<std::mutex> lock(this->mutex);

		for (auto &item : this->queue) {
			if (item.failure) {
				this->callback->OnFailure();
			} else {
				this->callback->OnReceiveData(std::move(item.data), item.length);
			}
		}

		this->queue.clear();
		this->queue_cv.notify_all();
	}

	/**
	 * Wait till the queue is dequeued, or a condition is met.
	 * @param condition Condition functor.
	 */
	template <typename T>
	void WaitTillEmptyOrCondition(T condition)
	{
		std::unique_lock<std::mutex> lock(this->mutex);

		while (!(queue.empty() || condition())) {
			this->queue_cv.wait(lock);
		}
	}

	/**
	 * Check if the queue is empty.
	 */
	bool IsQueueEmpty()
	{
		std::lock_guard<std::mutex> lock(this->mutex);
		return this->queue.empty();
	}

	HTTPThreadSafeCallback(HTTPCallback *callback) : callback(callback) {}

	~HTTPThreadSafeCallback()
	{
		std::lock_guard<std::mutex> lock(this->mutex);

		/* Clear the list and notify explicitly. */
		queue.clear();
		queue_cv.notify_all();
	}

	std::atomic<bool> cancelled = false;

private:
	HTTPCallback *callback; ///< The callback to send data back on.
	std::mutex mutex; ///< Mutex to protect the queue.
	std::vector<Callback> queue; ///< Queue of data to send back.
	std::condition_variable queue_cv; ///< Condition variable to wait for the queue to be empty.
};

#endif /* NETWORK_CORE_HTTP_SHARED_H */
