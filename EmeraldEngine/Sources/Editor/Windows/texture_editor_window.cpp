#include "Editor/Windows/texture_editor_window.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <filesystem>

#include "Texture/texture_importer.h"
#include "Graphics/Core/graphics_utility.h"
#include "Resource/resource_manager.h"
#include "System/asset_browser.h"
#include "System/imgui_texture_keeper.h"

#include "Core/logger.h"

texture_editor_window::texture_editor_window(std::string_view file_path)
	: file_path(file_path)
{
	load();
}

texture_editor_window::~texture_editor_window()
{
}

void texture_editor_window::register_open_handler(std::shared_ptr<editor_items::context> context)
{
	asset_browser::open_handler handler;
	handler.label = "Texture Editor";
	handler.extensions = { std::string(".") + std::string(texture_resource::extension) };
	handler.open_file = [context](const core::cache_string& file_path) mutable
	{
		editor_items::open_window<texture_editor_window>(context, file_path.view());
	};
	asset_browser::instance()->register_open_handler(std::move(handler));
}

std::string texture_editor_window::get_label() const
{
	const std::string name = std::filesystem::path(file_path).filename().string();
	return "Texture : " + core::convert_shift_jis_to_utf8(name);
}

void texture_editor_window::on_file_moved(std::string_view new_file_path)
{
	file_path = std::string(new_file_path);
}

bool texture_editor_window::load()
{
	auto operation = resource_manager::instance()->load<texture_resource>(file_path);
	if (!operation || !operation->is_succeeded())
	{
		DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "texture_editor open failed[%s]", file_path.c_str());
		return false;
	}

	target = operation->get_resource();
	revert();
	return true;
}

void texture_editor_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	retired_textures.clear();

	const std::string title = get_label() + "###texture_editor:" + core::convert_shift_jis_to_utf8(file_path);
	ImGui::SetNextWindowSize(ImVec2(720.0f, 820.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &open))
	{
		if (!target)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "open failed : %s", core::convert_shift_jis_to_utf8(file_path).c_str());
		}
		else
		{
			ImGui::TextUnformatted(core::convert_utf8(target->get_file_path()).c_str());
			ImGui::TextDisabled("uuid : %s", target->get_uuid().to_string().c_str());
			ImGui::Separator();

			imgui_render_preview();
			ImGui::Separator();
			imgui_render_settings();
			imgui_render_sampler();
			ImGui::Separator();
			imgui_render_buttons();
		}
	}
	ImGui::End();
}

void texture_editor_window::imgui_render_preview()
{
	const texture_ptr& value = target->get_texture();
	if (!value || value->get_dimension() != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
	{
		ImGui::TextDisabled("no texture");
		return;
	}

	const auto& desc = static_cast<const texture2d*>(value.get())->get_desc();
	ImGui::Text("%u x %u  mips %u  %s%s", desc.Width, desc.Height, desc.MipLevels, magic_enum::enum_name(desc.Format).data(),
		value->is_flag(texture::flags::cube_map) ? "  cube" : "");

	//	キューブと整数形式は ImGui で表示できない
	if (value->is_flag(texture::flags::cube_map) || graphics_utility::is_int_format(desc.Format))
		return;

	const float size = (std::min)(ImGui::GetContentRegionAvail().x, 384.0f);
	const float aspect = static_cast<float>(desc.Width) / static_cast<float>((std::max)(desc.Height, 1u));
	const ImVec2 extent = aspect >= 1.0f ? ImVec2(size, size / aspect) : ImVec2(size * aspect, size);
	ImGui::Image(imgui_texture_keeper::instance()->keep(value), extent);
}

void texture_editor_window::imgui_render_settings()
{
	ImGui::TextUnformatted("import");

	{
		if (ImGui::InputText("source", source_path_utf8))
			editing.source_path = core::convert_utf8_to_shift_jis(source_path_utf8);
		std::error_code error;
		if (editing.source_path.empty() || !std::filesystem::exists(std::filesystem::path(editing.source_path), error))
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "source not found (only sampler can be applied)");
	}

	//	プリセットは設定を埋めるだけ(元画像・最大解像度・サンプラーは残す)
	ImGui::TextUnformatted("preset");
	for (auto preset : magic_enum::enum_values<texture_import_settings::presets>())
	{
		ImGui::SameLine();
		if (ImGui::Button(magic_enum::enum_name(preset).data()))
		{
			texture_import_settings filled = texture_import_settings::make_preset(preset);
			filled.source_path = editing.source_path;
			filled.max_resolution = editing.max_resolution;
			filled.sampler = editing.sampler;
			editing = filled;
		}
	}

	ImGui::Combo("transfer function", editing.transfer_function);
	ImGui::Combo("gamut", editing.gamut);
	ImGui::Combo("format", editing.format);
	ImGui::Checkbox("generate mipmaps", &editing.generate_mipmaps);
	ImGui::Combo("filter", editing.filter);
	int max_resolution = static_cast<int>(editing.max_resolution);
	if (ImGui::DragInt("max resolution (0=unlimited)", &max_resolution, 16.0f, 0, 16384))
		editing.max_resolution = static_cast<std::uint32_t>((std::max)(max_resolution, 0));
}

