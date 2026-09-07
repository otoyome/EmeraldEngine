#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

//	並列実行
class parallel_for_executor
{
public:
	//	worker_index … 0 は呼び出しスレッド、1.. は常駐ワーカー
	using range_function = std::function<void(std::size_t worker_index, std::size_t begin, std::size_t end)>;

	explicit parallel_for_executor(unsigned thread_count = 0)
	{
		unsigned hardware = thread_count ? thread_count : std::thread::hardware_concurrency();
		if (hardware == 0)
			hardware = 4;

		//	呼び出しスレッドで 1 枠使うので常駐ワーカーは hardware - 1
		const unsigned worker_threads = (hardware > 1) ? (hardware - 1) : 0;
		workers.reserve(worker_threads);
		for (unsigned i = 0; i < worker_threads; ++i)
			workers.emplace_back([this] { worker_loop(); });
	}

	~parallel_for_executor()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			shutting_down = true;
		}
		work_condition.notify_all();
		for (auto& worker : workers)
			if (worker.joinable())
				worker.join();
	}

	parallel_for_executor(const parallel_for_executor&) = delete;
	parallel_for_executor& operator=(const parallel_for_executor&) = delete;

	std::size_t max_workers() const { return workers.size() + 1; }

	void parallel_for(std::size_t count, std::size_t grain, const range_function& body)
	{
		if (count == 0)
			return;

		std::size_t split = max_workers();
		if (grain > 0)
		{
			const std::size_t by_grain = (count + grain - 1) / grain;
			if (by_grain < split)
				split = by_grain;
		}
		if (split <= 1 || workers.empty())
		{
			body(0, 0, count);
			return;
		}

		const std::size_t chunk = (count + split - 1) / split;

		{
			std::lock_guard<std::mutex> lock(mutex);
			for (std::size_t worker_index = 1; worker_index < split; ++worker_index)
			{
				const std::size_t begin = worker_index * chunk;
				const std::size_t end = (begin + chunk < count) ? (begin + chunk) : count;
				tasks.emplace_back([this, &body, worker_index, begin, end]
				{
					if (begin < end)
						body(worker_index, begin, end);
					if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
					{
						std::lock_guard<std::mutex> done_lock(mutex);
						done_condition.notify_one();
					}
				});
			}
			remaining.store(split - 1, std::memory_order_release);
		}
		work_condition.notify_all();

		{
			const std::size_t end = (chunk < count) ? chunk : count;
			body(0, 0, end);
		}

		//	完了待ち
		{
			std::unique_lock<std::mutex> lock(mutex);
			done_condition.wait(lock, [this] { return remaining.load(std::memory_order_acquire) == 0; });
		}
	}

private:
	void worker_loop()
	{
		for (;;)
		{
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(mutex);
				work_condition.wait(lock, [this] { return shutting_down || !tasks.empty(); });
				if (shutting_down && tasks.empty())
					return;
				task = std::move(tasks.front());
				tasks.pop_front();
			}
			task();
		}
	}

	std::vector<std::thread> workers;
	std::mutex mutex;
	std::condition_variable work_condition;
	std::condition_variable done_condition;
	std::deque<std::function<void()>> tasks;
	std::atomic<std::size_t> remaining{ 0 };
	bool shutting_down = false;
};
