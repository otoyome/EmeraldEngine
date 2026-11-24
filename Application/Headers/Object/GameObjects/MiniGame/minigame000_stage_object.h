#pragma once

#include "object/collision/collision_object.h"
#include "Object/Graphics/model_object.h"
#include "Object/Graphics/effect_object.h"
#include "Object/collision_shape_object.h"

#include "minigame000_roomba_movement.h"
#include "minigame000_roomba_status.h"
#include "minigame000_roomba_input_source.h"

class game_object_serializer;

namespace minigame000
{
	//	ステージ設定情報
	struct stage_settings
	{
		math::float3 world_gravity = math::float3(0.0f, system_units::convert_m_to_unit(-9.8f), 0.0f);	//	重力(m/s2)
		float world_air_density = 1.225f / 1000.0f;	//	空気密度(kg/m^3)

		template<class Archive>
		void serialize(Archive& archive, std::uint32_t version);
	};

	//	ステージオブジェクト
	class minigame_stage_object : public spatial_object
	{
		GAME_OBJECT_DECRLEAR(minigame_stage_object, spatial_object)

		friend class ::game_object_serializer;
		static constexpr std::uint32_t serialize_version = 1;

	public:
		//	内部ステート
		enum class states
		{
			setup_wait,
			collect_effects,
			effects_load_wait,
			classify_effects,
			idle,
		};

	public:
		//	設定情報
		stage_settings& get_setting() {return setting;}
		const stage_settings& get_setting() const {return setting;}
		void set_setting(const stage_settings& stage_setting) {setting = stage_setting;}

		//	振り分けたエフェクト
		const std::vector<std::weak_ptr<effect_object>>& get_trash_effects() const { return trash_effects; }
		const std::vector<std::weak_ptr<effect_object>>& get_bubble_effects() const { return bubble_effects; }

		//	ゴミの最大数
		int get_trash_max() const { return trash_max; }

		//	準備完了
		bool is_completed() const { return state == states::idle; }

		//	準備を始める
		void begin_setup() { state = states::collect_effects; }

	public:
		minigame_stage_object(std::string_view name);
		~minigame_stage_object() override;

		void on_initialize() override;
		void on_finalize() override;

		void fixed_update(float elapsed_time) override;

		void serialize(cereal::JSONOutputArchive& archive) const override;
		void deserialize(cereal::JSONInputArchive& archive) override;

	#if defined(USE_IMGUI)
		void imgui_render_core() override;
	#endif	//	defined(USE_IMGUI)

	private:	//	シリアライズ
		template<class Archive>
		void serialize_fields(Archive& archive, std::uint32_t version);

	private:	//	設定
		std::shared_ptr<model_object> model;					//	モデル
		std::shared_ptr<collision_placement_object> collision;	//	当たり判定
		stage_settings setting;	//	セッティング情報

	private:	//	実行時
		states state;
		std::vector<std::weak_ptr<effect_object>> effects;			//	シーンから集めたエフェクト
		std::vector<std::weak_ptr<effect_object>> trash_effects;	//	ゴミのエフェクト
		std::vector<std::weak_ptr<effect_object>> bubble_effects;	//	泡のエフェクト
		int trash_max;												//	ゴミの最大数
	};
}	//	namespace minigame000
