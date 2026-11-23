#pragma once

#include "object/game_object.h"
#include "Core/signals.h"
#include "Input/input_action.h"

#include "minigame000_roomba_input_source.h"

class camera_object;

namespace minigame000
{
	//	プレイヤー操作のコントローラー
	class roomba_player_controller_object : public game_object, public roomba_input_source
	{
		GAME_OBJECT_DECRLEAR(roomba_player_controller_object, game_object)

	public:
		//	基準カメラ設定
		void set_camera(const std::shared_ptr<camera_object>& value) { camera = value; }
		std::shared_ptr<camera_object> get_camera() const { return camera.lock(); }

		//	入力許可フラグ設定
		bool get_input_purmit_flag() const { return input_purmit_flag; }
		void set_input_purmit_flag(bool value) { input_purmit_flag = value; }

	public:
		roomba_player_controller_object(std::string_view name);
		~roomba_player_controller_object() override;

		void on_initialize() override;
		void on_finalize() override;

	#if defined(USE_IMGUI)
		void imgui_render_core() override;
	#endif	//	defined(USE_IMGUI)

	public:	//	roomba_input_source
		//	入力コマンド生成
		bool make_command(std::uint32_t tick, roomba_input_command& out_command) override;

	private:
		//	移動方向算出
		math::float3 get_move_vector() const;

	private:
		std::weak_ptr<camera_object> camera;	//	基準カメラ
		bool input_purmit_flag;					//	入力許可フラグ

		input::action dash_action;				//	入力アクション
		input::action brake_action;				//	入力アクション
		input::axis_action move_x_action;		//	入力アクション
		input::axis_action move_y_action;		//	入力アクション

		bool dash_pressed;
		bool brake_pressed;
		float move_x;
		float move_y;
		core::signals::connection_list connections;	//	購読リスト
	};
}	//	namespace minigame000