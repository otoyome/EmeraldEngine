#pragma once

#include "object/collision/collision_object.h"
#include "object/model_object.h"
#include "Object/collision_shape_object.h"

class game_object_serializer;

namespace minigame000
{
	//	ルンバ本体
	class roomba_object : public spatial_object
	{
		GAME_OBJECT_DECRLEAR(roomba_object, spatial_object)

		friend class ::game_object_serializer;
		static constexpr std::uint32_t serialize_version = 2;

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

		math::rotatorf get_rotation() const;
		void set_rotation(math::rotatorf rotator);

		math::vector3d get_position() const;
		void set_position(const math::vector3d& position);

		math::float3 get_velocity() const { return velocity; }
		void set_velocity(const math::float3& value) { velocity = value; }

		bool is_on_ground() const { return on_ground; }

		float get_max_speed(move_modes mode) const;
		const std::shared_ptr<model_object>& get_model_object() const { return model; }
		const std::shared_ptr<collision_shape_object>& get_capsule() const { return capsule; }

	private:
		//	姿勢・速度・移動
		void update_rotation(float elapsed_time);
		void update_velocity(float elapsed_time);
		void update_move(float elapsed_time);
		void update_tilt(float elapsed_time);

		bool calculate_position(const math::float3& move_velocity, float elapsed_time, math::vector3d& out_position, math::float3& out_normal, float& out_friction);
		float calculate_frontal_area() const;
		float get_accelaration_force(move_modes mode) const;

	private:	//	シリアライズ
		template<class Archive>
		void serialize_fields(Archive& archive, std::uint32_t version);

	private:	//	設定
		std::shared_ptr<model_object> model;	//	見た目のモデル
		std::shared_ptr<collision_shape_object> capsule;	//	当たり判定のカプセル

		float mass;					//	重さ
		float diameter;				//	直径（空気抵抗用）
		float height;				//	高さ（空気抵抗用）
		float drag_coefficient;		//	抗力係数

		float walk_accelaration;	//	歩きの加速度
		float walk_max_speed;		//	歩きの最高速度
		float dash_accelaration;	//	ダッシュの加速度
		float dash_max_speed;		//	ダッシュの最高速度（上限としては使わず、傾きなどの基準）
		float braking_deceleration;	//	ブレーキの減速度
		float base_friction;		//	摩擦係数

		float rotation_speed;		//	旋回速度（ラジアン/秒）
		float gravity_scale;		//	重力スケール

	private:	//	実行時の状態
		action_datas action_data;			//	今の入力
		math::float2 move_vector;			//	移動方向
		math::float2 turn_vector;			//	向く方向
		math::float3 velocity;				//	速度
		bool on_ground;						//	接地しているか
		math::float3 ground_normal;			//	地面の法線
		float ground_friction;				//	地面の摩擦係数

		float velocity_tilt_pitch;			//	速度による傾き
		float velocity_tilt_roll;
		float ground_tilt_pitch;			//	地面による傾き
		float ground_tilt_roll;
	};
}	//	namespace minigame000
