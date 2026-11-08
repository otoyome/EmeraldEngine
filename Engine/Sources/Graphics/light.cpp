#include "Graphics/light.h"
#include "Texture/texture.h"
#include "Graphics/graphics_context.h"
#include "collision/frustum_culling.h"
#include "System/console_manager.h"
#include "System/system_units.h"
#include "System/engine_config_data.h"
#include "Graphics/light_utility.h"

#include <algorithm>
#include <cmath>

namespace
{
	//	LOD と空間分割の設定
	console_variable<float> max_draw_distance_variable("r.light.max_draw_distance", system_units::convert_m_to_unit(500.0f), "点光源/スポットライトを描画する最大距離（0 以下で無制限。ライトごとの設定はこれ以下に制限される）", console_flags::user_setting);
	console_variable<float> fade_ratio_variable("r.light.fade_ratio", 0.1f, "最大距離・最小サイズに近づいたときにフェードアウトさせる割合", console_flags::user_setting);
	console_variable<float> min_screen_size_variable("r.light.min_screen_size", 0.002f, "画面上の大きさ（半径 / 画面の半分の高さ）がこれより小さいライトは描画しない", console_flags::user_setting);
	console_variable<float> grid_cell_size_variable("r.light.grid_cell_size", system_units::convert_m_to_unit(32.0f), "ライト用の空間分割のセルの大きさ", console_flags::user_setting);
	float get_max_draw_distance() { return max_draw_distance_variable.get(); }
	float get_fade_ratio() { return (std::max)(fade_ratio_variable.get(), 1.0e-4f); }
	float get_min_screen_size() { return (std::max)(min_screen_size_variable.get(), 0.0f); }
	double get_grid_cell_size() { return (std::max)(static_cast<double>(grid_cell_size_variable.get()), 1.0e-3); }

	//	これより多くのセルを跨ぐライトは空間分割に入れず常に候補にする
	constexpr std::int64_t large_light_cell_count = 512;

	//	シェーダー用のライト情報に変換
	light_data convert_light_data(const light_render_data& light)
	{
		light_data data{};
		data.color = light.color;
		data.attribute = light_data_utility::make_light_attribute(light.attribute, light.type);
		switch(light.type)
		{
			case light_type::directional:
			{
				data.direction = light.direction;
				break;
			}
			case light_type::point:
			{
				data.position = light.position;
				data.inv_range = light_data_utility::convert_inv_range(light.range);
				break;
			}
			case light_type::spot:
			{
				data.position = light.position;
				data.inv_range = light_data_utility::convert_inv_range(light.range);
				data.direction = math::vector3f(light.direction).normalized().to_float3();
				data.cone = light_data_utility::encode_spot_cone(light.inner_cone, light.outer_cone);
				break;
			}
		}
		return data;
	}

	//	ライトの描画情報を描画原点からの相対座標で作る
	light_render_data make_light_render_data(const light& source, const math::vector3d& render_origin, float fade = 1.0f)
	{
		light_render_data ret;
		ret.type = source.get_light_type();
		ret.attribute = source.get_attribute();
		ret.id = source.get_id();
		ret.position = (source.get_position() - render_origin).to_float3();
		ret.rotation = source.get_rotation();
		ret.direction = source.get_rotation().front().to_float3();
		const math::float3 radiometric_color = source.calculate_radiometric_color(engine_config_data::instance()->get_active_rendering_gamut());
		ret.color = { radiometric_color.x * fade, radiometric_color.y * fade, radiometric_color.z * fade };
		ret.range = source.get_range();
		ret.inner_cone = source.get_inner_cone();
		ret.outer_cone = source.get_outer_cone();
		ret.shadow_attenuation = source.get_shadow_attenuation();
		ret.csm_distance = source.get_csm_distance();
		ret.pssm_split_weight = source.get_pssm_split_weight();
		for (int index = 0; index < static_cast<int>(common_pipeline_states::shadow_map_max); ++index)
			ret.bias_data[index] = source.get_shadow_bias(index);
		ret.data = convert_light_data(ret);
		return ret;
	}

