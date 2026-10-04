#include <cmath>

#include "animation_resource.h"
#include "graphics_utility.h"
#include "render_data.h"
#include "texture.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"

#include "animation_data_version.h"

namespace
{
	//	サンプリングレート
	constexpr float AnimationSamplingRate = system_units::convert_fps_to_sec(1.0f);

	//	正規化フレームVMD
	constexpr float NormalizedFrameForVMD = 30.0f;

	//	MMDは1.0 == 8cm(目測)、EMOは 1.0 == 10cm なのでスケールしておく
	constexpr float ScaleFactorForMMDtoEMO = 10.0f / 8.0f;

	//	回転補正
	constexpr math::float3 RotationFactorForMMDtoEMO = { 0.0f, math::constants<float>::pi, 0.0f };
}

template<class Archive>
void animation::node::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(time),
		CEREAL_NVP(scale),
		CEREAL_NVP(rotation),
		CEREAL_NVP(translation));
}
template<class Archive>
void animation::node_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(node_name), CEREAL_NVP(nodes));
}

template<class Archive>
void animation::camera_spherical::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(time),
		CEREAL_NVP(distance),
		CEREAL_NVP(view_angle),
		CEREAL_NVP(is_parth),
		CEREAL_NVP(focus),
		CEREAL_NVP(rotation));
}

template<>
void animation::event::get_variable<int>(int& value) const
{
	if (type != variable_type::Int)
		return;

	value = std::atoi(variable.data());
}
template<>
void animation::event::get_variable<float>(float& value) const
{
	if (type != variable_type::Float)
		return;

	value = static_cast<float>(std::atof(variable.data()));
}
template<>
void animation::event::get_variable<std::string>(std::string& value) const
{
	if (type != variable_type::String)
		return;

	value = variable;
}

template<>
void animation::event::set_variable<int>(int& value)
{
	if (type != variable_type::Int)
		return;

	variable = std::to_string(value);
}
template<>
void animation::event::set_variable<float>(float& value)
{
	if (type != variable_type::Float)
		return;

	variable = std::to_string(value);
}
template<>
void animation::event::set_variable<std::string>(std::string& value)
{
	if (type != variable_type::String)
		return;

	variable = value;
}
template<class Archive>
void animation::event::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(time),
		CEREAL_NVP(type),
		CEREAL_NVP(function_name),
		CEREAL_NVP(variable));
}

template<class Archive>
void animation::variable::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(time), CEREAL_NVP(buffer));
}

template<class Archive>
void animation::morph::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(time), CEREAL_NVP(weight));
}

template<class Archive>
void animation::morph_data::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(morph_name), CEREAL_NVP(morphs));
}

float animation::interpolation::calculate_time(float rate) const
{
	if (datas.empty() || type == types::none)
		return rate;

	if (rate <= 0.0f)
		return datas.front().y;

	if (rate >= 1.0f)
		return datas.back().y;

	UINT data_index = 0;
	for (; data_index < datas.size() - 1; ++data_index)
	{
		if (datas[data_index].x <= rate && rate < datas[data_index + 1].x)
			break;
	}
	if (data_index >= datas.size() - 1)
		return datas.back().y;

	math::float2 s = datas[data_index + 0];
	math::float2 e = datas[data_index + 1];
	switch (type)
	{
		case types::smooth:		rate = math::smoothstep(s.x, e.x, rate); break;
		case types::linear:		rate = math::linearstep(s.x, e.x, rate); break;
		case types::discrete:	rate = (rate < (e.x - FLT_EPSILON)) ? 0.0f : 1.0f; break;
	}
	return math::lerp(s.y, e.y, rate);
}

template<class Archive>
void animation::interpolation::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(type), CEREAL_NVP(datas));
}

template<class Archive>
void animation::data::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 1:
		{
			archive(CEREAL_NVP(name),
					CEREAL_NVP(end_time),
					CEREAL_NVP(root_motion_xz),
					CEREAL_NVP(root_motion_y),
					CEREAL_NVP(root_rotation),
					CEREAL_NVP(root_motion_node_name),
					CEREAL_NVP(time_curve),
					CEREAL_NVP(nodes),
					CEREAL_NVP(camera_sphericals),
					CEREAL_NVP(events),
					CEREAL_NVP(variables),
					CEREAL_NVP(morphs));
			break;
		}
		case 2:
		{
			archive(CEREAL_NVP(name),
					CEREAL_NVP(end_time),
					CEREAL_NVP(root_motion_xz),
					CEREAL_NVP(root_motion_y),
					CEREAL_NVP(root_rotation),
					CEREAL_NVP(root_motion_node_name),
					CEREAL_NVP(time_curve),
					CEREAL_NVP(nodes),
					CEREAL_NVP(camera_sphericals),
					CEREAL_NVP(events),
					CEREAL_NVP(variables));
			break;
		}
		case 3:
		{
			archive(CEREAL_NVP(name),
					CEREAL_NVP(end_time),
					CEREAL_NVP(time_curve),

					CEREAL_NVP(root_motion_xz),
					CEREAL_NVP(root_motion_y),
					CEREAL_NVP(root_rotation),
					CEREAL_NVP(root_motion_node_name),

					CEREAL_NVP(foot_ik),
					CEREAL_NVP(hand_ik),
					CEREAL_NVP(aim_ik),

					CEREAL_NVP(nodes),
					CEREAL_NVP(camera_sphericals),
					CEREAL_NVP(events),
					CEREAL_NVP(variables));
			break;
		}
	}
}

