#include "Material/material_resource.h"
#include "Material/material_resource_factory.h"
#include "Material/material_shader_map.h"
#include "Texture/texture.h"
#include "Graphics/Core/render_data.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "Resource/resource_manager.h"
#include "Resource/data_compression.h"
#include "Resource/asset_file_header.h"
#include "System/project_path.h"

#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/unordered_map.hpp"

namespace
{
	using variable_kind = pipeline_state_base::variable_kinds;
	using variable_type = pipeline_state_base::variable_types;

	bool split_array_element(std::string& inout_text, UINT& out_element)
	{
		auto head_pos = inout_text.find("[");
		auto tail_pos = inout_text.find("]");
		if (head_pos != std::string::npos &&
			tail_pos != std::string::npos)
		{
			//	変数名と配列数取得
			std::string st = inout_text.substr(head_pos + 1, tail_pos - head_pos - 1);
			inout_text = inout_text.substr(0, head_pos);
			out_element = atoi(st.data());
			return true;
		}
		return false;
	}

	//	サーフェイスパスへ変換
	UINT to_surface_pass(material_resource::surface_paths path)
	{
		switch (path)
		{
			case material_resource::surface_paths::depth_write_pass: return static_cast<UINT>(material_shader_map::surface_passes::depth_write);
			case material_resource::surface_paths::shadow_write_pass: return static_cast<UINT>(material_shader_map::surface_passes::shadow_write);
			case material_resource::surface_paths::forward_pass: return static_cast<UINT>(material_shader_map::surface_passes::forward);
			default: return static_cast<UINT>(material_shader_map::surface_passes::g_buffer);
		}
	}

	//	マテリアルでのパスの番号を material_shader_map のパスの番号に変換
	UINT to_shader_map_pass(material_resource::domains domain, UINT index)
	{
		switch (domain)
		{
			case material_resource::domains::surface:
				return to_surface_pass(static_cast<material_resource::surface_paths>(index));
			case material_resource::domains::particle:
				return index;
			case material_resource::domains::deferred_decal:
				return index == static_cast<UINT>(material_resource::decal_paths::fullscreen) ? static_cast<UINT>(material_shader_map::decal_passes::fullscreen) : static_cast<UINT>(material_shader_map::decal_passes::primitive);
			default:
				return 0;
		}
	}
}

// CEREALバージョン定義
CEREAL_CLASS_VERSION(material_resource::pipeline_state, 1)
CEREAL_CLASS_VERSION(material_resource::constant_buffer::subset, 1)
CEREAL_CLASS_VERSION(material_resource::constant_buffer, 1)
CEREAL_CLASS_VERSION(material_resource::shader_resource, 1)
CEREAL_CLASS_VERSION(material_resource::shader_resource::resource, 1)
CEREAL_CLASS_VERSION(material_resource::physical_datas, 1)
CEREAL_CLASS_VERSION(material_resource, 1)

template<class Archive>
void material_resource::pipeline_state::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name));
}

template<class Archive>
void material_resource::constant_buffer::subset::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(start_offset),
			CEREAL_NVP(size),
			CEREAL_NVP(stride),
			CEREAL_NVP(kind),
			CEREAL_NVP(type),
			CEREAL_NVP(rows),
			CEREAL_NVP(columns));
}

template<class Archive>
void material_resource::constant_buffer::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(data_buffer), CEREAL_NVP(subsets), CEREAL_NVP(identifieres));
}

template<class Archive>
void material_resource::shader_resource::resource::serialize(Archive& archive, int version)
{
	//	ファイルを持たないテクスチャリソースは参照として残せないので空で書く
	if constexpr (Archive::is_saving::value)
	{
		if (asset && asset->get_file_path().empty())
		{
			const ::resource::asset_handle<texture_resource> empty;
			archive(cereal::make_nvp("asset", empty));
			return;
		}
	}
	archive(CEREAL_NVP(asset));
}

template<class Archive>
void material_resource::shader_resource::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(resources));
}

template<class Archive>
void material_resource::physical_datas::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(friction));
}

material_resource::material_resource()
: name()
, domain(domains::unknown)
, blend_mode(blend_modes::opaque)
, blendable_location(blendable_locations::after_dof_translucency)
, translucency_path(translucency_paths::after_dof)
, shading_model(shading_models::unlit)
, pipeline_states()
, system_flags(0)
, alpha_clip_threshold(0.5f)
, physical_data()
, shader_resources()
, constant_buffers()
{
	set_flag(flags::shadow_caster, true);
	set_flag(flags::shadow_receiver, true);
}