	//	ライトプローブの描画情報を描画原点からの相対座標で作る
	light_probe_render_data make_light_probe_render_data(const light_probe& source, const math::vector3d& render_origin)
	{
		light_probe_render_data ret;
		ret.attribute = source.get_attribute();

		const math::float3 position = (source.get_position() - render_origin).to_float3();
		const math::float3 inner_range = source.get_inner_range();
		const math::float3 outer_range = source.get_outer_range();
		ret.data.position.x = position.x;
		ret.data.position.y = position.y;
		ret.data.position.z = position.z;
		ret.data.inner_area.x = inner_range.x;
		ret.data.inner_area.y = inner_range.y;
		ret.data.inner_area.z = inner_range.z;
		ret.data.outer_area.x = outer_range.x;
		ret.data.outer_area.y = outer_range.y;
		ret.data.outer_area.z = outer_range.z;
		const math::float3 radiometric_color = source.calculate_radiometric_color(engine_config_data::instance()->get_active_rendering_gamut());
		ret.data.color = { radiometric_color.x, radiometric_color.y, radiometric_color.z, 1.0f };

		ret.diffuse_ibl_texture = source.get_diffuse_ibl_texture();
		ret.specular_ibl_texture = source.get_specular_ibl_texture();
		ret.lut_ggx_texture = source.get_lut_ggx_texture();
		return ret;
	}
}

light::light(light_type type)
: system_flags(0)
, id(id_counter.fetch_add(1))
, type(type)
, position(math::vector3d::zero())
, rotation(0, 0, 0)
, parameter(10.0f, math::angle::to_radian(30.0f), math::angle::to_radian(45.0f))
{
	//	初期値は平行光源が晴天の太陽(lx)、点光源/スポットが 60W 電球相当(lm)
	if (type == light_type::directional)
	{
		unit_mode = unit_modes::lux;
		intensity = 100000.0f;
	}
	else
	{
		unit_mode = unit_modes::lumen;
		intensity = 800.0f;
	}
	set_flag(flags::active, true);
	set_flag(flags::use_shadow, true);

	csm_distance = 5000.0f;
	pssm_split_weight = 0.8f;
	shadow_attenuation = {0.2f, 0.2f, 0.2f};

	switch(type)
	{
		case light_type::directional:
		{
			bias_data[0].max_depth_bias = 0.01f;
			bias_data[0].sigma_factor = 0.5f;
			bias_data[1].max_depth_bias = 0.015f;
			bias_data[1].sigma_factor = 1.25f;
			bias_data[2].max_depth_bias = 0.018f;
			bias_data[2].sigma_factor = 20.0f;
			bias_data[3].max_depth_bias = 0.1f;
			bias_data[3].sigma_factor = 20.0f;
			bias_data[4].max_depth_bias = 0.12f;
			bias_data[4].sigma_factor = 20.0f;
			bias_data[5].max_depth_bias = 0.15f;
			bias_data[5].sigma_factor = 20.0f;
			break;
		}
		case light_type::point:
		{
			bias_data[0].max_depth_bias = 0.01f;
			bias_data[1].max_depth_bias = 0.01f;
			bias_data[2].max_depth_bias = 0.01f;
			bias_data[3].max_depth_bias = 0.01f;
			bias_data[4].max_depth_bias = 0.01f;
			bias_data[5].max_depth_bias = 0.01f;
			break;
		}
		case light_type::spot:
		{
			bias_data[0].max_depth_bias = 0.01f;
			bias_data[1].max_depth_bias = 0.01f;
			bias_data[2].max_depth_bias = 0.01f;
			bias_data[3].max_depth_bias = 0.01f;
			bias_data[4].max_depth_bias = 0.01f;
			bias_data[5].max_depth_bias = 0.01f;
			break;
		}
	}
}

