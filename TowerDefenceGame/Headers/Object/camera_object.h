#pragma once

#include "Graphics/camera.h"
#include "object/game_object.h"
#include "object/spatial_object.h"

//	カメラ制御用オブジェクト
class camera_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(camera_object, spatial_object)

public:
	camera_ptr get_camera() const {return camera;}

	//	シーンカメラオーダー
	std::uint64_t get_view_order() const { return view_order; }

public:
	camera_object(std::string_view name);
	~camera_object() override;

	void post_update(float elapsed_time) override;

	//	描画に使うカメラの優先順を一番にする
	void bring_to_front() { view_order = view_order_counter.fetch_add(1); }

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

public:
	//	シリアライズ
	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

protected:
	void on_active() override;
	void on_deactive() override;

private:
	camera_ptr camera;
	std::uint64_t view_order = 0;

	inline static std::atomic<std::uint64_t> view_order_counter{ 1 };
};
