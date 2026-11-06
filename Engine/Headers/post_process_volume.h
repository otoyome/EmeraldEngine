#pragma once

#include "texture.h"
#include "Graphics/exposure_controller.h"

class material_resource;
class material_instance;

//	ブルーム用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct bloom_data
{
	enum class modes
	{
		standard,
		convolution,
	};

private:
	bool use = true;

public:
	float threshold = 1.2f;
	float intensity = 0.1f;
	modes mode = modes::convolution;

	//	gaussian
	float deviation = 50.0f;

	//	convolution
	texture_ptr image_kernel;

public:
	bloom_data();

	bool is_active() const;
	void set_active(bool flag);
};

//	レンズゴースト用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct lens_ghosts_data
{
private:
	bool use = true;

public:
	float threshold = 1.2f;
	float intensity = 0.1f;
	float dispersion_scale = 1.5f;
	float dispersion_offset = 0;

	float ghost_mask_radius = 320;
	float ghost_mask_fade = 0.2f;
	int iteration = 4;
	float ghost_bokeh = 2.0f;

	texture_ptr image_kernel;
	texture_ptr ghost_color_lut;

public:
	lens_ghosts_data();

	bool is_active() const;
	void set_active(bool flag);
};

//	被写界深度用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct depth_of_field_data
{
private:
	bool use = true;

public:
	float focal_length = 30.0f;
	float focus_distance = 1.0f;
	float circle_of_confusion = 0.03f;
	float apeture = 1.5f;

	bool display_dof_area = false;
	int blur_size = 57;
	math::float2 adjust_length = { 0.0f, 0.0f };

	int precision_scale_factor = 26; //	精度調整係数
									 //	被写界深度で色がおかしくなっている時はこれを調整してみてください。

public:
	float calculate_f_number() const;
	bool is_active() const;
	void set_active(bool flag);
};

//	スクリーン空間反射用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct screen_space_reflection_data
{
private:
	bool use = true;

public:
	float ray_max_distance = 1000.0f;
	float ray_resolution = 0.5f;
	math::float2 ray_tickness = {0.5f, 100.0f};

	int ray_substep_iteration_max = 15;
	int ray_flags = 0;

	bool accumalate_reset = true;
	float velocity_reset_threshold = 1.0f;

public:
	screen_space_reflection_data();
	bool is_flag(UINT ssr_flag) const;
	void set_flag(UINT ssr_flag, bool value);
	bool is_active() const;
	void set_active(bool flag);
};

//	モーションブラー用情報
//	ﾃｷﾄｰ('ω')ｰﾄｷﾃ
struct motion_blur_data
{
private:
	bool use = true;

public:
	int iteration = 10;
	float jitter_noise_value = 0.5f;
	float velocity_multiplyer = 10.0f;

public:
	bool is_active() const;
	void set_active(bool flag);
};

//	ポストプロセスボリューム情報
class post_process_volume
{
public:
	enum class flags
	{
		active,		//	アクティブ状態かどうか
		unbind,		//	範囲制限をかけるかどうか(かけない場合は全画面)
		debug_draw,	//	デバッグ描画
	};

public:
	post_process_volume();
	~post_process_volume();

	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value){ system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); }

	const math::vector3d& get_position() const;
	void set_position(const math::vector3d& position);
	math::float3 get_inner_range() const;
	void set_inner_range(const math::float3& inner_range);
	math::float3 get_outer_range() const;
	void set_outer_range(const math::float3& outer_range);
	const math::float4& get_color() const;
	void set_color(const math::float4& color);

	const bloom_data& get_bloom_constant() const;
	void set_bloom_constant(const bloom_data& data);

	const lens_ghosts_data& get_lens_ghosts_constant() const;
	void set_lens_ghosts_constant(const lens_ghosts_data& data);

	const depth_of_field_data& get_depth_of_field_constant() const;
	void set_depth_of_field_constant(const depth_of_field_data& data);

	const screen_space_reflection_data& get_screen_space_reflection_constant() const;
	void set_screen_space_reflection_constant(const screen_space_reflection_data& data);

	const motion_blur_data& get_motion_blur_constant() const;
	void set_motion_blur_constant(const motion_blur_data& data);

	const exposure_settings& get_exposure_settings() const;
	void set_exposure_settings(const exposure_settings& data);

	const std::vector<std::shared_ptr<material_instance>>& get_materials() const;
	bool register_postprocess_materials(std::shared_ptr<material_resource> material_ptr);

private:
	UINT system_flags;
	bloom_data bloom_constant;
	lens_ghosts_data lens_ghosts_constant;
	depth_of_field_data depth_of_field_constant;
	screen_space_reflection_data screen_space_reflection_constant;
	motion_blur_data motion_blur_constant;
	exposure_settings exposure_constant;
	std::vector<std::shared_ptr<material_instance>> postprocess_materials;

	math::vector3d position;
	math::float3 inner_range;
	math::float3 outer_range;
	math::float4 color;
};
ENUM_ATTR_BITFLAG(post_process_volume::flags);

using post_process_volume_ptr = std::shared_ptr<post_process_volume>;

//	ポストプロセスボリュームコンテナ
struct post_process_volume_container
{
	std::vector<post_process_volume_ptr> volumes;
	post_process_volume_ptr selected_volume;
};

//	ポストプロセスボリューム管理クラス
class post_process_volume_manager
{
	SINGLETON(post_process_volume_manager);

private:
	post_process_volume_manager();
	~post_process_volume_manager();

public:
	bool generate_post_process_volume(post_process_volume_ptr& out_post_process_volume);
	void get_container(post_process_volume_container& container, const math::vector3d& view_position);

private:
	std::vector<std::weak_ptr<post_process_volume>> post_process_volumes;
};
