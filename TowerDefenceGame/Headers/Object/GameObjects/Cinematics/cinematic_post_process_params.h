#pragma once

#include "texture.h"

//	@tool ActionGame の post_process_volume が持っていたポストプロセス用パラメータ群を
//	@tool こちら（TowerDefenceGame）へ移植したもの。Renderer 側で定数バッファを直接書くのではなく、
//	@tool cinematics_object が post_process_volume のマテリアルへ effect 単位で push する。

namespace postprocess
{
	//	色相補正パーツ
	struct color_grading_part_data
	{
		DirectX::XMFLOAT3 hsv_weight = { 1, 1, 1 };			//	HSV 空間での調整係数
		float dummy = 0.0f;
		DirectX::XMFLOAT3 color_multiplyer = { 1, 1, 1 };	//	乗算値
		float dummy2 = 0.0f;
	};

	struct color_grading_data
	{
		float intensity = 0.0f;				//	適用強度
		float stage_depth_scale = 10.0f;	//	（旧）ステージ深度スケール。移植版では未使用
		DirectX::XMFLOAT2 dummy;

		color_grading_part_data character_data;	//	（旧）キャラクター用
		color_grading_part_data stage_data;		//	（旧）ステージ用。移植版はこちらのみ使用
	};

	//	ラジアルブラー
	struct radial_blur_datas
	{
		float radius = 0.0f;
		int sampling_count = 10;
		DirectX::XMFLOAT2 center = { 0.5f, 0.5f };

		float mask_radius = 0.0f;
		DirectX::XMFLOAT3 dummy;
	};

	//	色収差
	struct separate_color_datas
	{
		float intensity = 0.0f;
		DirectX::XMFLOAT3 color_min = { 0, 0, 0 };	//	閾値以下の色
		float threshold = 0.0f;						//	色閾値
		DirectX::XMFLOAT3 color_max = { 1, 1, 1 };	//	閾値以上の色
	};

	//	テクスチャ上書き（ブレイク演出のロゴ等）
	struct overwrite_text_posterize_color
	{
		float step = 1.0f;
		float animation_rate = 0.0f;
		float amount = 0.0f;
		float dummy = 0.0f;
	};
	struct overwrite_text_datas
	{
		float intensity = 0.0f;
		float texcoord_scale = 1.0f;
		DirectX::XMFLOAT2 texcoord_offset = { 0.0f, 0.0f };
		DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		overwrite_text_posterize_color posterize_data;
	};

	//	集中線
	struct concentrated_line_work_datas
	{
		float intensity = 0.0f;
		float use_animation = 1.0f;
		float animation_speed = 10.0f;
		float pattern_seed = 10.0f;

		float noise_scale = 500.0f;
		DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT2 edge = { 0.5f, 1.0f };
		DirectX::XMFLOAT2 dummy;
	};

	//	歪み
	struct distortion_datas
	{
		float power = 0.0f;
		float speed = 0.0f;
		float amount = 0.0f;
		float dummy = 0.0f;
	};

	//	sin 波による揺らぎ
	struct sin_distortion_data
	{
		float height_amplitude = 0.0f;
		float height_angular_frequency = 0.0f;
		int height_postraize_count = 0;
		float amount = 0.0f;
	};

	//	ボロノイノイズによる揺らぎ
	struct voronoi_distortion_data
	{
		int square_count = 0;
		float amount = 0.0f;
		DirectX::XMFLOAT2 dummy;
	};
}

//	ポストプロセス情報
struct post_process_datas
{
	postprocess::color_grading_data color_grading;
	postprocess::radial_blur_datas radial_blur;
	postprocess::separate_color_datas separate_color;

	postprocess::overwrite_text_datas overwrite_text;
	texture_ptr overwrite_text_texture;

	postprocess::concentrated_line_work_datas concentrated_line_work;
	postprocess::distortion_datas distortion_data;
	postprocess::sin_distortion_data sin_distortion_datas[2];

	postprocess::voronoi_distortion_data voronoi_distortion_data;
};
