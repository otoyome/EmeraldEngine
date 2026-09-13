#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <DirectXMath.h>

struct timeline_resource
{
public:
	static constexpr std::string_view extension = "etl";

public:
	//	アニメーション
	struct animation_entry
	{
		std::string animation_name;		//	アニメーション
		float start_time = 0.0f;		//	開始時刻
		float end_time = 1.0f;			//	終了時刻
		bool loop = false;				//	ループ再生フラグ

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	//	オブジェクト
	struct object_entry
	{
		enum class kind
		{
			actor,		//	アクター
			camera,		//	カメラ
			effect,		//	エフェクト
			event,		//	イベント
		};

		//	アクター
		struct actor_data
		{
			struct transform_entry
			{
				struct parameter
				{
					DirectX::XMFLOAT3 position = { 0, 0, 0 };
					math::rotator rotation = { 0, 0, 0 };

					template<class Archive>
					void serialize(Archive& archive, int version);
				};

				float start_time = 0.0f;
				float end_time = 1.0f;
				parameter start;
				parameter end;

				template<class Archive>
				void serialize(Archive& archive, int version);
			};

			std::string model_path;
			std::vector<animation_entry> animations;
			std::vector<transform_entry> transforms;

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	カメラ
		struct camera_data
		{
			struct parameter
			{
				DirectX::XMFLOAT3 position = { 0, 0, 0 };
				math::rotator rotation = { 0, 0, 0 };
				float fov_y = 0.5235988f;

				template<class Archive>
				void serialize(Archive& archive, int version);
			};
			float start_time = 0.0f;
			float end_time = 1.0f;
			parameter start;
			parameter end;

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	エフェクト
		struct effect_data
		{
			float start_time = 0.0f;
			float end_time = 1.0f;
			std::string effect_path;
			DirectX::XMFLOAT3 position = { 0, 0, 0 };

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	イベント
		struct event_data
		{
			float time = 0.0f;
			std::string identifier;

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	データ
		using data_variant = std::variant<actor_data, camera_data, effect_data, event_data>;

		std::string name;
		data_variant data;

		kind object_kind() const { return static_cast<kind>(data.index()); }

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

public:
	float total_time = 10.0f;			//	タイムライン全体の長さ
	std::vector<object_entry> objects;	//	オブジェクトリスト

public:
	bool serialize(std::string_view file_path);
	bool deserialize(std::string_view file_path);
};