light::~light()
{
}

void light::notify_bounds_changed()
{
	++bounds_version;
}

light_type light::get_light_type() const
{
	return type;
}

const math::vector3d& light::get_position() const
{
	return position;
}

void light::set_position(const math::vector3d& position)
{
	this->position = position;
	notify_bounds_changed();
}

const math::rotatorf& light::get_rotation() const
{
	return rotation;
}

void light::set_rotation(const math::rotatorf& rotation)
{
	this->rotation = rotation;
}

void light::set_unit_mode(unit_modes mode)
{
	//	平行光源は照度[lx]固定、点光源/スポットは照度では指定できない(距離で変わるため)
	if (type == light_type::directional)
		mode = unit_modes::lux;
	else if (mode == unit_modes::lux)
		return;
	if (mode == unit_mode)
		return;

	const double cos_half_angle = std::cos(get_outer_cone());
	if (unit_mode == unit_modes::lumen && mode == unit_modes::candela)
	{
		intensity = static_cast<float>(type == light_type::spot
			? light_utility::convert_spot_light_lumen_to_candela(intensity, cos_half_angle)
			: light_utility::convert_point_light_lumen_to_candela(intensity));
	}
	else if (unit_mode == unit_modes::candela && mode == unit_modes::lumen)
	{
		intensity = static_cast<float>(type == light_type::spot
			? light_utility::convert_spot_light_candela_to_lumen(intensity, cos_half_angle)
			: light_utility::convert_point_light_candela_to_lumen(intensity));
	}
	unit_mode = mode;
}

math::float3 light::calculate_radiometric_color(color_space::gamuts rendering_gamut) const
{
	//	色温度の色は輝度 1 に正規化済みで、フィルターは光を吸収する(輝度を下げる)ものとして掛ける
	math::vector3f rgb(color.x, color.y, color.z);
	if (color_mode == color_modes::filter_and_temperature)
		rgb = rgb * light_utility::get_temperature_color(kelvin);
	else
		rgb = light_utility::normalize_luminance_rec2020(rgb);

	float work_intensity = intensity;
	if (unit_mode == unit_modes::lumen)
	{
		//	光束[lm]は光度[cd]にする(点光源は 4π sr、スポットは外側コーンの立体角)
		if (type == light_type::spot)	//	スポットライト
			work_intensity = static_cast<float>(light_utility::convert_spot_light_lumen_to_candela(intensity, std::cos(get_outer_cone())));
		else	//	ポイントライト
			work_intensity = static_cast<float>(light_utility::convert_point_light_lumen_to_candela(intensity));
	}

	//	測光量から放射量へ、Rec.2020 から描画色域へ
	const float radiometric_value = static_cast<float>(light_utility::convert_photometric_to_radiometric(work_intensity));
	const math::vector3f result = color_space::convert_gamut(rgb * radiometric_value, color_space::gamuts::rec2020, rendering_gamut);
	return { result[0], result[1], result[2] };
}

float light::get_range() const
{
	return parameter.x;
}

void light::set_range(float range)
{
	this->parameter.x = range;
	notify_bounds_changed();
}

float light::get_inner_cone() const
{
	return parameter.y;
}

void light::set_inner_cone(float inner_cone)
{
	this->parameter.y = inner_cone;
}

float light::get_outer_cone() const
{
	return parameter.z;
}

void light::set_outer_cone(float outer_cone)
{
	this->parameter.z = outer_cone;
}

const math::float3& light::get_shadow_attenuation() const
{
	return shadow_attenuation;
}

void light::set_shadow_attenuation(const math::float3& attenuation)
{
	this->shadow_attenuation = attenuation;
}

float light::get_csm_distance() const
{
	return csm_distance;
}

void light::set_csm_distance(float distance)
{
	this->csm_distance = math::max(distance, 0.0001f);
}

float light::get_pssm_split_weight() const
{
	return pssm_split_weight;
}

