#pragma	once

#include <chrono>
#include <string>
#include <vector>

#include "Graphics/Core/color_space.h"

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
		//	カメラ相対描画を有効にするか
		bool camera_relative_rendering = false;

		template<class Archive>
		void serialize(Archive& archive, int version);
	} rendering;

	//	当たり判定の設定
	struct collision_data
	{
		//	応答方式
		enum class responses
		{
			ignore,		//	当たらない
			overlap,	//	押し返さず、重なったことだけ知らせる
			block,		//	押し返す
		};

		//	チャンネルの最大数
		static constexpr size_t max_channels = 32;

		//	チャンネル情報
		struct channel_data
		{
			std::string name;

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	応答情報
		struct response_data
		{
			std::string channel;
			responses response = responses::block;

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	プロファイル情報
		struct profile_data
		{
			std::string name;
			std::string object_type;						//	所属するチャンネル
			std::vector<response_data> channel_responses;	//	既定から変えたチャンネルの応答

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		std::vector<channel_data> channels;
		std::vector<profile_data> profiles;

		template<class Archive>
		void serialize(Archive& archive, int version);
	} collision;

private:
	engine_config_data();
	~engine_config_data();
	engine_config_data(const engine_config_data&) = delete;
	engine_config_data(engine_config_data&&) = delete;
};
