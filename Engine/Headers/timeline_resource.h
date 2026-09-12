#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <DirectXMath.h>

struct timeline_resource
{
	static constexpr std::string_view extension = "etl";

public:
	struct animation_entry
	{
		std::string animation_name;	//	再生するアニメーションクリップ名
		int start_frame = 0;		//	タイムライン上の開始フレーム
		int end_frame = 30;			//	タイムライン上の終了フレーム
		bool loop = false;			//	区間内でループ再生するか

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct actor_entry
	{
		std::string name;							//	表示名
		std::string model_path;						//	モデルファイルパス(.emo)
		std::vector<animation_entry> animations;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct effect_entry
	{
		int start_frame = 0;								//	発火フレーム
		int end_frame = 30;									//	消滅フレーム（表示上の長さ）

		std::string effect_path;							//	エフェクトファイルパス(.eme)
		int actor_index = -1;								//	追従するアクターの添字。-1 ならワールド座標基準
		DirectX::XMFLOAT3 offset_position = { 0, 0, 0 };	//	actor_index>=0: アクター座標からのオフセット / -1: ワールド座標そのもの

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct camera_entry
	{
		struct parameter
		{
			DirectX::XMFLOAT3 position = { 0, 0, 0 };	//	座標
			math::rotator rotation = { 0, 0, 0 };		//	オイラー角
			float fov_y = 0.5235988f;					//	視野角

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		int start_frame = 0;	//	区間開始フレーム
		int end_frame = 30;		//	区間終了フレーム
		parameter start;		//	開始パラメーター
		parameter end;			//	終了パラメーター

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	float fps = 30.0f;					//	フレームレート
	int total_frame = 300;				//	タイムライン全体の長さ

	std::vector<actor_entry> actors;
	std::vector<effect_entry> effects;
	std::vector<camera_entry> cameras;

	bool serialize(std::string_view file_path);
	bool deserialize(std::string_view file_path);
};
