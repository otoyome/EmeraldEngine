#pragma once

#include "object/collision/collision_object.h"
#include "object/model_object.h"
#include "Object/collision_shape_object.h"

class game_object_serializer;

namespace minigame000
{
	//	入力コマンド
	//	通信ではこれをホストに送る
	struct roomba_input_command
	{
		std::uint32_t tick = 0;
		math::float3 move = { 0, 0, 0 };	// 移動方向
		bool dash = false;					//	ダッシュ
		bool brake = false;					//	ブレーキ
	};

	//	移動情報
	//	通信ではこれをホストが配る
	struct roomba_movement_state
	{
		std::uint32_t tick = 0;								//	最後に処理したinput_command
		math::vector3d position = { 0.0, 0.0, 0.0 };		//	位置
		math::rotatorf rotation;							//	回転
		math::float3 velocity = { 0.0f, 0.0f, 0.0f };		//	速度
		bool on_ground = true;								//	接地しているか
		math::float3 ground_normal = { 0.0f, 1.0f, 0.0f };	//	地面の法線
		float ground_friction = 1.0f;						//	地面の摩擦係数
		float velocity_tilt_pitch = 0.0f;					//	速度ピッチ
		float velocity_tilt_roll = 0.0f;					//	速度ロール
		float ground_tilt_pitch = 0.0f;						//	地形ピッチ
		float ground_tilt_roll = 0.0f;						//	地形ロール
	};

	//	セッティング
	struct roomba_movement_settings
	{
		float mass = system_units::convert_kg_to_unit(4.0f);					//	重さ
		float diameter = system_units::convert_m_to_unit(0.34f);				//	直径（空気抵抗用）
		float height = system_units::convert_m_to_unit(0.092f);					//	高さ（空気抵抗用）
		float drag_coefficient = 0.45f;											//	抗力係数

		float walk_accelaration = system_units::convert_m_to_unit(14.0f);		//	歩きの加速度
		float walk_max_speed = system_units::convert_m_to_unit(0.75f);			//	歩きの最高速度
		float dash_accelaration = system_units::convert_m_to_unit(18.0f);		//	ダッシュの加速度
		float dash_max_speed = system_units::convert_m_to_unit(10.0f);			//	ダッシュの最高速度	@remainder	仕組みとして使っていないのでコレは定数で良いと思う
		float braking_deceleration = system_units::convert_m_to_unit(15.0f);	//	ブレーキの減速度
		float base_friction = 2.5f;												//	摩擦係数

		float rotation_speed = math::angle::to_radian(1440.0f);					//	旋回速度
	};

	//	環境情報h
	struct roomba_movement_environment
	{
		math::float3 world_gravity = math::float3(0.0f, system_units::convert_m_to_unit(-9.8f), 0.0f);	//	ワールド重力(m/s2)
		float world_air_density = 1.225f / 1000.0f;														//	空気密度(kg/m^3)
	};

	//	ルンバの移動計算
	class roomba_movement
	{
	public:
		//	移動段階
		enum class move_modes
		{
			walk,	//	普通ルンバ
			dash,	//	爆速ルンバ
		};

		//	形状情報(カプセル前提)
		struct shape_descs
		{
			float radius = 0.0f;						//	半径
			float length = 0.0f;						//	長さ
			math::float3 offset = { 0.0f, 0.0f, 0.0f };	//	本体からカプセル中心までの位置
			collision_filter_ptr filter;				//	コリジョンフィルター
			collision_object_ptr exclude;				//	自分の当たり判定
		};

		struct step_datas
		{
			roomba_input_command command;						//	今の入力
			math::float2 move_vector = { 0.0f, 0.0f };			//	移動方向
			math::float2 turn_front_vector  = { 0.0f, 0.0f };	//	向く方向
		};

	public:
		roomba_movement_settings& get_settings() { return settings; }
		const roomba_movement_settings& get_settings() const { return settings; }

		float get_max_speed(move_modes mode) const;
		float get_accelaration_force(move_modes mode) const;

	public:
		//	シミュレート
		void simulate(roomba_movement_state& state, const roomba_input_command& command, float elapsed_time, const roomba_movement_environment& environment, collision_world& world, const shape_descs& shape) const;

	private:	//	速度系
		void add_force(roomba_movement_state& state, math::float3 force) const;

		float get_current_accelaration_force(const step_datas& step) const;
		float get_current_braking_force() const;
		float get_current_friction() const;

		float calculate_drag_coefficient() const;
		float calculate_frontal_area(const roomba_movement_state& state) const;

	private:	//	姿勢制御
		void update_rotation(roomba_movement_state& state, const step_datas& step, float elapsed_time) const;
		void update_velocity(roomba_movement_state& state, const step_datas& step, const roomba_movement_environment& environment, float elapsed_time) const;
		void update_move(roomba_movement_state& state, float elapsed_time, collision_world& world, const shape_descs& shape) const;
		void update_tilt(roomba_movement_state& state, const step_datas& step, float elapsed_time) const;

		bool calculate_position(const roomba_movement_state& state, math::float3 velocity, float elapsed_time, collision_world& world, const shape_descs& shape, math::vector3d& out_position, math::float3& out_normal, float* out_friction) const;

	private:
		roomba_movement_settings settings;	//	移動設定
		float gravity_scale = 1.0f;			//	重力スケール
	};
}	//	namespace minigame000
