#pragma once

#include <cstdint>

//	描画設定
struct render_settings
{
	//	アンチエイリアス情報
	enum class antialias_modes
	{
		none,
		fxaa,
		smaa,
	};

	//	クラスタードライティング
	struct clustered_lighting_settings
	{
		bool enable = true;				//	影無しの点光源/スポットライトをクラスタードライティングで描画するか
		float far_distance = 0.0f;		//	クラスターの深度分割最大距離
		bool depth_culling = true;		//	深度カリングを行うかどうか

#if defined(USE_IMGUI)
		bool visualize = false;			//	クラスターごとのライト数をヒートマップで表示するか
		float visualize_max = 32.0f;	//	ヒートマップで最大色になるライト数
#endif	//	defined(USE_IMGUI)
	};

	//	シャドウマップアトラス
	struct shadow_atlas_settings
	{
		bool enable = true;					//	シャドウマップアトラスを使うか
		unsigned int resolution = 512;		//	1スライスの解像度
		unsigned int slices = 64;			//	スライス数
		unsigned int max_lights = 32;		//	割り当てるライトの最大数
		bool cache = true;					//	変化が無ければ影を描き直さないか
		std::uint64_t keep_frames = 120;	//	使われなくなったライトのスライスを持ち続けるフレーム数
	};

	antialias_modes antialias_mode;

	clustered_lighting_settings clustered_lighting;

	shadow_atlas_settings shadow_atlas;
};
