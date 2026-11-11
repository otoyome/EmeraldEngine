#pragma	once

#include <chrono>
#include <string>

#include "Graphics/color_space.h"

//	エンジン用の設定情報
struct engine_config_data
{
	SINGLETON(engine_config_data)

public:
	//	アプリケーションの情報
	struct application_data
	{
		cache_string name = "Sample Engine";	//	アプリケーションの名前
		cache_string splash_image_path = "Assets/Resources/Textures/splash_window/splash.png";	//	起動時のスプラッシュ画像
		cache_string icon_path = "Assets/Resources/Textures/app_icon/app_icon.ico";	//	ウィンドウのアイコン

		template<class Archive>
		void serialize(Archive& archive, int version);
	} application;

	//	ウィンドウズ関係の情報
	struct windows_data
	{
		//	表示の方法
		enum class window_modes
		{
			windowed,	//	ウィンドウ
			borderless,	//	枠なしでモニター全体を覆う（バックバッファは画面の大きさに引き伸ばす）
		};

		LONG window_left = 0;		//	ウィンドウの左位置
		LONG window_top = 0;		//	ウィンドウの上位置
		UINT screen_width = 1280;	//	バックバッファの幅
		UINT screen_height = 720;	//	バックバッファの高さ
		window_modes window_mode = window_modes::windowed;
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
		color_space::gamuts rendering_gamut = color_space::gamuts::rec2020;

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
