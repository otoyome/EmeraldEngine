#pragma once

#include "object/game_object.h"

class camera_object;

namespace minigame000
{
	class roomba_object;

	//	ルンバ追従カメラ
	class roomba_camera_controller_object : public game_object
	{
		GAME_OBJECT_DECRLEAR(roomba_camera_controller_object, game_object)

	public:
		//	ステート
		enum class states
		{
			entry,
			free,
			clear_performance,
		};

	public:
		//	動かすカメラ設定/取得
		void set_camera(const std::shared_ptr<camera_object>& value) { camera = value; }
		const std::shared_ptr<camera_object>& get_camera_object() const { return camera; }

		//	追従対象設定(注視点を対象の位置に合わせる)
		void set_target(const std::shared_ptr<roomba_object>& value);
		const std::shared_ptr<roomba_object>& get_target() const { return target; }

		//	入力許可フラグ設定/取得
		void set_input_purmit_flag(bool flag) { input_purmit_flag = flag; }
		bool get_input_purmit_flag() const { return input_purmit_flag; }

	public:
		roomba_camera_controller_object(std::string_view name);
		~roomba_camera_controller_object() override;

		void fixed_update(float elapsed_time) override;
		void update(float elapsed_time) override;

		void on_finalize() override;

	#if defined(USE_IMGUI)
		void imgui_render_core() override;
	#endif	//	defined(USE_IMGUI)

		void change_state(states state);

	private:
		void fixed_update_entry(float elapsed_time);
		void fixed_update_default(float elapsed_time);

	private:
		std::shared_ptr<roomba_object> target;		//	追従対象
		std::shared_ptr<camera_object> camera;		//	動かすカメラ

		states state;								//	モード
		bool input_purmit_flag;						//	入力許可フラグ

		float minimum_distance;						//	最低距離
		float current_distance;						//	現在距離
		float maximum_distance;						//	最大距離

		math::rotatord disengage_direction;			//	カメラを離す方向
		float update_disengage_direction_rate;		//	方向適応割合

		math::vector3d focus;						//	注視点座標
		float focus_distance;						//	注視点距離
		float focus_limit_range;					//	注視点移動制限範囲
		float update_focus_rate_inner;				//	注視点更新係数

		float update_fov_rate_inner;				//	視野角更新係数
	};
}	//	namespace minigame000