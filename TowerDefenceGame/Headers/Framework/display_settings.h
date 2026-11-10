#pragma once

#include <optional>
#include <vector>

#include "System/engine_config_data.h"

//	ディスプレイセッティング
class display_settings
{
	SINGLETON(display_settings)

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

	//	変更リクエスト
	struct change_request
	{
		resolution size;
		window_modes mode = window_modes::windowed;
	};

public:
	const std::vector<resolution>& get_resolutions() const { return resolutions; }
	const resolution& get_current_resolution() const { return current_resolution; }
	window_modes get_window_mode() const { return window_mode; }

	//	現在の解像度として保存
	void set_current(const resolution& size, window_modes mode);

	//	保存されている設定
	std::optional<change_request> get_saved_request() const;

private:
	display_settings();
	~display_settings();

	//	モニターが対応している解像度を集める
	void collect_resolutions();

public:
	//	解像度と表示の方法の変更リクエスト
	void request_change(const resolution& size, window_modes mode);

	//	変更中か？
	bool is_changing() const { return request.has_value(); }

	//	変更の要求を取得
	std::optional<change_request> take_request();

private:
	std::vector<resolution> resolutions;
	resolution current_resolution;
	window_modes window_mode = window_modes::windowed;

	std::optional<change_request> request;
};
