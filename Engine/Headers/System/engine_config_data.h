#pragma	once

#include <chrono>

#include "Graphics/color_space.h"

//	エンジン用の設定情報
struct engine_config_data
{
	SINGLETON(engine_config_data)

public:
	//	ウィンドウズ関係の情報
	struct windows_data
	{
		LONG window_left = 0;
		LONG window_top = 0;
		//	スクリーンサイズは決めない

		bool vertical_sync = false;	//	垂直同期

		template<class Archive>
		void serialize(Archive& archive, int version);
	} windows;

	//	アンチエイリアス関係の情報
	struct antialias_data
	{
		enum class modes
		{
			NONE,
			FXAA,
			SMAA,
		};
		modes mode = modes::SMAA;

		template<class Archive>
		void serialize(Archive& archive, int version);
	} antialias;

	//	描画関係の情報
	struct rendering_data
	{
		//	カメラ相対描画（描画原点をカメラ位置にする）を有効にするか
		bool camera_relative_rendering = false;

		//	描画色域
		color_space::gamuts rendering_gamut = color_space::gamuts::rec709;

		template<class Archive>
		void serialize(Archive& archive, int version);
	} rendering;

public:
	//	起動時に読み込んだ描画色域(実行中は変わらない)
	color_space::gamuts get_active_rendering_gamut() const { return active_rendering_gamut; }

private:
	color_space::gamuts active_rendering_gamut = color_space::gamuts::rec709;

private:
	engine_config_data();
	~engine_config_data();
	engine_config_data(const engine_config_data&) = delete;
	engine_config_data(engine_config_data&&) = delete;
};