void light::set_pssm_split_weight(float weight)
{
	this->pssm_split_weight = weight;
}

const light::shadow_map_bias_data& light::get_shadow_bias(int index) const
{
	return bias_data[math::clamp(index, 0, static_cast<int>(common_pipeline_states::shadow_map_max) - 1)];
}

void light::set_shadow_bias(int index, const shadow_map_bias_data& bias_data)
{
	if (index < 0 || index >= static_cast<int>(common_pipeline_states::shadow_map_max))
		return;
	this->bias_data[index] = bias_data;
}

////////////////////////////////////////////////////////////////////////////////////////////////
light_probe::light_probe()
: system_flags(0)
, diffuse_texture()
, specular_texture()
, lut_ggx_texture()
, position(math::vector3d::zero())
, inner_range(50.0f, 50.0f, 50.0f)
, outer_range(100.0f, 100.0f, 100.0f)
{
	set_flag(flags::active, true);
}

light_probe::~light_probe()
{
}

texture_ptr light_probe::get_diffuse_ibl_texture() const
{
	return diffuse_texture;
}

texture_ptr light_probe::get_specular_ibl_texture() const
{
	return specular_texture;
}

texture_ptr light_probe::get_lut_ggx_texture() const
{
	return lut_ggx_texture;
}

void light_probe::set_diffuse_ibl_texture(texture_ptr texture)
{
	diffuse_texture = texture;
}

void light_probe::set_specular_ibl_texture(texture_ptr texture)
{
	specular_texture = texture;
}

void light_probe::set_lut_ggx_texture(texture_ptr texture)
{
	lut_ggx_texture = texture;
}

const math::vector3d& light_probe::get_position() const
{
	return position;
}

void light_probe::set_position(const math::vector3d& position)
{
	this->position = position;
}

math::float3 light_probe::get_inner_range() const
{
	return inner_range;
}

void light_probe::set_inner_range(const math::float3& inner_range)
{
	this->inner_range = inner_range;
}

math::float3 light_probe::get_outer_range() const
{
	return outer_range;
}

void light_probe::set_outer_range(const math::float3& outer_range)
{
	this->outer_range = outer_range;
}

math::float3 light_probe::calculate_radiometric_color(color_space::gamuts rendering_gamut) const
{
	//	輝度[nit]を放射輝度にし、Rec.2020 から描画色域へ
	const float radiometric_value = static_cast<float>(light_utility::convert_photometric_to_radiometric(intensity));
	const math::vector3f result = color_space::convert_gamut(math::vector3f(color.x, color.y, color.z) * radiometric_value, color_space::gamuts::rec2020, rendering_gamut);
	return { result[0], result[1], result[2] };
}

////////////////////////////////////////////////////////////////////////////////////////////////
size_t light_collector::cell_key_hash::operator()(const cell_key& key) const
{
	size_t hash = std::hash<std::int64_t>()(key.x);
	hash ^= std::hash<std::int64_t>()(key.y) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
	hash ^= std::hash<std::int64_t>()(key.z) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
	return hash;
}

light_collector::cell_key light_collector::to_cell(const math::vector3d& position) const
{
	return
	{
		static_cast<std::int64_t>(std::floor(position.x / cell_size)),
		static_cast<std::int64_t>(std::floor(position.y / cell_size)),
		static_cast<std::int64_t>(std::floor(position.z / cell_size)),
	};
}

