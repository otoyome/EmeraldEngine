#pragma once

#include "object/collision/collision_object.h"
#include "object/model_object.h"
#include "Object/collision_shape_object.h"

#include "minigame000_roomba_movement.h"

class game_object_serializer;

namespace minigame000
{
	//	ルンバ本体
	class roomba_object : public spatial_object
	{
		GAME_OBJECT_DECRLEAR(roomba_object, spatial_object)

		friend class ::game_object_serializer;
		static constexpr std::uint32_t serialize_version = 3;

	public:
		//	移動段階
		enum class move_modes
		{
			walk,	//	普通ルンバ
			dash,	//	爆速ルンバ
		};

		//	入力情報
		struct action_datas
		{
			math::float3 move = { 0.0f, 0.0f, 0.0f };	//	移動方向
			bool dash = false;							//	ダッシュ
			bool brake = false;							//	ブレーキ
		};

	public:
		const std::shared_ptr<model_object>& get_model_object() const { return model; }
		const std::shared_ptr<collision_shape_object>& get_collision_object() const { return collision; }
		const roomba_movement& get_movement() const { return movement; }
		const roomba_movement_state& get_movement_state() const { return movement_state; }

		void set_movement_environment(const roomba_movement_environment& value) { movement_environment = value; }
		const roomba_movement_environment& get_movement_environment() const { return movement_environment; }

		math::rotatorf get_rotation() const;
		void set_rotation(math::rotatorf rotator);

		math::vector3d get_position() const;
		void set_position(const math::vector3d& position);

		math::float3 get_velocity() const { return movement_state.velocity; }
		void set_velocity(const math::float3& value) { movement_state.velocity = value; }

		bool is_on_ground() const { return movement_state.on_ground; }

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
		//	入力を渡す
		void input_action_data(const action_datas& data);

		//	移動の状態を本体とモデルへ反映
		//	@remainder	通信でホストの状態を受け取ったときもここを通す
		void apply_movement_state();

	private:	//	シリアライズ
		template<class Archive>
		void serialize_fields(Archive& archive, std::uint32_t version);

	private:	//	設定
		std::shared_ptr<model_object> model;				//	モデル
		std::shared_ptr<collision_shape_object> collision;	//	当たり判定
		roomba_movement movement;							//	ルンバムーブメント

		roomba_movement_environment movement_environment;	//	環境情報(ここはステージの位置関係で良く変わる可能性が高い)
		roomba_movement_state movement_state;				//	ムーブステート
		roomba_input_command input_command;					//	入力コマンド
		std::uint32_t next_tick = 0;						//	ネットワーク用処理順
	};
}	//	namespace minigame000