float animation::data::calculate_time(float time) const
{
	return time_curve.calculate_time(time / end_time) * end_time;
}

bool animation::data::inner_compute_animation(animation::node& out, size_t node_index, float time) const
{
	auto& animation_nodes = nodes[node_index];

	if (animation_nodes.nodes.empty())
		return false;

	auto animation_node_ptr = animation_nodes.nodes.data();
	size_t index = 0, node_max = animation_nodes.nodes.size();
	for (; index < node_max - 1; ++index)
	{
		auto& animation_1 = *(animation_node_ptr + index + 0);
		auto& animation_2 = *(animation_node_ptr + index + 1);
		if (time < animation_1.time || animation_2.time <= time)
			continue;

		float value = (time - animation_1.time) / (animation_2.time - animation_1.time);
		out.scale = math::lerp(math::vector3f(animation_1.scale), math::vector3f(animation_2.scale), value).to_float3();
		out.rotation = math::slerp(math::quaternionf(animation_1.rotation), math::quaternionf(animation_2.rotation), value).to_float4();
		out.translation = math::lerp(math::vector3f(animation_1.translation), math::vector3f(animation_2.translation), value).to_float3();
		index = node_max;
		break;
	}
	if (index != node_max)
	{
		auto& an = animation_nodes.nodes[index];
		if (an.time <= time)
		{
			out.scale = an.scale;
			out.rotation = an.rotation;
			out.translation = an.translation;
		}
	}
	return true;
}

bool animation::data::compute_animation(std::vector<animation::node>& outs, float time) const
{
	if (outs.size() < nodes.size())
		outs.resize(nodes.size());

	float current_time = calculate_time(time);
	auto outs_ptr = outs.data();
	for (size_t node_index = 0; node_index < nodes.size(); ++node_index)
	{
		auto& node = *(outs_ptr + node_index);
		if (!inner_compute_animation(node, node_index, current_time))
			continue;
	}
	return true;
}

bool animation::data::compute_animation(animation::node& out, size_t node_index, float time) const
{
	return inner_compute_animation(out, node_index, calculate_time(time));
}

bool animation::data::compute_animation(animation::camera_spherical& out, float time) const
{
	float current_time = calculate_time(time);

	size_t index = 0;
	auto data = camera_sphericals.data();
	for (; index < camera_sphericals.size() - 1; ++index)
	{
		auto& animation_1 = data[index + 0];
		auto& animation_2 = data[index + 1];
		float value = (current_time - animation_1.time) / (animation_2.time - animation_1.time);
		if (0.0f <= value && value < 1.0f)
		{
			out.distance = math::lerp(animation_1.distance, animation_2.distance, value);
			out.focus = math::lerp(math::vector3f(animation_1.focus), math::vector3f(animation_2.focus), value).to_float3();
			out.is_parth = math::lerp(animation_1.is_parth, animation_2.is_parth, value);

			const math::quaternionf quaternion1 = math::rotatorf(math::vector3f(animation_1.rotation)).to_quaternion();
			const math::quaternionf quaternion2 = math::rotatorf(math::vector3f(animation_2.rotation)).to_quaternion();
			out.rotation = math::rotatorf(math::slerp(quaternion1, quaternion2, value)).to_vector3().to_float3();
			out.view_angle = math::lerp(animation_1.view_angle, animation_2.view_angle, value);
			index = camera_sphericals.size();
			break;
		}
	}
	if (index != camera_sphericals.size() && camera_sphericals[index].time <= current_time)
	{
		out.distance = camera_sphericals[index].distance;
		out.focus = camera_sphericals[index].focus;
		out.is_parth = camera_sphericals[index].is_parth;
		out.rotation = camera_sphericals[index].rotation;
		out.view_angle = camera_sphericals[index].view_angle;
	}
	return true;
}

