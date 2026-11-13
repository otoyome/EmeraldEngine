#pragma once

#include "Graphics/Lighting/light_data.h"
#include "Graphics/Pipeline/common_pipeline_states.h"
#include "Texture/texture.h"
#include "Graphics/Core/color_space.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

//	ライト情報
class light
{
public:
	enum class flags
	{
		active,
		use_shadow,
		contact_shadow,

		debug_csm_area_flag = 20,
		debug_visualize_ailiasing_error_flag = 21,
	};

	//	強さの単位
	enum class unit_modes : std::uint8_t
	{
		lumen,		//	光源から放射される光束[lm]
		candela,	//	ある方向への光度[cd]
		lux,		//	面が受ける照度[lx]
	};

	//	色の指定方法
	enum class color_modes : std::uint8_t
	{
		color,					//	色を直接指定
		filter_and_temperature,	//	色温度xフィルター
	};

	struct	shadow_map_bias_data
	{
		float depth_bias = 0.5f;
		float slope_depth_bias_scale = 0.5f;

		float max_depth_bias = 0.1f;
		float slope_depth_bias = 3.0f;
		float max_slope_depth_bias = 1.0f;
		float sigma_factor = 5.0f;
	};

public:
	light(light_type type);
	~light();

	UINT get_attribute() const { return system_flags; }
	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value){ system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); }

	light_type get_light_type() const;

	const math::vector3d& get_position() const;
	void set_position(const math::vector3d& position);
	const math::rotatorf& get_rotation() const;
	void set_rotation(const math::rotatorf& rotation);

	//	強さ
	unit_modes get_unit_mode() const { return unit_mode; }
	void set_unit_mode(unit_modes mode);
	float get_intensity() const { return intensity; }
	void set_intensity(float intensity) { this->intensity = intensity; }

	//	色
	color_modes get_color_mode() const { return color_mode; }
	void set_color_mode(color_modes mode) { color_mode = mode; }
	const math::float3& get_color() const { return color; }
	void set_color(const math::float3& color) { this->color = color; }
	int get_kelvin() const { return kelvin; }
	void set_kelvin(int kelvin) { this->kelvin = kelvin; }

	//	放射量の色
	math::float3 calculate_radiometric_color(color_space::gamuts rendering_gamut) const;

	float get_range() const;
	void set_range(float range);
	float get_inner_cone() const;
	void set_inner_cone(float inner_cone);
	float get_outer_cone() const;
	void set_outer_cone(float outer_cone);

	//	影関係
	const math::float3& get_shadow_attenuation() const;
	void set_shadow_attenuation(const math::float3& attenuation);
	float get_csm_distance() const;
	void set_csm_distance(float distance);
	float get_pssm_split_weight() const;
	void set_pssm_split_weight(float weight);
	const shadow_map_bias_data& get_shadow_bias(int index) const;
	void set_shadow_bias(int index, const shadow_map_bias_data& bias_data);

	//	LOD
	float get_max_draw_distance() const { return max_draw_distance; }
	void set_max_draw_distance(float distance) { max_draw_distance = distance; }

	//	ライト管理で使う識別番号
	UINT get_id() const { return id; }
	UINT get_bounds_version() const { return bounds_version; }

private:
	void notify_bounds_changed();

private:
	UINT system_flags;
	UINT id = 0;
	UINT bounds_version = 0;
	float max_draw_distance = 0.0f;

	inline static std::atomic<UINT> id_counter{ 1 };

	//	ライトパラメーター
	light_type type;
	math::vector3d position;
	math::rotatorf rotation;
	unit_modes unit_mode = unit_modes::lux;
	float intensity = 0.0f;
	color_modes color_mode = color_modes::filter_and_temperature;
	math::float3 color{ 1.0f, 1.0f, 1.0f };
	int kelvin = 6000;
	math::float3 parameter;

	//	影関係
	float csm_distance; // 平行光源専用
	float pssm_split_weight; // 平行光源専用
	math::float3 shadow_attenuation;
	shadow_map_bias_data bias_data[common_pipeline_states::shadow_map_max];
};
ENUM_ATTR_BITFLAG(light::flags);