void light_collector::insert_record(UINT id, light_record& record)
{
	const light* pointer = record.pointer;
	if (!pointer || pointer->get_light_type() == light_type::directional)
		return;

	//	影響範囲の球が掛かるセルに入れる
	const math::vector3d& position = pointer->get_position();
	const double radius = (std::max)(static_cast<double>(pointer->get_range()), 0.0);
	record.cell_min = to_cell(position - math::vector3d(radius, radius, radius));
	record.cell_max = to_cell(position + math::vector3d(radius, radius, radius));

	const std::int64_t count = (record.cell_max.x - record.cell_min.x + 1) * (record.cell_max.y - record.cell_min.y + 1) * (record.cell_max.z - record.cell_min.z + 1);
	record.large = count > large_light_cell_count;
	if (record.large)
	{
		large_lights.emplace_back(id);
		return;
	}

	for (std::int64_t z = record.cell_min.z; z <= record.cell_max.z; ++z)
		for (std::int64_t y = record.cell_min.y; y <= record.cell_max.y; ++y)
			for (std::int64_t x = record.cell_min.x; x <= record.cell_max.x; ++x)
				cells[{ x, y, z }].emplace_back(id);
}

void light_collector::remove_record(UINT id, light_record& record)
{
	if (record.large)
	{
		large_lights.erase(std::remove(large_lights.begin(), large_lights.end(), id), large_lights.end());
	}
	else
	{
		for (std::int64_t z = record.cell_min.z; z <= record.cell_max.z; ++z)
			for (std::int64_t y = record.cell_min.y; y <= record.cell_max.y; ++y)
				for (std::int64_t x = record.cell_min.x; x <= record.cell_max.x; ++x)
				{
					auto it = cells.find({ x, y, z });
					if (it == cells.end())
						continue;
					auto& ids = it->second;
					auto found = std::find(ids.begin(), ids.end(), id);
					if (found != ids.end())
					{
						*found = ids.back();
						ids.pop_back();
					}
					if (ids.empty())
						cells.erase(it);
				}
	}
	record.large = false;
	record.cell_min = { 0, 0, 0 };
	record.cell_max = { 0, 0, -1 };
}

void light_collector::update_records(const std::vector<light_ptr>& lights)
{
	//	セルの大きさが変わったら全て入れ直す
	const double desired_cell_size = get_grid_cell_size();
	const bool rebuild = desired_cell_size != cell_size;
	if (rebuild)
	{
		for (auto& pair : records)
			remove_record(pair.first, pair.second);
		cell_size = desired_cell_size;
	}

	//	レコードにライトを登録
	++seen_stamp;
	directional_lights.clear();
	for (const auto& source : lights)
	{
		if (!source)
			continue;

		const UINT id = source->get_id();
		auto [it, inserted] = records.try_emplace(id);
		light_record& record = it->second;
		record.pointer = source.get();
		record.seen_stamp = seen_stamp;

		if (source->get_light_type() == light_type::directional)
		{
			directional_lights.emplace_back(id);
			continue;
		}

		if (inserted || rebuild || record.bounds_version != source->get_bounds_version())
		{
			remove_record(id, record);
			insert_record(id, record);
			record.bounds_version = source->get_bounds_version();
		}
	}

	//	レコードからライトを消す
	for (auto it = records.begin(); it != records.end();)
	{
		if (it->second.seen_stamp != seen_stamp)
		{
			remove_record(it->first, it->second);
			it = records.erase(it);
			continue;
		}
		++it;
	}

	//	ソート
	std::sort(directional_lights.begin(), directional_lights.end());
}

