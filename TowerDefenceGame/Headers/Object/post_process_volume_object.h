#pragma once

#include "object/game_object.h"
#include "object/spatial_object.h"
#include "Graphics/View/post_process_volume.h"

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"
#endif	//	defined(USE_IMGUI)

//	ポストプロセスボリューム制御用オブジェクト
//	真面目には対応していない
class post_process_volume_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(post_process_volume_object, spatial_object)

public:
	static constexpr std::uint32_t serialize_version = 1;

public:
	post_process_volume_ptr get_volume() const {return instance;}

	bool is_flag(post_process_volume::flags flag) const;
	void set_flag(post_process_volume::flags flag, bool value);

	//	@tool 箱はワールドのトランスフォームで決まる（拡大縮小が箱の大きさ）
	const math::float4& get_color() const;
	void set_color(const math::float4& color);

public:
	post_process_volume_object(std::string_view name);
	~post_process_volume_object() override;

	void post_update(float elapsed_time) override;
	void post_render() override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;

	void imgui_bloom();
	void imgui_lens_ghost();
	void imgui_depth_of_field();
	void imgui_screen_space_reflection();
	void imgui_motion_blur();
	void imgui_exposure();
	void imgui_tone_mapping();
#endif	//	defined(USE_IMGUI)

private:
	void on_active() override;
	void on_deactive() override;

	void sync_to_volume();

	friend class game_object_serializer;
	template<class Archive>
	void serialize_fields(Archive& archive, std::uint32_t version);

private:
	post_process_volume_ptr instance;
	std::uint32_t applied_spatial_version = UINT32_MAX;
#if defined(USE_IMGUI)
	std::shared_ptr<editor_items::context> editor_context;
#endif	//	defined(USE_IMGUI)
};
