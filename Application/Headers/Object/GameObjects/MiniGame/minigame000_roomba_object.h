#pragma once

#include "object/collision/collision_object.h"
#include "Object/Graphics/model_object.h"
#include "Object/collision_shape_object.h"

#include "minigame000_roomba_movement.h"
#include "minigame000_roomba_status.h"
#include "minigame000_roomba_input_source.h"

class game_object_serializer;

namespace minigame000
{
	//	ルンバ本体
	class roomba_object : public spatial_object
	{
		GAME_OBJECT_DECRLEAR(roomba_object, spatial_object)

		friend class ::game_object_serializer;
		static constexpr std::uint32_t serialize_version = 5;

	public:
		//	移動段階
		enum class move_modes
		{
			walk,	//	普通ルンバ
			dash,	//	爆速ルンバ
		};

	public:
		//	モデル取得
		const std::shared_ptr<model_object>& get_model_object() const { return model; }

		//	当たり判定取得
		const std::shared_ptr<collision_shape_object>& get_collision_object() const { return collision; }

		//	移動制御取得
		const roomba_movement& get_movement() const { return movement; }

		//	状態制御取得
		const roomba_status& get_status() const { return status; }

		//	移動情報取得
		const roomba_movement_snapshot& get_movement_snapshot() const { return movement_snapshot; }

		//	状態情報取得
		const roomba_status_snapshot& get_status_snapshot() const { return status_snapshot; }

		//	移動環境情報設定/取得
		void set_movement_environment(const roomba_movement_environment& value) { movement_environment = value; }
		const roomba_movement_environment& get_movement_environment() const { return movement_environment; }

		//	入力元設定/取得
		void set_input_source(const std::shared_ptr<roomba_input_source>& source) { input_source = source; }
		std::shared_ptr<roomba_input_source> get_input_source() const { return input_source.lock(); }

		//	回転取得
		math::rotatorf get_rotation() const;
		void set_rotation(math::rotatorf rotator);

		//	座標取得
		math::vector3d get_position() const;
		void set_position(const math::vector3d& position);

		//	注視点取得
		math::vector3d get_focus_point() const;

		//	速度取得
		math::float3 get_velocity() const { return movement_snapshot.velocity; }
		void set_velocity(const math::float3& value) { movement_snapshot.velocity = value; }

		//	接地判定
		bool is_on_ground() const { return movement_snapshot.on_ground; }

		//	死亡しているか
		bool is_dead() const { return status_snapshot.dead; }

		//	濡らす
		void add_wet(float value) { pending_wet_value += value; }

	public:
		roomba_object(std::string_view name);
		~roomba_object() override;

		void on_initialize() override;
		void on_finalize() override;

		void fixed_update(float elapsed_time) override;

		void serialize(cereal::JSONOutputArchive& archive) const override;
		void deserialize(cereal::JSONInputArchive& archive) override;

	#if defined(USE_IMGUI)
		void imgui_render_core() override;
	#endif	//	defined(USE_IMGUI)

	public:
		//	移動の状態を本体とモデルへ反映
		//	@remainder	通信でホストの状態を受け取ったときもここを通す
		void apply_movement_snapshot();

	private:	//	シリアライズ
		template<class Archive>
		void serialize_fields(Archive& archive, std::uint32_t version);

	private:	//	設定
		std::shared_ptr<model_object> model;				//	モデル
		std::shared_ptr<collision_shape_object> collision;	//	当たり判定
		roomba_status status;								//	ルンバステータス
		roomba_movement movement;							//	ルンバムーブメント

		roomba_movement_snapshot movement_snapshot;			//	ムーブステート
		roomba_status_snapshot status_snapshot;				//	ステータスステート

		roomba_movement_environment movement_environment;	//	環境情報
		float pending_wet_value = 0.0f;						//	次の固定更新で反映する濡れ量

		std::weak_ptr<roomba_input_source> input_source;	//	入力元
		std::uint32_t next_tick = 0;						//	ネットワーク用処理順
	};
}	//	namespace minigame000
