#include "object/post_process_volume_object.h"
#include "Object/game_object_registry.h"
#include "Object/game_object_serializer.h"
#include "Texture/texture_resource_utility.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/common.hpp"

#include "implot/implot.h"

#if defined(USE_IMGUI)
#include "Graphics/Debug/debug_primitive_collector.h"
#include "Object/engine_context.h"
#endif	//	defined(USE_IMGUI)

void post_process_volume_object::sync_to_volume()
{
	if (!instance)
		return;

	const std::uint32_t spatial_version = get_spatial()->get_version();
	if (applied_spatial_version == spatial_version)
		return;

	applied_spatial_version = spatial_version;
	instance->set_position(get_world_translation());
	instance->set_rotation(get_world_rotation());
	const math::vector3d scale = get_world_scale();
	instance->set_extents(math::double3(scale.x * 0.5, scale.y * 0.5, scale.z * 0.5));
}

post_process_volume_object::post_process_volume_object(std::string_view name)
: spatial_object(name)
{
	instance = std::make_shared<post_process_volume>();

	//	@tool 拡大縮小が箱の大きさになるので、最初は 10m 四方にしておく
	set_local_scale(math::vector3d(10.0, 10.0, 10.0));
	sync_to_volume();

#if defined(USE_IMGUI)
	editor_context = editor_items::generate_context();
#endif	//	defined(USE_IMGUI)
}

post_process_volume_object::~post_process_volume_object()
{
}

void post_process_volume_object::post_update(float elapsed_time)
{
	sync_to_volume();

#if defined(USE_IMGUI)
	//	範囲表示
	if (!instance->is_flag(post_process_volume::flags::unbind) &&
		(instance->is_flag(post_process_volume::flags::active) ||
		 instance->is_flag(post_process_volume::flags::debug_draw)))
	{
		const math::float4 rotation = instance->get_rotation().to_float4();
		const math::float4 color = { 0.0f, 1.0f, 0.0f, 1.0f };
		const math::double3& extents = instance->get_extents();
		const math::float3 box_extents(static_cast<float>(extents.x), static_cast<float>(extents.y), static_cast<float>(extents.z));
		QUEUE_DEBUG_PRIMITIVE_BOX(get_context(), instance->get_position().to_double3(), rotation, box_extents, color, true);
	}
#endif	//	defined(USE_IMGUI)
}

void post_process_volume_object::post_render()
{
	auto data = instance->get_screen_space_reflection_constant();
	data.accumalate_reset = false;
	instance->set_screen_space_reflection_constant(data);
}

void post_process_volume_object::on_active()
{
	instance->set_flag(post_process_volume::flags::active, true);
}

void post_process_volume_object::on_deactive()
{
	instance->set_flag(post_process_volume::flags::active, false);
}

bool post_process_volume_object::is_flag(post_process_volume::flags flag) const
{
	return instance->is_flag(flag);
}

void post_process_volume_object::set_flag(post_process_volume::flags flag, bool value)
{
	instance->set_flag(flag, value);
}

const math::float4& post_process_volume_object::get_color() const
{
	return instance->get_color();
}

void post_process_volume_object::set_color(const math::float4& color)
{
	instance->set_color(color);
}

