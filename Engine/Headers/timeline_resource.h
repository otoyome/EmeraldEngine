#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <DirectXMath.h>

struct timeline_resource
{
	static constexpr std::string_view extension = "etl";

public:
	//	アニメーション
	struct animation_entry
	{
		std::string animation_name;		//	アニメーション
		float start_time = 0.0f;		//	開始時間
		float end_time = 1.0f;			//	終了時間
		bool loop = false;				//	ループ再生フラグ

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	//	アクター
	struct actor_entry
	{
		struct transform_entry
		{
			struct parameter
			{
				DirectX::XMFLOAT3 position = { 0, 0, 0 };	//	座標
				math::rotator rotation = { 0, 0, 0 };		//	オイラー角

				template<class Archive>
				void serialize(Archive& archive, int version);
			};

			float start_time = 0.0f;	//	区間開始
			float end_time = 1.0f;		//	区間終了
			parameter start;			//	開始パラメーター
			parameter end;				//	終了パラメーター

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		std::string name;							//	表示名
		std::string model_path;						//	ファイルパス
		std::vector<animation_entry> animations;	//	アニメーション
		std::vector<transform_entry> transforms;	//	配置

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	//	エフェクト
	struct effect_entry
	{
		float start_time = 0.0f;							//	開始時間
		float end_time = 1.0f;								//	終了時間

		std::string effect_path;							//	エフェクトファイルパス
		int actor_index = -1;								//	追従するアクターの番号。-1 ならワールド座標基準
		DirectX::XMFLOAT3 offset_position = { 0, 0, 0 };	//	actor_index = -1 ワールド座標、それ以外はアクター座標からのオフセット

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	//	カメラ
	struct camera_entry
	{
		struct parameter
		{
			DirectX::XMFLOAT3 position = { 0, 0, 0 };	//	座標
			math::rotator rotation = { 0, 0, 0 };		//	オイラー角
			float fov_y = 0.5235988f;					//	画角

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		float start_time = 0.0f;	//	区間開始
		float end_time = 1.0f;		//	区間終了
		parameter start;			//	開始パラメーター
		parameter end;				//	終了パラメーター

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	float total_time = 10.0f;			//	タイムライン全体の長さ

	std::vector<actor_entry> actors;
	std::vector<effect_entry> effects;
	std::vector<camera_entry> cameras;

	bool serialize(std::string_view file_path);
	bool deserialize(std::string_view file_path);
};