bool animation::data::inner_compute_animation(animation::morph& out, size_t morph_index, float time) const
{
	auto& animation_morphs = morphs[morph_index];

	if (animation_morphs.morphs.empty())
		return false;

	auto animation_morph_ptr = animation_morphs.morphs.data();
	size_t index = 0, morph_max = animation_morphs.morphs.size();
	for (; index < morph_max - 1; ++index)
	{
		auto& animation_1 = *(animation_morph_ptr + index + 0);
		auto& animation_2 = *(animation_morph_ptr + index + 1);
		if (time < animation_1.time || animation_2.time <= time)
			continue;

		float value = (time - animation_1.time) / (animation_2.time - animation_1.time);
		out.weight = math::lerp(animation_1.weight, animation_2.weight, value);
		index = morph_max;
		break;
	}
	if (index != morph_max)
	{
		auto& am = animation_morphs.morphs[index];
		if (am.time <= time)
		{
			out.weight = am.weight;
		}
	}
	return true;
}

bool animation::data::compute_animation(std::vector<morph>& outs, float time) const
{
	if (outs.size() < morphs.size())
		outs.resize(morphs.size());

	float current_time = calculate_time(time);
	auto outs_ptr = outs.data();
	for (size_t morph_index = 0; morph_index < morphs.size(); ++morph_index)
	{
		auto& morph = *(outs_ptr + morph_index);
		if (!inner_compute_animation(morph, morph_index, current_time))
			continue;
	}
	return true;
}

bool animation::data::compute_animation(float time, std::string_view identifier, variable_function function) const
{
	auto it = variables.find(std::string(identifier));
	if (it == variables.end())
		return false;

	float current_time = calculate_time(time);
	size_t index = 0;
	auto data = it->second.data();
	for (; index < it->second.size() - 1; ++index)
	{
		auto& animation_1 = data[index + 0];
		auto& animation_2 = data[index + 1];
		float value = (current_time - animation_1.time) / (animation_2.time - animation_1.time);
		if (0.0f <= value && value < 1.0f)
		{
			function(value, identifier, animation_1, animation_2);
			index = variables.size();
			break;
		}
	}
	if (index != variables.size() && it->second[index].time <= current_time)
	{
		function(0.0f, identifier, it->second[index], it->second[index]);
	}
	return true;
}

bool animation::data::compute_animation(float time, variable_function function) const
{
	float current_time = calculate_time(time);

	//	登録されている物全ての処理を行う
	for (auto& it_variable : variables)
	{
		size_t index = 0;
		auto data = it_variable.second.data();
		for (; index < it_variable.second.size() - 1; ++index)
		{
			auto& animation_1 = data[index + 0];
			auto& animation_2 = data[index + 1];
			float value = (current_time - animation_1.time) / (animation_2.time - animation_1.time);
			if (0.0f <= value && value < 1.0f)
			{
				function(value, it_variable.first, animation_1, animation_2);
				index = variables.size();
				break;
			}
		}
		if (index != variables.size() && it_variable.second[index].time <= current_time)
		{
			function(0.0f, it_variable.first, it_variable.second[index], it_variable.second[index]);
		}
	}
	return true;
}

bool animation::data::call_events(math::float2 time_range, std::function<void(const event& event)> function) const
{
	float previous_time = calculate_time(time_range.x) * end_time;
	float current_time = calculate_time(time_range.y) * end_time;
	for (auto& event : events)
	{
		bool skip = !(previous_time <= event.time && event.time < current_time);
		if (current_time < previous_time)
		{
			skip = true;
			if ((previous_time <= event.time && event.time < end_time) ||
				(0.0f <= event.time && event.time < current_time))
			{
				skip = false;
			}
		}
		if (skip)
			continue;
		function(event);
	}
	return true;
}

animation_resource::animation_resource()
: file_path()
, animations()
{
}

animation_resource::~animation_resource()
{
}

size_t animation_resource::get_index_from_name(std::string_view name) const
{
	size_t index = 0;
	for (auto& animation : animations)
	{
		if (animation.name == name)
		{
			return index;
		}
		++index;
	}
	return invalid_id;
}

bool animation_resource::add_animation_data(const animation::data& data)
{
	auto it = std::find_if(animations.begin(),
						animations.end(),
						[&](const animation::data& d)
						{
							return d.name == data.name;
						});
	if (it == animations.end())
	{
		animations.push_back(data);
		return true;
	}
	*it = data;	//	登録
	return true;
}

bool animation_resource::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if(ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(CEREAL_NVP(animations));
		}
		catch (...)
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::animation, "animation_resource serialize failed[%s]", file_path.data());
			return false;
		}

		DEBUG_LOG_CATEGORY(logger::categorys::animation, "animation_resource serialize successed[%s]", file_path.data());
		return true;
	}
	DEBUG_ERROR_CATEGORY(logger::categorys::animation, "animation_resource open failed[%s]", file_path.data());
	return false;
}