material_resource::material_resource(std::string_view name)
: material_resource()
{
	this->name = name;
}

material_resource::material_resource(const material_resource& material)
{
	material.duplicate(*this);
}

material_resource::material_resource(material_resource&& material)
{
	material.duplicate(*this);
}

material_resource& material_resource::operator =(const material_resource& material)
{
	material.duplicate(*this);
	return *this;
}

material_resource::~material_resource()
{
}

void material_resource::flush_constant_buffers(gpu_upload_list& upload_list)
{
#if defined(USE_IMGUI)
	if (is_error())
	{
		return;
	}
#endif	//	defined(USE_IMGUI)

	//	定数バッファ設定
	for (auto& cb : constant_buffers)
	{
		if (cb.second.dirty)
		{
			upload_list.update_whole(cb.second.buffer.Get(), cb.second.data_buffer.data(), static_cast<UINT>(cb.second.data_buffer.size()));
			cb.second.dirty = false;
		}
	}
}

template<class Archive>
void material_resource::serialize(Archive& archive, int version)
{
	archive(CEREAL_NVP(name),
			CEREAL_NVP(domain),
			CEREAL_NVP(blend_mode),
			CEREAL_NVP(shading_model),
			CEREAL_NVP(blendable_location),
			CEREAL_NVP(translucency_path),
			CEREAL_NVP(system_flags),
			CEREAL_NVP(alpha_clip_threshold),
			CEREAL_NVP(physical_data),
			CEREAL_NVP(shader_resources),
			CEREAL_NVP(constant_buffers),
			CEREAL_NVP(shader_path),
			CEREAL_NVP(static_switches));
}

//	モデル・エフェクトのファイルはバイナリで読み書きする
template void material_resource::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive& archive, int version);
template void material_resource::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive& archive, int version);

void material_resource::duplicate(material_resource& material) const
{
	std::stringstream iostream;
	serialize_stream(iostream);
	material.deserialize_stream(iostream);

	for (auto& [name, source] : shader_resources)
	{
		auto it = material.shader_resources.find(name);
		if (it == material.shader_resources.end())
			continue;

		const std::size_t count = (std::min)(source.resources.size(), it->second.resources.size());
		for (std::size_t index = 0; index < count; ++index)
		{
			const auto& asset = source.resources[index].asset;
			if (asset)
				it->second.resources[index].asset = asset.get();
		}
	}
	material.build();
}

void material_resource::build()
{
	//	マテリアルシェーダーを使う場合は、シェーダーパスと静的パラメーターからパイプラインステートを決める
	//	PSO を名前で持つのは system ドメインだけ
	if (domain != domains::system)
	{
		refresh_pipeline_states();
		build_parameters();
		return;
	}

	//	パイプラインステート登録
	for (auto& pipeline_state : pipeline_states)
	{
		if (auto state = pipeline_state_manager::instance()->search(pipeline_state.name))
		{
			pipeline_state.value = state;
			pipeline_state.reload_version = state->get_reload_version();
		}
	}

	//	マテリアル設定
	build_parameters();
}

void material_resource::sync_shader_reloads()
{
	//	パイプラインステートが読み直されたか
	bool reloaded = false;
	for (auto& pipeline_state : pipeline_states)
	{
		const std::uint32_t current = pipeline_state.value ? pipeline_state.value->get_reload_version() : 0;
		if (pipeline_state.reload_version != current)
		{
			pipeline_state.reload_version = current;
			reloaded = true;
		}
	}

	//	シェーダーマップが生成済みのパイプラインステートを捨てたか(スイッチや GS の有無が変わった)
	const bool shader_map_changed = domain != domains::system && shader_map_version != material_shader_map::instance()->get_version();
	if (!reloaded && !shader_map_changed)
		return;

#if defined(USE_IMGUI)
	pipeline_states_verified = false;
#endif	//	defined(USE_IMGUI)

	//	取り直しの中でシェーダーマップが宣言を読み直す(宣言が変わっていれば他のマテリアルにも番号で伝わる)
	refresh_pipeline_states();
	build_parameters();
}

