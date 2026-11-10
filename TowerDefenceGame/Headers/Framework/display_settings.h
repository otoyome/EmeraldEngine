#pragma once

#include <optional>
#include <vector>

#include "System/engine_config_data.h"

class app_window;

//	ディスプレイセッティング
class display_settings
{
	SINGLETON_DECREATE(display_settings)

public:
	using window_modes = engine_config_data::windows_data::window_modes;

	//	解像度
	struct resolution
	{
		UINT width = 0;
		UINT height = 0;

		bool operator==(const resolution& other) const { return width == other.width && height == other.height; }
		bool operator!=(const resolution& other) const { return !(*this == other); }
	};

public:
	static inline void create_instance(app_window& window)
	{
		if (!instance_ptr)
		{
			instance_ptr = new display_settings(window);
		}
	}

public:
	const std::vector<resolution>& get_resolutions() const { return resolutions; }
	const resolution& get_current_resolution() const { return current_resolution; }
	window_modes get_window_mode() const { return window_mode; }

private:
	explicit display_settings(app_window& window);
	~display_settings();

	//	モニターが対応している解像度を集める
	void collect_resolutions();

public:
	//	更新処理
	void update();

	//	解像度と表示の方法の変更リクエスト
	void request_change(const resolution& size, window_modes mode);

	//	変更中か？
	bool is_changing() const { return request.has_value(); }

private:
	//	変更フェーズ
	enum class phases
	{
		idle,		//	待機
		released,	//	バックバッファパスを解放
		//	ここで処理
	};

	struct change_request
	{
		resolution size;
		window_modes mode = window_modes::windowed;
	};

private:
	app_window& window;
	std::vector<resolution> resolutions;
	resolution current_resolution;
	window_modes window_mode = window_modes::windowed;

	std::optional<change_request> request;
	phases phase = phases::idle;
};
