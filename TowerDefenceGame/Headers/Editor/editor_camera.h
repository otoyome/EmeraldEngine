#pragma once

#include "camera.h"

#if	defined(USE_IMGUI)
//	エディターカメラ
class editor_camera
{
public:
	static constexpr std::string_view left_click_down_identifier = "editor_camera::left_click_down_identifier";
	static constexpr std::string_view left_click_hold_identifier = "editor_camera::left_click_hold_identifier";
	static constexpr std::string_view left_click_release_identifier = "editor_camera::left_click_release_identifier";
	struct mouse_position_data
	{
		math::float2 position;
	};

public:
	inline camera_ptr get_camera() const {return camera;}
	void set_rotation(const math::rotatorf& rotation) {this->rotation = rotation;}
	const math::rotatorf& get_rotation() const {return rotation;}
	void set_position(const math::vector3d& position) { this->position = position; }
	const math::vector3d& get_position() const { return position; }

	void set_rotation_speed(float rotation_speed) { this->rotation_speed = rotation_speed; }
	float get_rotation_speed() const { return rotation_speed; }
	void set_move_speed(float move_speed) { this->move_speed = move_speed; }
	float get_move_speed() const { return move_speed; }

public:
	editor_camera();
	~editor_camera();

	//	入力による操作
	void update(float elapsed_time);

	//	元になる視点を引き継ぎ、姿勢だけ自身のものにする
	void apply_view(const view_parameters* base);

private:
	camera_ptr camera;

	bool is_auto_rotate = false;

	math::rotatorf rotation;
	math::vector3d position;

	float rotation_speed = 0.0f;
	float move_speed = 0.0f;
};

//	エディターカメラの管理
class editor_camera_manager
{
	SINGLETON(editor_camera_manager)

public:
	//	エディターカメラにリクエストを送る中継コマンド
	class scene_request
	{
		friend class editor_camera_manager;

	public:
		//	一時停止
		void set_suspended(bool suspended) { this->suspended = suspended; }
		bool is_suspended() const { return suspended; }

	private:
		bool suspended = false;
	};
	using scene_request_ptr = std::shared_ptr<scene_request>;

private:
	editor_camera_manager();
	~editor_camera_manager();

public:
	//	更新
	void update(float elapsed_time);

	//	エディターカメラでオーバーライド
	scene_request_ptr request(const math::vector3d& position, const math::rotatorf& rotation, float speed_scale = 1.0f);

	//	エディターカメラで描画しているか？
	bool is_enabled() const { return enabled; }

	//	設定フラグ
	bool is_user_enabled() const;
	void set_user_enabled(bool enabled);

	const std::shared_ptr<editor_camera>& get_camera() const { return camera; }

private:
	bool compute_enabled();
	void begin_camera();
	void end_camera();

private:
	std::shared_ptr<editor_camera> camera;
	std::vector<std::weak_ptr<scene_request>> requests;
	bool enabled = false;

	//	次に有効になったときに使う初期姿勢
	bool has_initial_pose = false;
	math::vector3d initial_position;
	math::rotatorf initial_rotation;
};

#endif	//	defined(USE_IMGUI)