#if defined(USE_IMGUI)
#define CHECK_BOX(FLAG) \
{\
	bool is_##FLAG = instance->is_flag(post_process_volume::flags::##FLAG);\
	ImGui::Checkbox(#FLAG, &is_##FLAG);\
	instance->set_flag(post_process_volume::flags::##FLAG, is_##FLAG);\
}

void post_process_volume_object::imgui_render_core()
{
	base::imgui_render_core();
	sync_to_volume();

	CHECK_BOX(unbind);

	//	ブルーム
	imgui_bloom();

	//	レンズゴースト
	imgui_lens_ghost();

	//	被写界深度
	imgui_depth_of_field();

	//	SSR
	imgui_screen_space_reflection();

	//	モーションブラー
	imgui_motion_blur();

	//	露出補正
	imgui_exposure();

	//	トーンマッピング
	imgui_tone_mapping();

	//	マテリアル
	for (auto& material : instance->get_materials())
	{
		if (ImGui::TreeNode(system_utility::convert_utf8(material->get_name()).data()))
		{
			editor_items::edit_shared_material(editor_context, material->get_resource());
			ImGui::TreePop();
		}
	}
}

void post_process_volume_object::imgui_bloom()
{
	ImGui::PushID("##bloom");
	bool is_change = false;
	auto bloom_constant = instance->get_bloom_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = bloom_constant.is_active();
	if (ImGui::Checkbox("bloom", &flag))
	{
		bloom_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::DragFloat("threshold", &bloom_constant.threshold, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("intensity", &bloom_constant.intensity, 0.01f, 0.0f, 10.0f, "%.6f"))
		{
			is_change = true;
		}

		//	モード設定
		{
			std::vector<std::string> row_texts;
			std::vector<char*> combo_texts;
			for (auto& name : magic_enum::enum_names<bloom_data::modes>())
			{
				row_texts.emplace_back(name);
			}
			for (auto& name : row_texts)
			{
				combo_texts.emplace_back(name.data());
			}
			int value = static_cast<int>(bloom_constant.mode);
			if (ImGui::Combo("modes", &value, combo_texts.data(), static_cast<int>(combo_texts.size())))
			{
				bloom_constant.mode = static_cast<bloom_data::modes>(value);
				is_change = true;
			}
		}
		switch (bloom_constant.mode)
		{
			case bloom_data::modes::standard:
			{
				if (ImGui::DragFloat("deviation", &bloom_constant.deviation, 0.001f))
				{
					is_change = true;
				}
				break;
			}
			case bloom_data::modes::convolution:
			{
				ImGui::PushID(editor_context->get_item_id());
				ImGui::Text("image_kernel");
				ImGui::SameLine();
				if (editor_items::edit_texture_item(editor_context, bloom_constant.image_kernel))
				{
					is_change = true;
				}
				ImGui::PopID();
				break;
			}
		}

		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_bloom_constant(bloom_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_lens_ghost()
{
	ImGui::PushID("##lens_ghost");
	bool is_change = false;
	auto lens_ghosts_constant = instance->get_lens_ghosts_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = lens_ghosts_constant.is_active();
	if (ImGui::Checkbox("lens_ghost", &flag))
	{
		lens_ghosts_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::DragFloat("threshold", &lens_ghosts_constant.threshold, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("intensity", &lens_ghosts_constant.intensity, 0.01f, 0.0f, 10.0f, "%.6f"))
		{
			is_change = true;
		}

		if (ImGui::DragFloat("dispersion_scale", &lens_ghosts_constant.dispersion_scale, 0.01f, 0.0f, 2.0f, "%.6f"))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("dispersion_offset", &lens_ghosts_constant.dispersion_offset, 0.01f))
		{
			is_change = true;
		}

		if (ImGui::DragFloat("ghost_mask_radius", &lens_ghosts_constant.ghost_mask_radius, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("ghost_mask_fade", &lens_ghosts_constant.ghost_mask_fade, 0.01f, 0.0f, 1.0f, "%.6f"))
		{
			is_change = true;
		}

		if (ImGui::SliderInt("iteration", &lens_ghosts_constant.iteration, 0, 10))
		{
			is_change = true;
		}
		if (ImGui::SliderFloat("ghost_bokeh", &lens_ghosts_constant.ghost_bokeh, 1.0f, 8.0f, "%.6f"))
		{
			is_change = true;
		}

		{
			ImGui::PushID(editor_context->get_item_id());
			ImGui::Text("image_kernel");
			ImGui::SameLine();
			if (editor_items::edit_texture_item(editor_context, lens_ghosts_constant.image_kernel))
			{
				is_change = true;
			}
			ImGui::PopID();
		}

		{
			ImGui::PushID(editor_context->get_item_id());
			ImGui::Text("ghost_color_lut");
			ImGui::SameLine();
			if (editor_items::edit_texture_item(editor_context, lens_ghosts_constant.ghost_color_lut))
			{
				is_change = true;
			}
			ImGui::PopID();
		}

		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_lens_ghosts_constant(lens_ghosts_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_depth_of_field()
{
	ImGui::PushID("##depth_of_field");
	bool is_change = false;
	auto depth_of_field_constant = instance->get_depth_of_field_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = depth_of_field_constant.is_active();
	if (ImGui::Checkbox("depth_of_field", &flag))
	{
		depth_of_field_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::DragFloat("focal_length(mm)", &depth_of_field_constant.focal_length, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("focus_distance(m)", &depth_of_field_constant.focus_distance, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("circle_of_confusion(mm)", &depth_of_field_constant.circle_of_confusion, 0.001f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat("apeture", &depth_of_field_constant.apeture, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::DragFloat2("adjust_length(m)", &depth_of_field_constant.adjust_length.x, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::Checkbox("display_dof_area", &depth_of_field_constant.display_dof_area))
		{
			is_change = true;
		}
		if (ImGui::DragInt("blur_size", &depth_of_field_constant.blur_size, 1.0f, 1, 64))
		{
			is_change = true;
		}
		if (ImGui::DragInt("precision_scale_factor", &depth_of_field_constant.precision_scale_factor, 1.0f, 1, 0xFF))
		{
			is_change = true;
		}
		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_depth_of_field_constant(depth_of_field_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_screen_space_reflection()
{
	ImGui::PushID("##screen_space_reflection");
	bool is_change = false;
	auto screen_space_reflection_constant = instance->get_screen_space_reflection_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = screen_space_reflection_constant.is_active();
	if (ImGui::Checkbox("screen_space_reflection", &flag))
	{
		screen_space_reflection_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::DragFloat("max_distance", &screen_space_reflection_constant.ray_max_distance, 0.01f))
		{
			is_change = true;
		}
		if (ImGui::SliderFloat("ray_resolution", &screen_space_reflection_constant.ray_resolution, 0.0f, 1.0f))
		{
			is_change = true;
		}
		flag = screen_space_reflection_constant.is_flag(common_pipeline_states::screen_space_reflection_flags_consideration_uv_hole);
		if (ImGui::Checkbox("uv_hole_fill", &flag))
		{
			screen_space_reflection_constant.set_flag(common_pipeline_states::screen_space_reflection_flags_consideration_uv_hole, flag);
			is_change = true;
		}
		flag = screen_space_reflection_constant.is_flag(common_pipeline_states::screen_space_reflection_flags_reflection_dir_adjust);
		if (ImGui::Checkbox("dir_adjust", &flag))
		{
			screen_space_reflection_constant.set_flag(common_pipeline_states::screen_space_reflection_flags_reflection_dir_adjust, flag);
			is_change = true;
		}
		if (flag)
		{
			if (ImGui::DragFloat2("tickness", &screen_space_reflection_constant.ray_tickness.x, 0.0001f))
			{
				is_change = true;
			}
		}
		else
		{
			if (ImGui::DragFloat("tickness", &screen_space_reflection_constant.ray_tickness.x, 0.0001f))
			{
				is_change = true;
			}
		}
		flag = screen_space_reflection_constant.is_flag(common_pipeline_states::screen_space_reflection_flags_substep_check);
		if (ImGui::Checkbox("depthstep_check", &flag))
		{
			screen_space_reflection_constant.set_flag(common_pipeline_states::screen_space_reflection_flags_substep_check, flag);
			is_change = true;
		}
		if (flag)
		{
			if (ImGui::DragInt("depthstep_iteration_max", &screen_space_reflection_constant.ray_substep_iteration_max, 1, 20))
			{
				is_change = true;
			}
		}
		flag = screen_space_reflection_constant.is_flag(common_pipeline_states::screen_space_reflection_flags_step_random);
		if (ImGui::Checkbox("step_random", &flag))
		{
			screen_space_reflection_constant.set_flag(common_pipeline_states::screen_space_reflection_flags_step_random, flag);
			is_change = true;
		}
		if (flag)
		{
			if (ImGui::Checkbox("accumalate_reset", &screen_space_reflection_constant.accumalate_reset))
			{
				is_change = true;
			}
			if (ImGui::DragFloat("velocity_reset_threshold", &screen_space_reflection_constant.velocity_reset_threshold, 0.1f, 0.0f, 100.0f))
			{
				is_change = true;
			}
		}
		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_screen_space_reflection_constant(screen_space_reflection_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_motion_blur()
{
	ImGui::PushID("##motion_blur");
	bool is_change = false;
	auto motion_blur_constant = instance->get_motion_blur_constant();
	bool top_tree_open = ImGui::TreeNode("");
	ImGui::SameLine();
	bool flag = motion_blur_constant.is_active();
	if (ImGui::Checkbox("motion_blur", &flag))
	{
		motion_blur_constant.set_active(flag);
		is_change = true;
	}
	if (top_tree_open)
	{
		if (ImGui::DragInt("iteration", &motion_blur_constant.iteration, 0.01f, 0, 20))
		{
			is_change = true;
		}
		if (ImGui::SliderFloat("jitter_noise_value", &motion_blur_constant.jitter_noise_value, 0.0f, 1.0f))
		{
			is_change = true;
		}
		if (ImGui::SliderFloat("velocity_multiplyer", &motion_blur_constant.velocity_multiplyer, 0.0f, 10.0f))
		{
			is_change = true;
		}
		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_motion_blur_constant(motion_blur_constant);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_exposure()
{
	ImGui::PushID("##exposure");
	bool is_change = false;
	auto settings = instance->get_exposure_settings();
	if (ImGui::TreeNode("exposure"))
	{
		//	自動露出(none は F値・シャッター速度・ISO感度をそのまま使う)
		is_change |= ImGui::Combo("auto_exposure_mode", settings.auto_exposure_mode);
		is_change |= ImGui::Combo("metering_mode", settings.metering_mode);
		is_change |= ImGui::SliderFloat("exposure_compensation(EV)", &settings.exposure_compensation, -5.0f, 5.0f, "%.2f");
		is_change |= ImGui::SliderFloat("adaptation_time(s)", &settings.adaptation_time, 0.0f, 5.0f, "%.2f");

		//	手動の値(自動露出では初期値)
		is_change |= ImGui::SliderFloat("f_number", &settings.f_number, exposure_controller::f_number_minimum, exposure_controller::f_number_maximum, "f/%.1f");
		float shutter_denominator = 1.0f / settings.shutter_speed;
		if (ImGui::SliderFloat("shutter_speed(1/s)", &shutter_denominator, 1.0f / exposure_controller::shutter_speed_maximum, 1.0f / exposure_controller::shutter_speed_minimum, "1/%.0f", ImGuiSliderFlags_Logarithmic))
		{
			settings.shutter_speed = 1.0f / std::max(shutter_denominator, 1.0f);
			is_change = true;
		}
		is_change |= ImGui::SliderFloat("iso_speed", &settings.iso_speed, exposure_controller::iso_speed_minimum, exposure_controller::iso_speed_maximum, "%.0f", ImGuiSliderFlags_Logarithmic);
		ImGui::TreePop();
	}
	if (is_change)
	{
		instance->set_exposure_settings(settings);
	}
	ImGui::Separator();
	ImGui::PopID();
}

void post_process_volume_object::imgui_tone_mapping()
{
	ImGui::PushID("##tone_mapping_constant");
	if (ImGui::TreeNode("tone_mapping"))
	{
		//	今はラインハルト(白点 1)のみ。横軸・縦軸ともにディスプレイの最大輝度を 1 とした輝度
		if (ImPlot::BeginPlot("tone curve"))
		{
			ImPlot::SetupAxis(ImAxis_X1, "input luminance");
			ImPlot::SetupAxis(ImAxis_Y1, "output luminance");
			constexpr float white_point = 1.0f;
			std::vector<float> plot_buffer;
			plot_buffer.resize(1000);
			for (int index = 0; index < plot_buffer.size(); ++index)
			{
				float x = static_cast<float>(index) / 100.0f;
				plot_buffer[index] = x / (1.0f + x) * (1.0f + white_point) / white_point;
			}
			ImPlot::PlotLine("", plot_buffer.data(), static_cast<int>(plot_buffer.size()), 0.01f);
			ImPlot::EndPlot();
		}
		ImGui::TreePop();
	}
	ImGui::Separator();
	ImGui::PopID();
}

#undef CHECK_BOX
#endif	//	defined(USE_IMGUI)

template<class Archive>
void post_process_volume_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	//	@tool 名前のノードの中で items を読み書きする
	auto section = [&archive](const char* name, auto&& items)
	{
		archive.setNextName(name);
		archive.startNode();
		items();
		archive.finishNode();
	};

	//	@tool テクスチャは UUID で持ち、読込時は参照があるときだけ差し替える（無ければ既定のまま）
	auto texture = [&archive](const char* name, resource::asset_handle<texture_resource>& handle)
	{
		universally_unique_identifier uuid = handle.get_reference().uuid;
		game_object_serializer::serialize_asset_reference(archive, name, uuid);
		if constexpr (Archive::is_loading::value)
		{
			if (!uuid.is_empty())
				handle = texture_resource_utility::load_texture_resource(game_object_serializer::resolve_asset_path(uuid));
		}
	};

	bool unbind = instance->is_flag(post_process_volume::flags::unbind);
	archive(CEREAL_NVP(unbind));

	bloom_data bloom = instance->get_bloom_constant();
	bool bloom_active = bloom.is_active();
	section("bloom", [&]()
	{
		archive(cereal::make_nvp("active", bloom_active), cereal::make_nvp("threshold", bloom.threshold), cereal::make_nvp("intensity", bloom.intensity),
			cereal::make_nvp("mode", bloom.mode), cereal::make_nvp("deviation", bloom.deviation));
		texture("image_kernel", bloom.image_kernel);
	});

	lens_ghosts_data lens_ghosts = instance->get_lens_ghosts_constant();
	bool lens_ghosts_active = lens_ghosts.is_active();
	section("lens_ghosts", [&]()
	{
		archive(cereal::make_nvp("active", lens_ghosts_active), cereal::make_nvp("threshold", lens_ghosts.threshold), cereal::make_nvp("intensity", lens_ghosts.intensity),
			cereal::make_nvp("dispersion_scale", lens_ghosts.dispersion_scale), cereal::make_nvp("dispersion_offset", lens_ghosts.dispersion_offset),
			cereal::make_nvp("ghost_mask_radius", lens_ghosts.ghost_mask_radius), cereal::make_nvp("ghost_mask_fade", lens_ghosts.ghost_mask_fade),
			cereal::make_nvp("iteration", lens_ghosts.iteration), cereal::make_nvp("ghost_bokeh", lens_ghosts.ghost_bokeh));
		texture("image_kernel", lens_ghosts.image_kernel);
		texture("ghost_color_lut", lens_ghosts.ghost_color_lut);
	});

	depth_of_field_data depth_of_field = instance->get_depth_of_field_constant();
	bool depth_of_field_active = depth_of_field.is_active();
	section("depth_of_field", [&]()
	{
		archive(cereal::make_nvp("active", depth_of_field_active), cereal::make_nvp("focal_length", depth_of_field.focal_length),
			cereal::make_nvp("focus_distance", depth_of_field.focus_distance), cereal::make_nvp("circle_of_confusion", depth_of_field.circle_of_confusion),
			cereal::make_nvp("apeture", depth_of_field.apeture), cereal::make_nvp("blur_size", depth_of_field.blur_size),
			cereal::make_nvp("adjust_length", depth_of_field.adjust_length), cereal::make_nvp("precision_scale_factor", depth_of_field.precision_scale_factor));
	});

	screen_space_reflection_data screen_space_reflection = instance->get_screen_space_reflection_constant();
	bool screen_space_reflection_active = screen_space_reflection.is_active();
	section("screen_space_reflection", [&]()
	{
		archive(cereal::make_nvp("active", screen_space_reflection_active), cereal::make_nvp("ray_max_distance", screen_space_reflection.ray_max_distance),
			cereal::make_nvp("ray_resolution", screen_space_reflection.ray_resolution), cereal::make_nvp("ray_tickness", screen_space_reflection.ray_tickness),
			cereal::make_nvp("ray_substep_iteration_max", screen_space_reflection.ray_substep_iteration_max), cereal::make_nvp("ray_flags", screen_space_reflection.ray_flags),
			cereal::make_nvp("velocity_reset_threshold", screen_space_reflection.velocity_reset_threshold));
	});

	motion_blur_data motion_blur = instance->get_motion_blur_constant();
	bool motion_blur_active = motion_blur.is_active();
	section("motion_blur", [&]()
	{
		archive(cereal::make_nvp("active", motion_blur_active), cereal::make_nvp("iteration", motion_blur.iteration),
			cereal::make_nvp("jitter_noise_value", motion_blur.jitter_noise_value), cereal::make_nvp("velocity_multiplyer", motion_blur.velocity_multiplyer));
	});

	exposure_settings exposure = instance->get_exposure_settings();
	section("exposure", [&]()
	{
		archive(cereal::make_nvp("auto_exposure_mode", exposure.auto_exposure_mode), cereal::make_nvp("metering_mode", exposure.metering_mode),
			cereal::make_nvp("exposure_compensation", exposure.exposure_compensation), cereal::make_nvp("adaptation_time", exposure.adaptation_time),
			cereal::make_nvp("f_number", exposure.f_number), cereal::make_nvp("shutter_speed", exposure.shutter_speed), cereal::make_nvp("iso_speed", exposure.iso_speed));
	});

	//	@tool 読込時だけ、読んだ値をボリュームに反映する
	if constexpr (Archive::is_loading::value)
	{
		instance->set_flag(post_process_volume::flags::unbind, unbind);
		bloom.set_active(bloom_active);
		instance->set_bloom_constant(bloom);
		lens_ghosts.set_active(lens_ghosts_active);
		instance->set_lens_ghosts_constant(lens_ghosts);
		depth_of_field.set_active(depth_of_field_active);
		instance->set_depth_of_field_constant(depth_of_field);
		screen_space_reflection.set_active(screen_space_reflection_active);
		screen_space_reflection.accumalate_reset = true;
		instance->set_screen_space_reflection_constant(screen_space_reflection);
		motion_blur.set_active(motion_blur_active);
		instance->set_motion_blur_constant(motion_blur);
		instance->set_exposure_settings(exposure);
	}
}

void post_process_volume_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	game_object_serializer::save_fields(archive, "post_process_volume_object", *this);
}

void post_process_volume_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	game_object_serializer::load_fields(archive, "post_process_volume_object", *this);
}

GAME_OBJECT_REGISTER(post_process_volume_object, "Rendering");