void material_resource::create_constant_buffer_resource(constant_buffer& target)
{
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	buffer_desc.ByteWidth = static_cast<UINT>(target.data_buffer.size());
	HRESULT hr = render_data::instance()->get_device()->CreateBuffer(&buffer_desc, nullptr, target.buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	target.dirty = true;
}

void material_resource::build_parameters()
{
	//	構成が変わるのでインスタンスに作り直させる
	++version;

#if defined(USE_IMGUI)
	if (is_error())
	{
		//	エラーならビルドしない
		return;
	}
#endif	//	defined(USE_IMGUI)

	//	退避
	auto temp_shader_resources = shader_resources;
	auto temp_constant_buffers = constant_buffers;
	shader_resources.clear();
	constant_buffers.clear();

	//	パラメーター構成の元にするパイプラインステート(マテリアルシェーダーは全スイッチ ON の構成から取る)
	std::vector<graphics_pipeline_state_ptr> parameter_sources;
	if (domain != domains::system)
	{
		material_shader_map::instance()->get_parameter_pipeline_states(shader_path, domain, parameter_sources);
	}
	else
	{
		for (auto& pipeline_state : pipeline_states)
		{
			if (pipeline_state.is_valid())
				parameter_sources.emplace_back(pipeline_state.value);
		}
	}

	//	パイプラインステートから入力リソース情報を取得
	input_resource_desc resource_desc;
	for (auto& parameter_source : parameter_sources)
	{
		std::vector<input_resource_desc> descs;
		parameter_source->generate_input_resource_descs(descs);

		//	名前が一緒の物は除外するようにしておく
		for (auto& desc : descs)
		{
			//	定数バッファ
			for (auto& cbv : desc.cbvs)
			{
				bool find = false;
				for (auto exclude_names : common_pipeline_states::system_constant_buffer_names)
				{
					if (cbv.name == exclude_names)
					{
						find = true;
						break;
					}
				}
				if (find)
					continue;

				decltype(resource_desc.cbvs)::iterator it = std::find_if(
					resource_desc.cbvs.begin(),
					resource_desc.cbvs.end(),
					[&](const decltype(resource_desc.cbvs)::value_type& it)
					{
						return it.name == cbv.name;
					});
				if (it == resource_desc.cbvs.end())
				{
					resource_desc.cbvs.emplace_back(cbv);
				}
			}

			//	リソース類
			for (auto& rv : desc.rvs)
			{
				bool find = false;
				for (auto exclude_names : common_pipeline_states::system_shader_resource_names)
				{
					if (rv.name == exclude_names)
					{
						find = true;
						break;
					}
				}
				if (find)
					continue;

				decltype(resource_desc.rvs)::iterator it = std::find_if(
					resource_desc.rvs.begin(),
					resource_desc.rvs.end(),
					[&](const decltype(resource_desc.rvs)::value_type& it)
					{
						return it.name == rv.name;
					});
				if (it == resource_desc.rvs.end())
				{
					resource_desc.rvs.emplace_back(rv);
				}
			}
		}
	}

	//	テクスチャ情報を構築
	for (auto& rv : resource_desc.rvs)
	{
		if (!rv.is_valid())
			continue;

		if (rv.is_srvs())
		{
			//	登録済みであれば除外
			auto it = shader_resources.find(rv.name);
			if (it != shader_resources.end())
				continue;

			//	空のテクスチャを登録
			shader_resource sr = {};
			sr.resources.resize(rv.array_size);
			shader_resources.insert(std::make_pair(rv.name, sr));
		}
	}

	//	定数バッファ情報を構築
	for (auto& cbv : resource_desc.cbvs)
	{
		constant_buffer cb;
		cb.data_buffer.resize(cbv.total_size);
		memset(cb.data_buffer.data(), 0, cb.data_buffer.size());

		//	識別子登録処理
		{
			std::function<UINT(const variable_desc&, std::string_view, UINT)> register_data_function;
			register_data_function = [&](const variable_desc& desc, std::string_view root_name, UINT start_offset) -> UINT
			{
				std::string variable_name = std::string(root_name);
				variable_name += desc.name;

				constant_buffer::subset subset;
				subset.start_offset = start_offset + desc.start_offset;
				subset.size = desc.size;
				subset.kind = desc.kind;
				subset.type = desc.type;
				subset.rows = desc.rows;
				subset.columns = desc.columns;

				if (desc.kind == pipeline_state_base::variable_kinds::Struct)
				{
					UINT stride = 0;
					for (auto member : desc.members)
					{
						UINT size = register_data_function(member, variable_name + ".", subset.start_offset);
						stride += member.size == 0 ? size : member.size;
					}
					subset.stride = stride;
				}
				else
				{
					subset.stride = pipeline_state_base::get_variable_type_size(desc.kind, desc.type, desc.rows, desc.columns);
				}
				cb.identifieres.insert(std::make_pair(variable_name, cb.subsets.size()));
				cb.subsets.emplace_back(subset);

				return math::calc_alignment_power_of_two(subset.stride, 0x0Fu);	//	16バイトアライメント
			};

			for (auto& variable : cbv.variables)
			{
				register_data_function(variable, "", 0);
			}
		}

		//	定数バッファの生成
		create_constant_buffer_resource(cb);

		//	定数バッファ登録
		constant_buffers.insert(std::make_pair(cbv.name, std::move(cb)));
	}

	//	前回情報を登録
	{
		for (auto& texture : temp_shader_resources)
		{
			auto it = shader_resources.find(texture.first);
			if (it == shader_resources.end())
				continue;

			for (UINT index = 0; index < texture.second.resources.size(); ++index)
			{
				auto& temp_resource = texture.second.resources[index];
				auto& resource = it->second.resources[index];
				resource = temp_resource;
			}
		}
		for (auto& constant_buffer : temp_constant_buffers)
		{
			for (auto& identifier : constant_buffer.second.identifieres)
			{
				auto& subset = constant_buffer.second.subsets[identifier.second];
				set_data(identifier.first, constant_buffer.second.data_buffer.data() + subset.start_offset, subset.size);
			}
		}
	}
}

void material_resource::set_name(std::string_view name)
{
	this->name = std::string(name);
}

std::string_view material_resource::get_name() const
{
	return name;
}

material_resource::domains material_resource::get_domain() const
{
	return domain;
}

void material_resource::set_domain(material_resource::domains domain)
{
	if (this->domain == domain)
		return;

	this->domain = domain;
	pipeline_states.clear();
	std::size_t size = 0; 
	switch (domain)
	{
		case material_resource::domains::surface:
			size = magic_enum::enum_count<surface_paths>() + 1;
			break;

		case material_resource::domains::particle:
			size = magic_enum::enum_count<particle_paths>() + 1;
			break;

		case material_resource::domains::deferred_decal:
			size = magic_enum::enum_count<decal_paths>() + 1;
			break;

		case material_resource::domains::post_process:
		case material_resource::domains::user_interface:
			size = 1;
			break;

		case material_resource::domains::system:
			size = magic_enum::enum_count<system_paths>() + 1;
			break;
		default:
			assert(!L"Not Compatible.");
			break;
	}

	pipeline_states.resize(size);

	//	マテリアルシェーダーを設定済みなら取り直す
	refresh_pipeline_states();
}

material_resource::blend_modes material_resource::get_blend_mode() const
{
	return blend_mode;
}

void material_resource::set_blend_mode(blend_modes blend_mode)
{
	if (this->blend_mode == blend_mode)
		return;

	this->blend_mode = blend_mode;

	//	ブレンドモードで使うパスと PS の有無が変わる
	refresh_pipeline_states();
}

void material_resource::set_flag(flags flag, bool value)
{
	const bool changed = is_flag(flag) != value;
	system_utility::set_bitflag(system_flags, 1 << static_cast<UINT>(flag), value);

	//	影を落とすかで影のパスを使うかが変わる
	if (changed && flag == flags::shadow_caster)
		refresh_pipeline_states();
}

void material_resource::set_shader_path(std::string_view path)
{
	//	@tool	区切りを「/」に揃えて保持する(空ならシェーダー未設定)
	std::string value = project_path::normalize(path);
	if (shader_path == value)
		return;

	shader_path = std::move(value);
	static_switches.clear();

	//	前に設定されていたパイプラインステートは使わない
	for (UINT index = 0; index < static_cast<UINT>(pipeline_states.size()); ++index)
		assign_pipeline_state(index, "", nullptr, true);

	if (domain != domains::system)
	{
		refresh_pipeline_states();
		build_parameters();
	}
}

void material_resource::set_static_switch(std::string_view name, bool value)
{
	const auto* shader_declaration = material_shader_map::instance()->find_declaration(shader_path, domain);
	if (!shader_declaration)
		return;

	auto it = std::find_if(shader_declaration->static_switches.begin(), shader_declaration->static_switches.end(),
		[&](const material_shader_map::static_switch_declaration& item) { return item.name == name; });
	if (it == shader_declaration->static_switches.end())
		return;

	//	既定値と同じなら設定を残さない
	if (value == it->default_value)
		static_switches.erase(it->name);
	else
		static_switches[it->name] = value;

	refresh_pipeline_states();
}

bool material_resource::get_static_switch(std::string_view name) const
{
	const auto* shader_declaration = material_shader_map::instance()->find_declaration(shader_path, domain);
	if (!shader_declaration)
		return false;

	for (auto& item : shader_declaration->static_switches)
	{
		if (item.name != name)
			continue;

		auto it = static_switches.find(item.name);
		return it != static_switches.end() ? it->second : item.default_value;
	}
	return false;
}

void material_resource::refresh_pipeline_states()
{
	//	system ドメインは PSO を名前で持つので取り直さない
	if (domain == domains::system)
		return;

	material_shader_map* shader_map = material_shader_map::instance();
	const auto* shader_declaration = shader_map->find_declaration(shader_path, domain);

	//	静的スイッチを宣言の並び順のビットにする
	std::uint64_t switch_bits = 0;
	if (shader_declaration)
	{
		for (std::size_t index = 0; index < shader_declaration->static_switches.size(); ++index)
		{
			if (get_static_switch(shader_declaration->static_switches[index].name))
				switch_bits |= 1ull << index;
		}
	}

	const bool is_opaque = blend_mode == blend_modes::opaque || blend_mode == blend_modes::mask;
	const bool is_translucent = blend_mode == blend_modes::translucent || blend_mode == blend_modes::addative;

	//	ドメインごとのパス数
	UINT path_count = 1;
	if (domain == domains::surface)
		path_count = static_cast<UINT>(magic_enum::enum_count<surface_paths>());
	else if (domain == domains::particle)
		path_count = static_cast<UINT>(magic_enum::enum_count<particle_paths>());
	else if (domain == domains::deferred_decal)
		path_count = static_cast<UINT>(magic_enum::enum_count<decal_paths>());

	if (pipeline_states.size() < path_count)
		pipeline_states.resize(path_count);

	for (UINT index = 0; index < path_count; ++index)
	{
		//	描画で使うパスだけ生成
		bool required = true;
		if (domain == domains::surface)
		{
			switch (static_cast<surface_paths>(index))
			{
				case surface_paths::depth_write_pass:
				case surface_paths::g_buffer_pass: required = is_opaque; break;
				case surface_paths::forward_pass: required = is_translucent; break;
				case surface_paths::shadow_write_pass: required = is_flag(flags::shadow_caster); break;
			}
		}
		else if (domain == domains::particle)
		{
			switch (static_cast<particle_paths>(index))
			{
				case particle_paths::depth_write_pass:
				case particle_paths::g_buffer_pass: required = is_opaque; break;
				case particle_paths::forward_pass: required = is_translucent; break;
			}
		}

		graphics_pipeline_state_ptr value;
		if (required && shader_declaration)
		{
			material_shader_map::request request;
			request.shader_path = shader_path;
			request.domain = domain;
			request.pass = to_shader_map_pass(domain, index);
			request.switch_bits = switch_bits;
			request.pixel = blend_mode == blend_modes::mask;
			value = shader_map->get_pipeline_state(request);
		}
		assign_pipeline_state(index, value ? value->get_name() : "", value, required);
	}

	//	取り直しの途中で宣言が読み直されることがあるので、最後に番号を覚える
	shader_map_version = shader_map->get_version();

#if defined(USE_IMGUI)
	pipeline_states_verified = false;
#endif	//	defined(USE_IMGUI)
}

void material_resource::assign_pipeline_state(UINT index, std::string_view name, graphics_pipeline_state_ptr value, bool required)
{
	if (index >= pipeline_states.size())
		pipeline_states.resize(index + 1);

	auto& target = pipeline_states[index];
	target.name = std::string(name);
	target.required = required;
	if (target.value == value)
		return;

	target.value = value;
	target.reload_version = value ? value->get_reload_version() : 0;
}

material_resource::shading_models material_resource::get_shading_model() const
{
	return shading_model;
}

void material_resource::set_shading_model(shading_models shading_model)
{
	this->shading_model = shading_model;
}

material_resource::blendable_locations material_resource::get_blendable_location() const
{
	return domain == domains::post_process ? blendable_location : blendable_locations::unknown;
}

void material_resource::set_blendable_location(blendable_locations location)
{
	if (domain == domains::post_process) blendable_location = location;
}

material_resource::translucency_paths material_resource::get_translucency_path() const
{
	if ((domain == domains::surface || domain == domains::particle) && (blend_mode == blend_modes::translucent || blend_mode == blend_modes::addative))
		return translucency_path;

	return translucency_paths::unknown;
}

void material_resource::set_translucency_path(translucency_paths pass)
{
	if (domain == domains::surface && blend_mode == blend_modes::translucent ||
		domain == domains::particle && blend_mode == blend_modes::translucent)
		translucency_path = pass;
}

const material_resource::constant_buffer_map& material_resource::get_constant_buffers() const
{
	return constant_buffers;
}

const material_resource::shader_resource_map& material_resource::get_shader_resources() const
{
	return shader_resources;
}

#if defined(USE_IMGUI)
material_resource::constant_buffer_map& material_resource::get_constant_buffers_for_editor()
{
	//	書き換えられる前提でインスタンスに作り直させる
	++version;
	return constant_buffers;
}

material_resource::shader_resource_map& material_resource::get_shader_resources_for_editor()
{
	//	書き換えられる前提でインスタンスに作り直させる
	++version;
	return shader_resources;
}
#endif	//	defined(USE_IMGUI)

bool material_resource::set_pipeline_state(domains domain, UINT index, std::string_view pipeline_state_name)
{
	//	ドメインが違えば登録させない
	if(this->domain != domain)
		return false;

	std::string name = std::string(pipeline_state_name);
	if (pipeline_states[index].name != name)
	{
		if (auto pipeline_state = pipeline_state_manager::instance()->search(name))
		{
			pipeline_states[index].name = name;
			pipeline_states[index].value = pipeline_state;
			pipeline_states[index].reload_version = pipeline_state->get_reload_version();

			//	マテリアルパラメーターの更新
			build_parameters();
			return true;
		}
		else
		{
			//	見つからない場合は解除する
			pipeline_states[index].name = name;
			pipeline_states[index].value = pipeline_state;
			pipeline_states[index].reload_version = 0;

			//	マテリアルパラメーターの更新
			build_parameters();
		}
	}
	return false;
}

bool material_resource::set_pipeline_state(system_paths type, std::string_view pipeline_state_name)
{
	return set_pipeline_state(domains::system, static_cast<UINT>(type), pipeline_state_name);
}

graphics_pipeline_state_ptr material_resource::get_pipeline_state(surface_paths value) const
{
	assert(domain == domains::surface);
	return get_pipeline_state(domains::surface, static_cast<UINT>(value));
}

graphics_pipeline_state_ptr material_resource::get_pipeline_state(particle_paths value) const
{
	assert(domain == domains::particle);
	return get_pipeline_state(domains::particle, static_cast<UINT>(value));
}

graphics_pipeline_state_ptr material_resource::get_pipeline_state(decal_paths value) const
{
	assert(domain == domains::deferred_decal);
	return get_pipeline_state(domains::deferred_decal, static_cast<UINT>(value));
}

graphics_pipeline_state_ptr material_resource::get_pipeline_state(system_paths value) const
{
	assert(domain == domains::system);
	return get_pipeline_state(domains::system, static_cast<UINT>(value));
}

graphics_pipeline_state_ptr material_resource::get_pipeline_state() const
{
	assert(domain != domains::surface ||
		   domain != domains::deferred_decal ||
		   domain != domains::particle ||
		   domain != domains::system);
	return get_pipeline_state(domain, 0);
}

graphics_pipeline_state_ptr material_resource::get_pipeline_state(domains domain, UINT index) const
{
	if (this->domain != domain)
		return nullptr;

#if defined(USE_IMGUI)
	//	エラーチェック
	if (is_error())
	{
		//	エラー表示用のマテリアルシェーダーで描く
		if (auto failed = material_shader_map::instance()->get_failed_pipeline_state(this->domain, to_shader_map_pass(this->domain, index)))
			return failed;
		assert(!"Not Comaptible Failed Shader.");
	}
#endif	//	defined(USE_IMGUI)

	return pipeline_states[index].value;
}

bool material_resource::set_data(std::string_view key_name, void* data, std::size_t data_size)
{
	std::string key = std::string(key_name);
	UINT array_element = 0;
	split_array_element(key, array_element);

	auto it = constant_buffers.begin();
	while (it != constant_buffers.end())
	{
		auto& cb = it->second;
		auto it_data = cb.identifieres.find(key);
		if (it_data != cb.identifieres.end())
		{
			auto& subset = cb.subsets[it_data->second];

			std::size_t start_offset = subset.start_offset + subset.stride * array_element;

			std::size_t buffer_size = cb.data_buffer.size() - start_offset;
			void* buffer = cb.data_buffer.data() + start_offset;
			memcpy_s(buffer, buffer_size, data, std::min(buffer_size, data_size));
			cb.dirty = true;
			++version;
			return true;
		}
		++it;
	}
	return false;
}

bool material_resource::get_data(std::string_view key_name, void* data, std::size_t data_size) const
{
	std::string key = std::string(key_name);
	UINT array_element = 0;
	split_array_element(key, array_element);

	auto it = constant_buffers.begin();
	while (it != constant_buffers.end())
	{
		auto& cb = it->second;
		auto it_data = cb.identifieres.find(key);
		if (it_data != cb.identifieres.end())
		{
			if (data)
			{
				auto& subset = cb.subsets[it_data->second];

				std::size_t start_offset = subset.start_offset + subset.stride * array_element;

				std::size_t buffer_size = cb.data_buffer.size() - start_offset;
				const void* buffer = cb.data_buffer.data() + start_offset;
				memcpy_s(data, data_size, buffer, std::min(data_size, buffer_size));
			}
			return true;
		}
		++it;
	}
	return false;
}

bool material_resource::set_texture(std::string_view key_name, texture_resource_ptr value)
{
	std::string key = std::string(key_name);
	UINT array_element = 0;
	split_array_element(key, array_element);

	auto it = shader_resources.find(key);
	if (it != shader_resources.end())
	{
		//	要素数をoオーバーしていたら除外
		if (it->second.resources.size() <= array_element)
			return false;

		auto& resource = it->second.resources[array_element];
		resource.asset = std::move(value);
		++version;
		return true;
	}
	return false;
}


bool material_resource::get_texture(std::string_view key_name, texture_ptr& value) const
{
	std::string key = std::string(key_name);
	UINT array_element = 0;
	split_array_element(key, array_element);

	auto it = shader_resources.find(key);
	if (it != shader_resources.end())
	{
		//	要素数をoオーバーしていたら除外
		if (it->second.resources.size() <= array_element)
			return false;

		auto& resource = it->second.resources[array_element];
		value = resource.get_texture();
		return true;
	}
	return false;
}

void material_resource::serialize_stream(std::stringstream& ostream) const
{
	cereal::JSONOutputArchive archive(ostream);

	try
	{
		archive(CEREAL_NVP(name),
				CEREAL_NVP(domain),
				CEREAL_NVP(blend_mode),
				CEREAL_NVP(shading_model),
				CEREAL_NVP(blendable_location),
				CEREAL_NVP(translucency_path),
				CEREAL_NVP(physical_data),
				CEREAL_NVP(pipeline_states),

				CEREAL_NVP(system_flags),
				CEREAL_NVP(alpha_clip_threshold),
				CEREAL_NVP(shader_resources),
				CEREAL_NVP(constant_buffers),
				CEREAL_NVP(shader_path),
				CEREAL_NVP(static_switches));
	}
	catch (...)
	{
		DEBUG_LOG_CATEGORY(logger::categorys::model, "material serialize_stream failed[%s]", name.data());
		return;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::model, "material serialize_stream succeeded[%s]", name.data());
}

void material_resource::deserialize_stream(std::stringstream& istream)
{
	cereal::JSONInputArchive archive(istream);

	try
	{
		archive(CEREAL_NVP(name),
				CEREAL_NVP(domain),
				CEREAL_NVP(blend_mode),
				CEREAL_NVP(shading_model),
				CEREAL_NVP(blendable_location),
				CEREAL_NVP(translucency_path),
				CEREAL_NVP(physical_data),
				CEREAL_NVP(pipeline_states),

				CEREAL_NVP(system_flags),
				CEREAL_NVP(alpha_clip_threshold),
				CEREAL_NVP(shader_resources),
				CEREAL_NVP(constant_buffers),
				CEREAL_NVP(shader_path),
				CEREAL_NVP(static_switches));
	}
	catch (...)
	{
		DEBUG_LOG_CATEGORY(logger::categorys::model, "material deserialize_stream failed[%s]", name.data());
		return;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::model, "material deserialize_stream succeeded[%s]", name.data());
}

#if defined(USE_IMGUI)
bool material_resource::is_error() const
{
	if (pipeline_states_verified)
		return false;

	std::size_t size = 0;
	switch (domain)
	{
		case material_resource::domains::surface: size = magic_enum::enum_count<surface_paths>(); break;
		case material_resource::domains::post_process: size = 1; break;
		case material_resource::domains::deferred_decal: size = magic_enum::enum_count<decal_paths>(); break;
		case material_resource::domains::user_interface: size = 1; break;
		case material_resource::domains::particle: size = magic_enum::enum_count<particle_paths>(); break;

		case material_resource::domains::system: size = magic_enum::enum_count<system_paths>(); break;
		default: assert(!L"Not Compatible."); break;
	}
	for (UINT index = 0; index < size; ++index)
	{
		auto& pipeline_state = pipeline_states[index];

		//	描画で使わないパスは調べない
		if (!pipeline_state.required)
			continue;

		if (!pipeline_state.value)
			return true;

		if (pipeline_state.value->is_failed())
			return true;
	}

	pipeline_states_verified = true;
	return false;
}

#endif	//	defined(USE_IMGUI)


bool material_resource::load_file(std::string_view file_path)
{
	::resource::asset_file_header header;
	std::vector<char> body;
	if (!::resource::read_asset_file(file_path, ::resource::asset_types::material, header, body))
		return false;

	::resource::memory_streambuf stream_buffer(body.data(), body.size());
	std::istream istream(&stream_buffer);
	try
	{
		cereal::BinaryInputArchive archive(istream);
		archive(*this);
	}
	catch (...)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "material_resource deserialize failed[%s]", std::string(file_path).c_str());
		return false;
	}

	inherit_uuid(header.uuid);
	set_file_path(file_path);
	set_missing(false);
	return true;
}