bool animation_resource::deserialize(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if(istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(animations));
		}
		catch (...)
		{
			DEBUG_ERROR_CATEGORY(logger::categorys::animation, "animation_resource deserialize failed[%s]", file_path.data());
			return false;
		}

		this->file_path = file_path;
		DEBUG_LOG_CATEGORY(logger::categorys::animation, "animation_resource deserialize successed[%s]", file_path.data());
		return true;
	}
	DEBUG_ERROR_CATEGORY(logger::categorys::animation, "animation_resource open failed[%s]", file_path.data());
	return false;
}

bool animation_resource::convert_from_camera_vmd(std::string_view animation_name, const vmd_loader& loader)
{
	animation::data animation;
	animation.name = animation_name;
	animation.end_time = loader.get_end_frame() / NormalizedFrameForVMD;

	const math::matrixf rotation_matrix = math::matrixf::rotation_xyz(math::vector3f(RotationFactorForMMDtoEMO));

	auto calculate_camera_animation = [&](const vmd_loader& loader, float time)
		{
			animation::camera_spherical camera;
			camera.time = time;

			auto camera_animations = loader.get_camera();
			float frame = time * NormalizedFrameForVMD;
			size_t index = 0;
			for (; index < camera_animations.size() - 1; ++index)
			{
				auto& animation_1 = camera_animations[index + 0];
				auto& animation_2 = camera_animations[index + 1];
				float value = (frame - animation_1.frame) / (animation_2.frame - animation_1.frame);
				if (0.0f <= value && value < 1.0f)
				{
					camera.distance = math::lerp(animation_1.distance, animation_2.distance, value);
					camera.distance *= -ScaleFactorForMMDtoEMO;
					camera.focus = math::lerp(math::vector3f(animation_1.position), math::vector3f(animation_2.position), value).to_float3();
					camera.focus.x *= ScaleFactorForMMDtoEMO;
					camera.focus.y *= ScaleFactorForMMDtoEMO;
					camera.focus.z *= ScaleFactorForMMDtoEMO;
					camera.focus = math::vector3f::transform_coord(camera.focus, rotation_matrix).to_float3();

					camera.is_parth = math::lerp(animation_1.is_parth, animation_2.is_parth, value);
					camera.rotation = math::lerp(math::vector3f(animation_1.rotation), math::vector3f(animation_2.rotation), value).to_float3();

					math::matrixf rotation = math::matrixf::rotation_quaternion(math::rotatorf(math::vector3f(camera.rotation)).to_quaternion());
					rotation *= rotation_matrix;
					camera.rotation = math::rotatorf(rotation).to_vector3().to_float3();
					camera.rotation.x = -camera.rotation.x;
					camera.rotation.y = -camera.rotation.y;
					camera.rotation.z = -camera.rotation.z;

					camera.view_angle = math::angle::to_radian(static_cast<float>(math::lerp(animation_1.view_angle, animation_2.view_angle, value)));
					return camera;
				}
			}

			camera.distance = camera_animations[index].distance;
			camera.distance *= -ScaleFactorForMMDtoEMO;
			camera.focus = camera_animations[index].position;
			camera.focus.x *= ScaleFactorForMMDtoEMO;
			camera.focus.y *= ScaleFactorForMMDtoEMO;
			camera.focus.z *= ScaleFactorForMMDtoEMO;
			camera.focus = math::vector3f::transform_coord(camera.focus, rotation_matrix).to_float3();

			camera.is_parth = camera_animations[index].is_parth;
			camera.rotation = camera_animations[index].rotation;

			math::matrixf rotation = math::matrixf::rotation_quaternion(math::rotatorf(math::vector3f(camera.rotation)).to_quaternion());
			rotation *= rotation_matrix;
			camera.rotation = math::rotatorf(rotation).to_vector3().to_float3();
			camera.rotation.x = -camera.rotation.x;
			camera.rotation.y = -camera.rotation.y;
			camera.rotation.z = -camera.rotation.z;

			camera.view_angle = math::angle::to_radian(static_cast<float>(camera_animations[index].view_angle));
			return camera;
		};

	//	これデータ量増えるけど……まぁいいや
	float sampling_time = 0;
	for (sampling_time = 0; sampling_time <= animation.end_time; sampling_time += AnimationSamplingRate)
	{
		animation.camera_sphericals.emplace_back(calculate_camera_animation(loader, sampling_time));
	}
	animations.push_back(animation);
	return true;
}
