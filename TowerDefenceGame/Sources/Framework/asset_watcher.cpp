#include "Framework/asset_watcher.h"

#include "shader.h"
#include "texture.h"
#include "Editor/detail_profiler.h"

#if defined(USE_IMGUI)
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "model_resource.h"
#include "effect_resource.h"
#endif	//	defined(USE_IMGUI)

#if defined(USE_IMGUI)
namespace
{
	//	@tool 書き込みの途中で読まないよう、最後の通知からこれだけ待ってから読み直す
	constexpr std::chrono::milliseconds reload_delay(300);

	enum class asset_kinds
	{
		none,
		model,
		effect,
	};

	asset_kinds get_asset_kind(const std::filesystem::path& path)
	{
		const std::wstring extension = path.extension().wstring();
		if (extension.size() <= 1)
			return asset_kinds::none;

		const std::wstring_view name(extension.c_str() + 1);
		const std::wstring model_extension(model_resource::extension.begin(), model_resource::extension.end());
		const std::wstring effect_extension(effect_resource::extension.begin(), effect_resource::extension.end());
		if (_wcsicmp(name.data(), model_extension.c_str()) == 0)
			return asset_kinds::model;
		if (_wcsicmp(name.data(), effect_extension.c_str()) == 0)
			return asset_kinds::effect;
		return asset_kinds::none;
	}
}

//	@tool フォルダーの変更通知を別スレッドで受け取り、更新されたファイルを溜めておく
struct asset_watcher::directory_watch
{
	using clock = std::chrono::steady_clock;

	std::filesystem::path root;
	HANDLE directory = INVALID_HANDLE_VALUE;
	HANDLE stop_event = nullptr;
	std::thread thread;

	std::mutex mutex;
	std::unordered_map<std::wstring, clock::time_point> pending_files;	//	@tool 更新されたファイル（絶対パス）と最後に通知された時刻

	bool start(const std::filesystem::path& directory_path)
	{
		std::error_code error;
		root = std::filesystem::absolute(directory_path, error).lexically_normal();
		if (error || !std::filesystem::is_directory(root, error))
			return false;

		directory = CreateFileW(root.c_str(),
								FILE_LIST_DIRECTORY,
								FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
								nullptr,
								OPEN_EXISTING,
								FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
								nullptr);
		if (directory == INVALID_HANDLE_VALUE)
			return false;

		stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		if (!stop_event)
		{
			CloseHandle(directory);
			directory = INVALID_HANDLE_VALUE;
			return false;
		}

		thread = std::thread([this] { run(); });
		return true;
	}

	void stop()
	{
		if (stop_event)
			SetEvent(stop_event);
		if (thread.joinable())
			thread.join();
		if (directory != INVALID_HANDLE_VALUE)
			CloseHandle(directory);
		if (stop_event)
			CloseHandle(stop_event);
		directory = INVALID_HANDLE_VALUE;
		stop_event = nullptr;
	}

	void run()
	{
		OVERLAPPED overlapped{};
		overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		if (!overlapped.hEvent)
			return;

		alignas(DWORD) BYTE buffer[64 * 1024];
		while (true)
		{
			ResetEvent(overlapped.hEvent);
			if (!ReadDirectoryChangesW(directory,
									   buffer,
									   sizeof(buffer),
									   TRUE,
									   FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
									   nullptr,
									   &overlapped,
									   nullptr))
			{
				break;
			}

			HANDLE handles[] = { stop_event, overlapped.hEvent };
			const DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
			if (result != WAIT_OBJECT_0 + 1)
			{
				//	@tool 終了要求（または失敗）。読み取り中の要求を取り消し、完了を待ってからバッファを手放す
				CancelIoEx(directory, &overlapped);
				DWORD ignored = 0;
				GetOverlappedResult(directory, &overlapped, &ignored, TRUE);
				break;
			}

			DWORD bytes = 0;
			if (!GetOverlappedResult(directory, &overlapped, &bytes, FALSE))
				break;

			//	@tool 0 バイトは通知があふれた時（取りこぼした変更は次の保存で拾う）
			if (bytes == 0)
				continue;

			collect(buffer);
		}

		CloseHandle(overlapped.hEvent);
	}

	void collect(const BYTE* buffer)
	{
		const auto now = clock::now();
		std::lock_guard<std::mutex> lock(mutex);

		const BYTE* cursor = buffer;
		while (true)
		{
			auto info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(cursor);
			if (info->Action == FILE_ACTION_ADDED ||
				info->Action == FILE_ACTION_MODIFIED ||
				info->Action == FILE_ACTION_RENAMED_NEW_NAME)
			{
				const std::wstring_view name(info->FileName, info->FileNameLength / sizeof(WCHAR));
				const std::filesystem::path path = (root / std::filesystem::path(name)).lexically_normal();
				if (get_asset_kind(path) != asset_kinds::none)
					pending_files[path.wstring()] = now;
			}

			if (info->NextEntryOffset == 0)
				break;
			cursor += info->NextEntryOffset;
		}
	}

	//	@tool 最後の通知から一定時間たったファイルを取り出す
	std::vector<std::filesystem::path> take_ready_files()
	{
		std::vector<std::filesystem::path> files;
		const auto now = clock::now();

		std::lock_guard<std::mutex> lock(mutex);
		for (auto it = pending_files.begin(); it != pending_files.end();)
		{
			if (now - it->second < reload_delay)
			{
				++it;
				continue;
			}
			files.emplace_back(it->first);
			it = pending_files.erase(it);
		}
		return files;
	}
};
#endif	//	defined(USE_IMGUI)

asset_watcher::asset_watcher()
{
}

asset_watcher::~asset_watcher()
{
#if defined(USE_IMGUI)
	if (watch)
		watch->stop();
#endif	//	defined(USE_IMGUI)
}

bool asset_watcher::update()
{
	bool model_reloaded = false;

#if defined(USE_IMGUI)
	//	各種データの更新チェック
	{
		DETAIL_PROFILE_CPU_SCOPE("update.asset_hot_reload");
		if (shader_manager::is_valid())
		{
			shader_manager::instance()->check_shader_update_and_recompile();
		}
		if (texture_manager::is_valid())
		{
			texture_manager::instance()->check_update_and_reload();
		}

		//	@tool 前回の読み直しで差し替えた古い中身は、全ワールドのインスタンスを作り直し終えているので破棄する
		if (model_resource_manager::is_valid())
			model_resource_manager::instance()->release_retired_resources();
		if (effect_resource_manager::is_valid())
			effect_resource_manager::instance()->release_retired_resources();

		//	@tool 監視は最初の更新で始める（作業フォルダーが確定した後）
		if (!watch)
		{
			watch = std::make_unique<directory_watch>();
			if (!watch->start("Assets"))
			{
				DEBUG_WARNING("asset_watcher: failed to watch Assets directory");
			}
			else
			{
				DEBUG_LOG("asset_watcher: watching [%ls]", watch->root.c_str());
			}
		}

		for (auto& path : watch->take_ready_files())
		{
			DEBUG_LOG("asset_watcher: changed [%ls]", path.c_str());
			switch (get_asset_kind(path))
			{
			case asset_kinds::model:
				if (model_resource_manager::is_valid() && model_resource_manager::instance()->reload_file(path))
					model_reloaded = true;
				break;
			case asset_kinds::effect:
				//	@tool エフェクトのインスタンスは各ワールドの effect_manager::update で作り直される
				if (effect_resource_manager::is_valid())
					effect_resource_manager::instance()->reload_file(path);
				break;
			default:
				break;
			}
		}
	}
#endif //	defined(USE_IMGUI)

	return model_reloaded;
}