bool material_resource::save_file(std::string_view file_path)
{
	std::ostringstream buffer_stream(std::ios::binary);
	try
	{
		cereal::BinaryOutputArchive archive(buffer_stream);
		archive(*this);
	}
	catch (...)
	{
		DEBUG_ERROR_CATEGORY(logger::categorys::model, "material_resource serialize failed[%s]", std::string(file_path).c_str());
		return false;
	}

	if (!::resource::write_asset_file(*this, ::resource::asset_types::material, file_path, buffer_stream.str()))
		return false;

	set_missing(false);
	DEBUG_LOG_CATEGORY(logger::categorys::model, "material_resource serialize successed[%s]", std::string(file_path).c_str());
	return true;
}

namespace resource
{
	operation_state loader<material_resource>::load(std::string_view file_path)
	{
		std::error_code error;
		if (!std::filesystem::exists(std::filesystem::path(std::string(file_path)), error))
		{
			set_error_message("file not found");
			return operation_state::not_found;
		}

		resource = std::make_shared<material_resource>();
		if (!resource->load_file(file_path))
			return operation_state::deserialize_failed;

		//	参照するテクスチャを依存リソースとして読み込む
		for (auto& [name, shader_resource] : resource->shader_resources)
		{
			for (std::size_t index = 0; index < shader_resource.resources.size(); ++index)
			{
				auto& slot = shader_resource.resources[index];
				if (slot.asset || slot.asset.is_empty())
					continue;

				auto& texture = texture_operations.emplace_back();
				texture.name = name;
				texture.index = index;
				texture.reference = slot.asset.get_reference();
				texture.operation = resource_manager::instance()->load_async(texture.reference, load_priority::high);
				add_dependency(texture.operation);
			}
		}
		return operation_state::success;
	}

	operation_state loader<material_resource>::build()
	{
		assign_textures();
		resource->build();
		return operation_state::success;
	}

	void loader<material_resource>::assign_textures()
	{
		for (auto& texture : texture_operations)
		{
			auto it = resource->shader_resources.find(texture.name);
			if (it == resource->shader_resources.end() || it->second.resources.size() <= texture.index)
				continue;
			it->second.resources[texture.index].asset = resource_manager::instance()->resolve(texture.operation, texture.reference);
		}
		texture_operations.clear();
	}

	material_resource_ptr loader<material_resource>::create_fallback(const asset_reference<material_resource>& reference, material_resource::domains domain)
	{
		return material_resource_factory::create_missing(reference, domain);
	}

	bool loader<material_resource>::revert(material_resource& target)
	{
		if (target.is_missing())
			return false;
		assign_textures();
		resource->duplicate(target);
		return true;
	}

	bool saver<material_resource>::save(material_resource& resource, std::string_view file_path)
	{
		return resource.save_file(file_path);
	}
}	//	namespace resource