void light_collector::collect(light_container& container, const std::vector<light_ptr>& lights, const std::vector<light_probe_ptr>& light_probes, const math::vector3d& render_origin, const light_view_info& view)
{
	update_records(lights);

	const float max_draw_distance = get_max_draw_distance();
	const float fade_ratio = get_fade_ratio();
	const float min_screen_size = get_min_screen_size();
	const bool limited = max_draw_distance > 0.0f;

	//	空間分割から候補を集める
	++visit_stamp;
	candidate_ids.clear();
	auto visit = [&](UINT id)
	{
		auto it = records.find(id);
		if (it == records.end() || it->second.visit_stamp == visit_stamp)
			return;
		it->second.visit_stamp = visit_stamp;
		candidate_ids.emplace_back(id);
	};

	if (!limited)
	{
		for (auto& pair : records)
			visit(pair.first);
	}
	else
	{
		const double radius = static_cast<double>(max_draw_distance);
		const cell_key query_min = to_cell(view.position - math::vector3d(radius, radius, radius));
		const cell_key query_max = to_cell(view.position + math::vector3d(radius, radius, radius));
		const double query_cells = static_cast<double>(query_max.x - query_min.x + 1) * static_cast<double>(query_max.y - query_min.y + 1) * static_cast<double>(query_max.z - query_min.z + 1);

		if (query_cells > static_cast<double>(cells.size()))
		{
			//	調べる範囲より使われているセルの方が少なければ、使われているセルを全て調べる
			for (auto& pair : cells)
			{
				const cell_key& key = pair.first;
				if (key.x < query_min.x || key.x > query_max.x || key.y < query_min.y || key.y > query_max.y || key.z < query_min.z || key.z > query_max.z)
					continue;
				for (UINT id : pair.second)
					visit(id);
			}
		}
		else
		{
			for (std::int64_t z = query_min.z; z <= query_max.z; ++z)
				for (std::int64_t y = query_min.y; y <= query_max.y; ++y)
					for (std::int64_t x = query_min.x; x <= query_max.x; ++x)
					{
						auto it = cells.find({ x, y, z });
						if (it == cells.end())
							continue;
						for (UINT id : it->second)
							visit(id);
					}
		}
		for (UINT id : large_lights)
			visit(id);
	}

	//	並び順をソートしておく
	std::sort(candidate_ids.begin(), candidate_ids.end());

	//	ライト
	container.lights.clear();
	for (UINT id : directional_lights)
	{
		auto it = records.find(id);
		if (it == records.end())
			continue;
		if (const light* pointer = it->second.pointer)
			container.lights.emplace_back(make_light_render_data(*pointer, render_origin));
	}

	UINT candidate_count = 0;
	for (UINT id : candidate_ids)
	{
		auto it = records.find(id);
		if (it == records.end())
			continue;
		const light* pointer = it->second.pointer;
		if (!pointer || pointer->get_light_type() == light_type::directional)
			continue;
		++candidate_count;

		const float range = pointer->get_range();
		const double distance = (pointer->get_position() - view.position).length();
		const float edge_distance = static_cast<float>(distance) - range;
		float fade = 1.0f;

		//	距離による LOD
		float light_max_distance = pointer->get_max_draw_distance();
		if (limited)
			light_max_distance = (light_max_distance > 0.0f) ? (std::min)(light_max_distance, max_draw_distance) : max_draw_distance;
		if (light_max_distance > 0.0f)
		{
			if (edge_distance > light_max_distance)
				continue;
			fade = (std::min)(fade, math::saturate((light_max_distance - edge_distance) / (light_max_distance * fade_ratio)));
		}

		//	画面上の大きさによる LOD
		if (min_screen_size > 0.0f && distance > range)
		{
			const float screen_size = range / (static_cast<float>(distance) * view.tan_half_fov_y);
			if (screen_size < min_screen_size)
				continue;
			fade = (std::min)(fade, math::saturate((screen_size - min_screen_size) / (min_screen_size * fade_ratio * 10.0f)));
		}

		//	視錐台カリング
		if (view.frustum && !view.frustum->to_sphere((pointer->get_position() - render_origin).to_float3(), range))
			continue;

		container.lights.emplace_back(make_light_render_data(*pointer, render_origin, fade));
	}

	//	プローブ
	{
		container.light_probes.clear();
		for (const auto& probe : light_probes)
		{
			if (!probe)
				continue;
			container.light_probes.emplace_back(make_light_probe_render_data(*probe, render_origin));
		}
	}

	//	統計
	last_statistics.total_lights = static_cast<UINT>(records.size());
	last_statistics.candidate_lights = candidate_count;
	last_statistics.visible_lights = static_cast<UINT>(container.lights.size() - (std::min)(container.lights.size(), directional_lights.size()));
	last_statistics.occupied_cells = static_cast<UINT>(cells.size());
}