//	ライトプローブ情報
class light_probe
{
public:
	enum class flags
	{
		active,	//	アクティブ状態かどうか
		unbind,	//	範囲制限をかけるかどうか(かけない場合は全画面)
	};

public:
	light_probe();
	~light_probe();

	UINT get_attribute() const { return system_flags; }
	bool is_flag(flags flag) const { return system_utility::is_bitflag(system_flags, 1 << static_cast<UINT>(flag)); }
	void set_flag(flags flag, bool value){ system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value); }
	texture_ptr get_diffuse_ibl_texture() const;
	texture_ptr get_specular_ibl_texture() const;
	texture_ptr get_lut_ggx_texture() const;
	void set_diffuse_ibl_texture(texture_ptr texture);
	void set_specular_ibl_texture(texture_ptr texture);
	void set_lut_ggx_texture(texture_ptr texture);

	const math::vector3d& get_position() const;
	void set_position(const math::vector3d& position);
	math::float3 get_inner_range() const;
	void set_inner_range(const math::float3& inner_range);
	math::float3 get_outer_range() const;
	void set_outer_range(const math::float3& outer_range);

	//	色(Rec.2020 リニア)と強さ(IBL の値 1.0 に対応する輝度 [nit])
	const math::float3& get_color() const { return color; }
	void set_color(const math::float3& color) { this->color = color; }
	float get_intensity() const { return intensity; }
	void set_intensity(float intensity) { this->intensity = intensity; }

	//	シェーダーへ渡す IBL の倍率(描画色域の放射量)
	math::float3 calculate_radiometric_color(color_space::gamuts rendering_gamut) const;

private:
	UINT system_flags;
	texture_ptr diffuse_texture;
	texture_ptr specular_texture;
	texture_ptr lut_ggx_texture;

	math::vector3d position;
	math::float3 inner_range;
	math::float3 outer_range;
	math::float3 color{ 1.0f, 1.0f, 1.0f };
	float intensity = 5000.0f;
};
ENUM_ATTR_BITFLAG(light_probe::flags);

using light_ptr = std::shared_ptr<light>;
using light_probe_ptr = std::shared_ptr<light_probe>;

struct light_container;
namespace collision { class frustum_culling; }

//	ライト収集時の視点情報
struct light_view_info
{
	math::vector3d position;								//	視点のワールド座標
	float tan_half_fov_y = 1.0f;							//	tan(視野角 / 2)
	const collision::frustum_culling* frustum = nullptr;	//	描画原点からの相対座標の視錐台
};

//	ライトコレクター
class light_collector
{
	//	統計情報
	struct statistics
	{
		UINT total_lights = 0;		//	登録されているライト数
		UINT candidate_lights = 0;	//	空間分割から候補になったライト数
		UINT visible_lights = 0;	//	LOD とカリング後に描画するライト数
		UINT occupied_cells = 0;	//	ライトが入っているセル数
	};

	//	空間分割のセル
	struct cell_key
	{
		std::int64_t x, y, z;
		bool operator==(const cell_key& other) const { return x == other.x && y == other.y && z == other.z; }
	};

	struct cell_key_hash
	{
		size_t operator()(const cell_key& key) const;
	};

	//	登録ライト
	struct light_record
	{
		const light* pointer = nullptr;
		cell_key cell_min = { 0, 0, 0 };
		cell_key cell_max = { 0, 0, -1 };
		bool large = false;
		UINT bounds_version = 0;
		UINT seen_stamp = 0;
		UINT visit_stamp = 0;
	};

public:
	void collect(light_container& container, const std::vector<light_ptr>& lights, const std::vector<light_probe_ptr>& light_probes, const math::vector3d& render_origin, const light_view_info& view);

	const statistics& get_statistics() const { return last_statistics; }

private:
	void update_records(const std::vector<light_ptr>& lights);
	void insert_record(UINT id, light_record& record);
	void remove_record(UINT id, light_record& record);
	cell_key to_cell(const math::vector3d& position) const;

private:
	std::unordered_map<UINT, light_record> records;
	std::unordered_map<cell_key, std::vector<UINT>, cell_key_hash> cells;
	std::vector<UINT> large_lights;
	std::vector<UINT> directional_lights;
	UINT seen_stamp = 0;
	UINT visit_stamp = 0;
	double cell_size = 0.0;

	std::vector<UINT> candidate_ids;
	statistics last_statistics;
};