void texture_editor_window::imgui_render_sampler()
{
	if (!ImGui::TreeNode("sampler"))
		return;

	auto& sampler = editing.sampler;
	static constexpr const char* filter_texts[] = { "point", "linear", "anisotropic" };
	static constexpr const char* address_texts[] = { "wrap", "mirror", "clamp", "border", "mirror_once" };
	static_assert(ARRAYSIZE(filter_texts) == static_cast<size_t>(texture_sampler_desc::filters::max));
	static_assert(ARRAYSIZE(address_texts) == static_cast<size_t>(texture_sampler_desc::address_modes::max));

	int filter = static_cast<int>(sampler.filter);
	if (ImGui::Combo("filter##sampler", &filter, filter_texts, ARRAYSIZE(filter_texts)))
		sampler.filter = static_cast<texture_sampler_desc::filters>(filter);

	auto address_combo = [&](const char* label, texture_sampler_desc::address_modes& mode)
	{
		int value = static_cast<int>(mode);
		if (ImGui::Combo(label, &value, address_texts, ARRAYSIZE(address_texts)))
			mode = static_cast<texture_sampler_desc::address_modes>(value);
	};
	address_combo("address_u", sampler.address_u);
	address_combo("address_v", sampler.address_v);
	address_combo("address_w", sampler.address_w);

	if (sampler.filter == texture_sampler_desc::filters::anisotropic)
	{
		int max_anisotropy = static_cast<int>(sampler.max_anisotropy);
		if (ImGui::SliderInt("max_anisotropy", &max_anisotropy, 1, D3D11_REQ_MAXANISOTROPY))
			sampler.max_anisotropy = static_cast<UINT>(max_anisotropy);
	}
	ImGui::DragFloat("mip_lod_bias", &sampler.mip_lod_bias, 0.01f, -16.0f, 15.99f);
	ImGui::DragFloat("min_lod", &sampler.min_lod, 0.1f, 0.0f, D3D11_FLOAT32_MAX);
	ImGui::DragFloat("max_lod", &sampler.max_lod, 0.1f, 0.0f, D3D11_FLOAT32_MAX);
	ImGui::ColorEdit4("border_color", sampler.border_color.data());
	ImGui::TreePop();
}

void texture_editor_window::imgui_render_buttons()
{
	const bool reimport = needs_reimport();
	if (ImGui::Button(reimport ? "Apply (reimport)" : "Apply"))
		apply();
	ImGui::SameLine();
	if (ImGui::Button("Revert"))
		revert();
	ImGui::SameLine();
	if (ImGui::Button("Reimport from source"))
	{
		retire_current_texture();
		if (auto reimported = texture_importer::instance()->import(editing.source_path, editing))
		{
			target = reimported;
			revert();
		}
	}
}

bool texture_editor_window::needs_reimport() const
{
	const auto& current = target->get_settings();
	return current.source_path != editing.source_path ||
		current.transfer_function != editing.transfer_function ||
		current.gamut != editing.gamut ||
		current.format != editing.format ||
		current.generate_mipmaps != editing.generate_mipmaps ||
		current.filter != editing.filter ||
		current.max_resolution != editing.max_resolution;
}

void texture_editor_window::apply()
{
	if (!target || !texture_importer::is_valid())
		return;

	if (needs_reimport())
	{
		retire_current_texture();
		auto reimported = texture_importer::instance()->import(editing.source_path, editing);
		if (!reimported)
		{
			DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "texture_editor reimport failed[%s]", std::string(target->get_file_path()).c_str());
			return;
		}
		target = reimported;
	}
	else if (!texture_importer::instance()->update_sampler(*target, editing.sampler))
	{
		DEBUG_ERROR_CATEGORY(core::logger::categorys::system, "texture_editor save failed[%s]", std::string(target->get_file_path()).c_str());
		return;
	}
	revert();
}

void texture_editor_window::retire_current_texture()
{
	if (target && target->get_texture())
		retired_textures.emplace_back(target->get_texture());
}

void texture_editor_window::revert()
{
	if (!target)
		return;
	editing = target->get_settings();
	source_path_utf8 = core::convert_utf8(editing.source_path);
}
#endif	//	defined(USE_IMGUI)
