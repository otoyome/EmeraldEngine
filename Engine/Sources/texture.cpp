#include "texture.h"

#include "render_data.h"

#include <sstream>
#include <iomanip>

#include <filesystem>
#include <chrono>
#include <vector>
#include "DirectXTex.h"


#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/unordered_map.hpp"

// CEREALバージョン定義
CEREAL_CLASS_VERSION(texture, 1)

namespace
{
	std::string_view extension = ".meta";

	bool generate_texture2d_core(texture_ptr& out_texture, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA* subresource_desc, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
	{
		HRESULT hr;
		//	テクスチャ
		Microsoft::WRL::ComPtr<ID3D11Texture2D> resource;
		hr = render_data::instance()->get_device()->CreateTexture2D(&desc, subresource_desc, &resource);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
		{
			return false;
		}

		//	シェーダーリソースビュー生成
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
		if (system_utility::is_bitflag(desc.BindFlags, D3D11_BIND_SHADER_RESOURCE))
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc{};
			if (srv_desc)
			{
				srvdesc = *srv_desc;
			}
			else
			{
				srvdesc.Format = desc.Format;
				srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srvdesc.Texture2D.MipLevels = 1;
			}
			hr = render_data::instance()->get_device()->CreateShaderResourceView(resource.Get(), &srvdesc, shader_resource_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			if (FAILED(hr))
			{
				return false;
			}
		}

		//	アンオーダードアクセスビュー生成
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view;
		if (system_utility::is_bitflag(desc.BindFlags, D3D11_BIND_UNORDERED_ACCESS))
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc{};
			if (uav_desc)
			{
				uavdesc = *uav_desc;
			}
			else
			{
				uavdesc.Format = desc.Format;
				uavdesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
				uavdesc.Texture2D.MipSlice = 0;
			}
			hr = render_data::instance()->get_device()->CreateUnorderedAccessView(resource.Get(), &uavdesc, unordered_access_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			if (FAILED(hr))
			{
				return false;
			}
		}

		out_texture = std::make_shared<texture2d>(resource, shader_resource_view, unordered_access_view);
		return true;
	}

	bool load_image_file(const WCHAR* name, DirectX::TexMetadata& tex_meta_data, DirectX::ScratchImage& scratch_image)
	{
		HRESULT	hr;

		//	DDSファイル読み込み
		hr = DirectX::GetMetadataFromDDSFile(name, DirectX::DDS_FLAGS_NONE, tex_meta_data);
		if (SUCCEEDED(hr) &&
			SUCCEEDED(DirectX::LoadFromDDSFile(name, DirectX::DDS_FLAGS_NONE, &tex_meta_data, scratch_image)))
		{
			return true;
		}

		//	HDRファイル読み込み
		hr = DirectX::GetMetadataFromHDRFile(name, tex_meta_data);
		if (SUCCEEDED(hr) &&
			SUCCEEDED(DirectX::LoadFromHDRFile(name, &tex_meta_data, scratch_image)))
		{
			return true;
		}

		//	TGAファイル読み込み
		hr = DirectX::GetMetadataFromTGAFile(name, DirectX::TGA_FLAGS_NONE, tex_meta_data);
		if (SUCCEEDED(hr) &&
			SUCCEEDED(DirectX::LoadFromTGAFile(name, DirectX::TGA_FLAGS_NONE, &tex_meta_data, scratch_image)))
		{
			return true;
		}

		//	WICファイル読み込み
		hr = DirectX::GetMetadataFromWICFile(name, DirectX::WIC_FLAGS_NONE, tex_meta_data);
		if (SUCCEEDED(hr) &&
			SUCCEEDED(DirectX::LoadFromWICFile(name, DirectX::WIC_FLAGS_NONE, &tex_meta_data, scratch_image)))
		{
			return true;
		}

		return false;
	}

	bool load_image_memory(const std::byte* data_buffer, std::size_t data_size, DirectX::TexMetadata& tex_meta_data, DirectX::ScratchImage& scratch_image)
	{
		HRESULT	hr;

		//	DDSファイル読み込み
		hr	= DirectX::GetMetadataFromDDSMemory(data_buffer, data_size, DirectX::DDS_FLAGS_NONE, tex_meta_data);
		if( SUCCEEDED( hr ) &&
			SUCCEEDED( DirectX::LoadFromDDSMemory(data_buffer, data_size, DirectX::DDS_FLAGS_NONE, &tex_meta_data, scratch_image) ) )
		{
			return	true;
		}

		//	HDRファイル読み込み
		hr	= DirectX::GetMetadataFromHDRMemory(data_buffer, data_size, tex_meta_data);
		if( SUCCEEDED( hr ) &&
			SUCCEEDED( DirectX::LoadFromHDRMemory(data_buffer, data_size, &tex_meta_data, scratch_image) ) )
		{
			return	true;
		}

		//	TGAファイル読み込み
		hr	= DirectX::GetMetadataFromTGAMemory(data_buffer, data_size, DirectX::TGA_FLAGS_NONE, tex_meta_data);
		if( SUCCEEDED( hr ) &&
			SUCCEEDED( DirectX::LoadFromTGAMemory(data_buffer, data_size, DirectX::TGA_FLAGS_NONE, &tex_meta_data, scratch_image) ) )
		{
			return	true;
		}

		//	WICファイル読み込み
		hr	= DirectX::GetMetadataFromWICMemory(data_buffer, data_size, DirectX::WIC_FLAGS_NONE, tex_meta_data);
		if( SUCCEEDED( hr ) &&
			SUCCEEDED( DirectX::LoadFromWICMemory(data_buffer, data_size, DirectX::WIC_FLAGS_NONE, &tex_meta_data, scratch_image) ) )
		{
			return	true;
		}

		return	false;
	}

	bool save_image_file(const WCHAR* name, texture_manager::save_texture_desc::data_formats data_format, const DirectX::ScratchImage& scratch_image)
	{
		auto& meta_data = scratch_image.GetMetadata();
		auto images = scratch_image.GetImages();
		auto image_count = scratch_image.GetImageCount();

		HRESULT hr = S_OK;
		std::filesystem::path path = name;
		switch (data_format)
		{
			case texture_manager::save_texture_desc::data_formats::dds:
			{
				path = path.replace_extension(".dds");
				hr = DirectX::SaveToDDSFile(images, image_count, meta_data, DirectX::DDS_FLAGS_NONE, path.wstring().data());
				break;
			}
			case texture_manager::save_texture_desc::data_formats::jpg:
			{
				path = path.replace_extension(".jpg");
				hr = DirectX::SaveToWICFile(images, image_count, DirectX::WIC_FLAGS_NONE,
					DirectX::GetWICCodec(DirectX::WICCodecs::WIC_CODEC_JPEG), path.wstring().data());
				break;
			}
			case texture_manager::save_texture_desc::data_formats::png:
			{
				path = path.replace_extension(".png");
				hr = DirectX::SaveToWICFile(images, image_count, DirectX::WIC_FLAGS_NONE,
					DirectX::GetWICCodec(DirectX::WICCodecs::WIC_CODEC_PNG), path.wstring().data());
				break;
			}
			case texture_manager::save_texture_desc::data_formats::tga:
			{
				path = path.replace_extension(".tga");
				hr = DirectX::SaveToTGAFile(images[0], path.wstring().data());
				break;
			}
			case texture_manager::save_texture_desc::data_formats::hdr:
			{
				path = path.replace_extension(".hdr");
				hr = DirectX::SaveToHDRFile(images[0], path.wstring().data());
				break;
			}
		}
		return SUCCEEDED(hr);
	}

	bool save_image_memory(const WCHAR* name, texture_manager::save_texture_desc::data_formats data_format, const DirectX::ScratchImage& scratch_image, DirectX::Blob& out_blob)
	{
		auto& meta_data = scratch_image.GetMetadata();
		auto images = scratch_image.GetImages();
		auto image_count = scratch_image.GetImageCount();
		switch (data_format)
		{
			case texture_manager::save_texture_desc::data_formats::dds:
			{
				if (FAILED(DirectX::SaveToDDSMemory(images, image_count, meta_data, DirectX::DDS_FLAGS_NONE, out_blob)))
				{
					return false;
				}
				break;
			}
			case texture_manager::save_texture_desc::data_formats::jpg:
			{
				if (FAILED(DirectX::SaveToWICMemory(images, image_count, DirectX::WIC_FLAGS_NONE,
					DirectX::GetWICCodec(DirectX::WICCodecs::WIC_CODEC_JPEG), out_blob)))
				{
					return false;
				}
				break;
			}
			case texture_manager::save_texture_desc::data_formats::png:
			{
				if (FAILED(DirectX::SaveToWICMemory(images, image_count, DirectX::WIC_FLAGS_NONE,
					DirectX::GetWICCodec(DirectX::WICCodecs::WIC_CODEC_PNG), out_blob)))
				{
					return false;
				}
				break;
			}
			case texture_manager::save_texture_desc::data_formats::tga:
			{
				if (FAILED(DirectX::SaveToTGAMemory(images[0], out_blob)))
				{
					return false;
				}
				break;
			}
			case texture_manager::save_texture_desc::data_formats::hdr:
			{
				if (FAILED(DirectX::SaveToHDRMemory(images[0], out_blob)))
				{
					return false;
				}
				break;
			}
		}
		return true;
	}
}

namespace
{
	//	バージョン付きメタデータの識別子
	constexpr uint32_t meta_data_signature = 'E' | ('T' << 8) | ('M' << 16) | ('T' << 24);

	//	アドレスモード変換テーブル
	constexpr D3D11_TEXTURE_ADDRESS_MODE address_mode_table[] =
	{
		D3D11_TEXTURE_ADDRESS_WRAP,
		D3D11_TEXTURE_ADDRESS_MIRROR,
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_TEXTURE_ADDRESS_BORDER,
		D3D11_TEXTURE_ADDRESS_MIRROR_ONCE,
	};
	static_assert(ARRAYSIZE(address_mode_table) == static_cast<size_t>(texture_sampler_desc::address_modes::max));

	D3D11_TEXTURE_ADDRESS_MODE to_d3d11_address_mode(texture_sampler_desc::address_modes mode)
	{
		size_t index = static_cast<size_t>(mode);
		return index < ARRAYSIZE(address_mode_table) ? address_mode_table[index] : D3D11_TEXTURE_ADDRESS_WRAP;
	}

	//	サンプラー設定の読み書き（列挙型は幅を固定するため uint32_t で扱う）
	template<class Archive>
	void save_sampler_desc(Archive& archive, const texture_sampler_desc& desc)
	{
		archive(
			static_cast<uint32_t>(desc.filter),
			static_cast<uint32_t>(desc.address_u),
			static_cast<uint32_t>(desc.address_v),
			static_cast<uint32_t>(desc.address_w),
			desc.max_anisotropy,
			desc.mip_lod_bias,
			desc.min_lod,
			desc.max_lod,
			desc.border_color[0], desc.border_color[1], desc.border_color[2], desc.border_color[3]);
	}

	template<class Archive>
	void load_sampler_desc(Archive& archive, texture_sampler_desc& desc)
	{
		uint32_t filter = 0, address_u = 0, address_v = 0, address_w = 0;
		archive(
			filter,
			address_u,
			address_v,
			address_w,
			desc.max_anisotropy,
			desc.mip_lod_bias,
			desc.min_lod,
			desc.max_lod,
			desc.border_color[0], desc.border_color[1], desc.border_color[2], desc.border_color[3]);

		auto to_filter = [](uint32_t value)
		{
			return value < static_cast<uint32_t>(texture_sampler_desc::filters::max) ? static_cast<texture_sampler_desc::filters>(value) : texture_sampler_desc::filters::linear;
		};
		auto to_address = [](uint32_t value)
		{
			return value < static_cast<uint32_t>(texture_sampler_desc::address_modes::max) ? static_cast<texture_sampler_desc::address_modes>(value) : texture_sampler_desc::address_modes::wrap;
		};
		desc.filter = to_filter(filter);
		desc.address_u = to_address(address_u);
		desc.address_v = to_address(address_v);
		desc.address_w = to_address(address_w);
	}
}

D3D11_SAMPLER_DESC texture_sampler_desc::to_d3d11_desc() const
{
	D3D11_SAMPLER_DESC desc{};
	switch (filter)
	{
		case filters::point:		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;	break;
		case filters::anisotropic:	desc.Filter = D3D11_FILTER_ANISOTROPIC;			break;
		case filters::linear:
		default:					desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	break;
	}
	desc.AddressU = to_d3d11_address_mode(address_u);
	desc.AddressV = to_d3d11_address_mode(address_v);
	desc.AddressW = to_d3d11_address_mode(address_w);
	desc.MipLODBias = mip_lod_bias;
	desc.MaxAnisotropy = std::clamp<UINT>(max_anisotropy, 1, D3D11_REQ_MAXANISOTROPY);
	desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	for (size_t index = 0; index < border_color.size(); ++index)
	{
		desc.BorderColor[index] = border_color[index];
	}
	desc.MinLOD = min_lod;
	desc.MaxLOD = max_lod;
	return desc;
}

bool texture_meta_data::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			//	@remainder	本来は不要な対応。識別子とバージョンを先頭に書く
			archive(meta_data_signature, current_version);
			archive(CEREAL_NVP(type), CEREAL_NVP(system_flags));
			save_sampler_desc(archive, sampler);
		}
		catch (...)
		{

			DEBUG_LOG(file_path.data());
			assert(!"texture serialize failed.\n");
			return false;
		}

		return true;
	}
	return false;
}

bool texture_meta_data::deserialize(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if (istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			//	識別子が無ければ旧形式として先頭から読み直す
			uint32_t signature = 0;
			archive(signature);

			uint32_t version = 0;
			if (signature == meta_data_signature)
			{
				archive(version);
				if (version > current_version)
				{
					DEBUG_ERROR("texture_meta_data unknown version[%s][%u]", file_path.data(), version);
					return false;
				}
			}
			else
			{
				istream.clear();
				istream.seekg(0, std::ios::beg);
			}

			archive(CEREAL_NVP(type), CEREAL_NVP(system_flags));

			sampler = texture_sampler_desc();
			if (version >= 1)
			{
				load_sampler_desc(archive, sampler);
			}
		}
		catch (...)
		{
			DEBUG_ERROR("texture_meta_data deserialize failed[%s]", file_path.data());
			return false;
		}

		DEBUG_LOG("texture_meta_data deserialize succeeded[%s]", file_path.data());
		return true;
	}
	DEBUG_ERROR("texture_meta_data open failed[%s]", file_path.data());
	return false;
}

//	毎回定義するのが面倒なのでマクロにしておく
#define	RETURN_SEARCH_FILE(KEY_NAME)			\
auto	key_name	= std::string(KEY_NAME);	\
auto	it	= textures.find(key_name);			\
if (it != textures.end()){						\
	auto ptr = it->second.lock();				\
	if(ptr){									\
		out_texture = ptr;						\
		return	true;							\
	}											\
	textures.erase(it);							\
}

D3D11_RESOURCE_DIMENSION texture::get_dimension() const
{
	return dimension;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture::get_shader_resource_view() const
{
	const std::shared_ptr<const view_set> current = views.load();
	return current ? current->shader_resource_view : nullptr;
}

Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> texture::get_unordered_access_view() const
{
	const std::shared_ptr<const view_set> current = views.load();
	return current ? current->unordered_access_view : nullptr;
}

texture_data_types texture::get_type() const
{
	return type;
}

void texture::set_type(texture_data_types type)
{
	this->type = type;
}

bool texture::is_file_path_or_key() const
{
	return this->is_file;
}

std::string_view texture::get_file_path_or_key() const
{
	return	this->file_path_or_key;
}

void texture::set_file_path(std::string_view file_path)
{
	this->is_file = true;
	this->file_path_or_key = file_path;
}

void texture::set_data_key(std::string_view key)
{
	this->is_file = false;
	this->file_path_or_key = key;
}

texture::texture(D3D11_RESOURCE_DIMENSION dimension, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view)
: is_file(false)
, file_path_or_key()
, type(texture_data_types::color)
, system_flags(0)
, dimension(dimension)
, views(std::make_shared<const view_set>(view_set{ shader_resource_view, unordered_access_view }))
, sampler_desc()
{
	set_flag(flags::srgb, false);
}

#if defined(USE_IMGUI)
bool texture::is_reload()
{
	if (!is_file)
		return false;

	std::error_code error;

	//	ファイルが存在しない場合
	if (!std::filesystem::exists(file_path_or_key, error) || error)
		return false;

	//	最終更新日時と違う場合は再読み込みが必要
	auto write_time = std::filesystem::last_write_time(file_path_or_key, error);
	if (error)
		return false;

	return last_write_time != write_time;
}

void texture::reload()
{
	texture_ptr fresh;
	if (texture_manager::instance()->decode_texture_from_file(fresh, file_path_or_key))
	{
		//	@tool ビュー一式を差し替える（描画側が保持している古いビューは、使い終わるまで解放されない）
		this->views.store(fresh->views.load());
		this->dimension = fresh->dimension;
		this->type = fresh->type;
		this->system_flags = fresh->system_flags;
		this->sampler_desc = fresh->sampler_desc;
		++revision;
		on_reload();
		update_last_write_time();
	}
}

#endif	//	defined(USE_IMGUI)

void texture::update_last_write_time()
{
#if defined(USE_IMGUI)
	if (file_path_or_key.empty())
		return;

	std::error_code error;
	auto write_time = std::filesystem::last_write_time(file_path_or_key, error);
	if (!error)
		last_write_time = write_time;
#endif	//	defined(USE_IMGUI)
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
texture_manager::texture_manager()
{
	//	ダミーテクスチャ生成
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 16;
		desc.Height = 16;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		generate_texture2d(white_texture2d, white_texture2d_name, desc, 0xFFFFFFFF);
		generate_texture2d(black_texture2d, black_texture2d_name, desc, 0xFF000000);
	}
}

texture_manager::~texture_manager()
{
}

void texture_manager::check_update_and_reload()
{
#if defined(USE_IMGUI)
	//	時間基準で間引く
	const auto now = std::chrono::steady_clock::now();
	if (now - last_check_time < std::chrono::milliseconds(500))
		return;
	last_check_time = now;

	std::vector<texture_ptr> alive;
	alive.reserve(textures.size());
	for (auto it = textures.begin(); it != textures.end(); )
	{
		auto ptr = it->second.lock();
		if (!ptr)
		{
			it = textures.erase(it);
			continue;
		}
		alive.emplace_back(std::move(ptr));
		++it;
	}

	//	ファイルの更新チェック
	for (auto& ptr : alive)
	{
		if (ptr->is_reload())
		{
			//	@tool 描画中のフレームがテクスチャの情報（大きさなど）を参照しているので、描き終わるのを待つ
			render_data::instance()->flush_rendering();
			ptr->reload();
		}
	}
#endif	//	defined(USE_IMGUI)
}

bool texture_manager::generate_texture2d(texture_ptr& out_texture, std::string_view key, D3D11_TEXTURE2D_DESC desc, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	//	存在すればそちらを渡す
	RETURN_SEARCH_FILE(key)

	if(generate_texture2d_core(out_texture, desc, nullptr, srv_desc, uav_desc))
	{
		out_texture->set_data_key(key);
		textures.insert(make_pair(key_name, out_texture));
		return true;
	}
	out_texture	= nullptr;
	return false;
}

bool texture_manager::generate_unregistered_texture2d(texture_ptr& out_texture, D3D11_TEXTURE2D_DESC desc, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	if (generate_texture2d_core(out_texture, desc, nullptr, srv_desc, uav_desc))
		return true;
	out_texture = nullptr;
	return false;
}

bool texture_manager::generate_texture2d(texture_ptr& out_texture, std::string_view key, D3D11_TEXTURE2D_DESC desc, DWORD init_value/*0xAABBGGRR*/, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	//	存在すればそちらを渡す
	RETURN_SEARCH_FILE(key)

	std::unique_ptr<DWORD[]> sysmem;
	size_t texels = desc.Width * desc.Height;
	sysmem = std::make_unique<DWORD[]>(texels);
	for (size_t i = 0; i < texels; ++i)
	{
		sysmem[i] = init_value;
	}

	D3D11_SUBRESOURCE_DATA subresource_data{};
	subresource_data.pSysMem = sysmem.get();
	subresource_data.SysMemPitch = sizeof(DWORD) * desc.Width;
	subresource_data.SysMemSlicePitch = 0;

	if(generate_texture2d_core(out_texture, desc, &subresource_data, srv_desc, uav_desc))
	{
		out_texture->set_data_key(key);
		textures.insert(make_pair(key_name, out_texture));
		return true;
	}
	out_texture	= nullptr;
	return false;
}

bool texture_manager::generate_texture2d(texture_ptr& out_texture, std::string_view key, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA subresource_data, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	//	存在すればそちらを渡す
	RETURN_SEARCH_FILE(key)

	if (generate_texture2d_core(out_texture, desc, &subresource_data, srv_desc, uav_desc))
	{
		out_texture->set_data_key(key);
		textures.insert(make_pair(key_name, out_texture));
		return true;
	}
	out_texture = nullptr;
	return false;
}

bool texture_manager::load_texture_from_file(texture_ptr& out_texture, std::string_view file_path, bool force_load)
{
	//	存在すればそれを渡す
	if (!force_load)
	{
		RETURN_SEARCH_FILE(file_path)
	}
	auto key_name = std::string(file_path);

	if (!load_texture_from_file_inner(out_texture, file_path))
		return false;

	textures[key_name] = out_texture;
	return true;
}

void texture_manager::load_textures_from_files(std::vector<texture_ptr>& out_textures, const std::vector<std::string_view>& file_pathes, bool force_load)
{
	out_textures.assign(file_pathes.size(), nullptr);

	//	必要な物だけを並列化
	struct pending_load
	{
		std::string path;
		std::vector<std::size_t> out_indices;
	};
	std::unordered_map<std::string, std::size_t> path_to_pending;
	std::vector<pending_load> pending_loads;
	for (std::size_t index = 0; index < file_pathes.size(); ++index)
	{
		auto key_name = std::string(file_pathes[index]);

		if (!force_load)
		{
			auto it = textures.find(key_name);
			if (it != textures.end())
			{
				auto ptr = it->second.lock();
				if (ptr)
				{
					out_textures[index] = ptr;
					continue;
				}
				textures.erase(it);
			}
		}

		auto pending_it = path_to_pending.find(key_name);
		if (pending_it == path_to_pending.end())
		{
			path_to_pending.emplace(key_name, pending_loads.size());
			pending_loads.push_back({ key_name, { index } });
		}
		else
		{
			pending_loads[pending_it->second].out_indices.push_back(index);
		}
	}

	if (pending_loads.empty())
		return;

	std::vector<texture_ptr> loaded_textures(pending_loads.size());

	load_parallel_executor.parallel_for(pending_loads.size(), 1,
		[&](std::size_t, std::size_t begin, std::size_t end)
		{
			thread_local bool com_initialized = false;
			if (!com_initialized)
			{
				CoInitializeEx(nullptr, COINIT_MULTITHREADED);
				com_initialized = true;
			}

			for (std::size_t i = begin; i < end; ++i)
				load_texture_from_file_inner(loaded_textures[i], pending_loads[i].path);
		});

	for (std::size_t i = 0; i < pending_loads.size(); ++i)
	{
		if (!loaded_textures[i])
			continue;

		textures[pending_loads[i].path] = loaded_textures[i];
		for (auto out_index : pending_loads[i].out_indices)
			out_textures[out_index] = loaded_textures[i];
	}
}

bool texture_manager::load_texture_from_file_inner(texture_ptr& out_texture, std::string_view file_path)
{
	HRESULT hr;
	std::filesystem::path path(file_path);
	DirectX::TexMetadata tex_meta_data;
	DirectX::ScratchImage scratch_image;
	if (!load_image_file(path.wstring().data(), tex_meta_data, scratch_image))
	{
		return false;
	}

	//	ミップマップ生成
//	if (generate_mipmap)
	if (false)
	{
		DirectX::ScratchImage	mip_chain;
		hr = DirectX::GenerateMipMaps(scratch_image.GetImages(),
									  scratch_image.GetImageCount(),
									  scratch_image.GetMetadata(),
									  DirectX::TEX_FILTER_DEFAULT,
									  0,	//	ミップマップを全て作る
									  mip_chain);
		if (SUCCEEDED(hr))
		{
			scratch_image = std::move(mip_chain);
		}
	}

	//	シェーダーリソースビューの生成
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	hr	= DirectX::CreateShaderResourceView(render_data::instance()->get_device().Get(),
											scratch_image.GetImages(),
											scratch_image.GetImageCount(),
											scratch_image.GetMetadata(),
											shader_resource_view.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
	{
		out_texture = nullptr;
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	shader_resource_view->GetResource(resource.GetAddressOf());

	D3D11_RESOURCE_DIMENSION dimension;
	resource->GetType(&dimension);
	if (dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
	{
		assert(!"[texture]unknown type.");
		out_texture = nullptr;
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> resource_data;
	hr = resource.As<ID3D11Texture2D>(&resource_data);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
	{
		out_texture = nullptr;
		return false;
	}

	out_texture = std::make_shared<texture2d>(resource_data, shader_resource_view);
	out_texture->set_file_path(file_path);
	out_texture->update_last_write_time();

	//	メタデータがあれば読み込み
	{
		path.replace_extension(extension);
		if (std::filesystem::exists(path))
		{
			texture_meta_data meta_data;
			if (meta_data.deserialize(path.string().data()))
			{
				out_texture->set_type(meta_data.type);
				out_texture->set_system_flags(meta_data.system_flags);
				out_texture->set_sampler_desc(meta_data.sampler);
			}
		}
	}
	return true;
}

bool texture_manager::load_texture_from_memory(texture_ptr& out_texture, std::string_view key, const void* data, size_t size)
{
	//	存在すればそちらを渡す
	RETURN_SEARCH_FILE(key)

	HRESULT hr;
	DirectX::TexMetadata tex_meta_data;
	DirectX::ScratchImage scratch_image;
	if (!load_image_memory(reinterpret_cast<const std::byte*>(data), size, tex_meta_data, scratch_image))
	{
		return false;
	}

	//	ミップマップ生成
//	if (generate_mipmap)
	if (false)
	{
		DirectX::ScratchImage	mip_chain;
		hr = DirectX::GenerateMipMaps(scratch_image.GetImages(),
									  scratch_image.GetImageCount(),
									  scratch_image.GetMetadata(),
									  DirectX::TEX_FILTER_DEFAULT,
									  0,	//	ミップマップをすべて作る
									  mip_chain);
		if (SUCCEEDED(hr))
		{
			scratch_image = std::move(mip_chain);
		}
	}

	//	シェーダーリソースビューの生成
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	hr	= DirectX::CreateShaderResourceView(render_data::instance()->get_device().Get(),
											scratch_image.GetImages(),
											scratch_image.GetImageCount(),
											scratch_image.GetMetadata(),
											shader_resource_view.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if(SUCCEEDED(hr))
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		shader_resource_view->GetResource(resource.GetAddressOf());

		D3D11_RESOURCE_DIMENSION dimension;
		resource->GetType(&dimension);
		switch(dimension)
		{
			case	D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> resource_data;
				hr	= resource.As<ID3D11Texture2D>(&resource_data);
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
				if(SUCCEEDED(hr))
				{
					out_texture	= std::make_shared<texture2d>(resource_data, shader_resource_view);
					textures.insert(make_pair(key_name, out_texture));
					return	true;
				}
				break;
			}
			//	とりあえず未対応
			case	D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			case	D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			default:
			{
				assert(!"[texture]unknown type.");
			}
		}
	}
	out_texture	= nullptr;
	return false;
}

texture_ptr texture_manager::search(std::string_view name)
{
	auto key_name = std::string(name);
	auto it = textures.find(key_name);
	if (it != textures.end())
	{
		auto ptr = it->second.lock();
		if (!ptr)
		{
			textures.erase(it);
			return	nullptr;
		}
		return	ptr;
	}

	return	nullptr;
}

texture_manager::texture_map texture_manager::get_textures() const
{
	return textures;
}

bool texture_manager::save_texture(const save_texture_desc& desc)
{
	std::filesystem::path texture_filepath(desc.file_path);

	std::vector<DirectX::Image> images;
	for (auto& desc_image : desc.images)
	{
		auto& image = images.emplace_back();
		image.width = desc_image.width;
		image.height = desc_image.height;
		image.pixels = const_cast<uint8_t*>(desc_image.pixels);
		image.format = desc.format;
		image.rowPitch = graphics_utility::get_byte_offset_from_format(image.format) * image.width;
		image.slicePitch = image.rowPitch * image.height;
	}

	DirectX::ScratchImage scratch_image;
	if (desc.misc == save_texture_desc::miscs::_cube)
	{
		scratch_image.InitializeCubeFromImages(images.data(), images.size());
	}
	else
	{
		switch (desc.dimension)
		{
			case save_texture_desc::dimensions::_1d:
			{
				scratch_image.InitializeArrayFromImages(images.data(), images.size(), true);
				break;
			}
			case save_texture_desc::dimensions::_2d:
			{
				scratch_image.InitializeArrayFromImages(images.data(), images.size());
				break;
			}
			case save_texture_desc::dimensions::_3d:
			{
				scratch_image.Initialize3DFromImages(images.data(), images.size());
				break;
			}
		}
	}
	return save_image_file(texture_filepath.wstring().data(), desc.data_format, scratch_image);
}

bool texture_manager::save_resource(ID3D11Resource* resource, std::string_view file_path, save_texture_desc::data_formats data_format)
{
	if (!resource)
		return false;

	auto device = render_data::instance()->get_device();
	auto immediate_context = render_data::instance()->get_immediate_context();
	//	@tool immediate context は提出スレッドと共有しているので排他する
	std::lock_guard<std::recursive_mutex> immediate_lock(render_data::instance()->get_immediate_context_mutex());

	DirectX::ScratchImage image;
	if (FAILED(DirectX::CaptureTexture(device.Get(), immediate_context.Get(), resource, image)))
		return false;

	//	@tool バックバッファのアルファは不定なので、画像として見やすいようアルファを捨てた形式にする
	const DXGI_FORMAT format = image.GetMetadata().format;
	if (data_format != save_texture_desc::data_formats::dds && !DirectX::IsCompressed(format) && DirectX::HasAlpha(format))
	{
		DirectX::ScratchImage converted;
		if (SUCCEEDED(DirectX::Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DXGI_FORMAT_B8G8R8X8_UNORM, DirectX::TEX_FILTER_DEFAULT | DirectX::TEX_FILTER_FORCE_NON_WIC, DirectX::TEX_THRESHOLD_DEFAULT, converted)))
		{
			image = std::move(converted);
		}
	}

	//	@tool 保存先のフォルダが無ければ作る
	std::filesystem::path path = file_path;
	std::error_code error;
	if (path.has_parent_path())
		std::filesystem::create_directories(path.parent_path(), error);

	return save_image_file(path.wstring().data(), data_format, image);
}

bool texture_manager::save_texture(const texture_ptr& texture, bool save_row_texture)
{
	std::filesystem::path file_path = texture->get_file_path_or_key();

	bool save_succeeded = true;
	if (save_row_texture)
	{
		auto device = render_data::instance()->get_device();
		auto immediate_context = render_data::instance()->get_immediate_context();
		//	@tool immediate context は提出スレッドと共有しているので排他する
		std::lock_guard<std::recursive_mutex> immediate_lock(render_data::instance()->get_immediate_context_mutex());

		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		texture->get_shader_resource_view()->GetResource(resource.GetAddressOf());

		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(device.Get(), immediate_context.Get(), resource.Get(), image)))
			return false;

		//	テクスチャ保存
		save_succeeded = save_image_file(file_path.wstring().data(), texture_manager::save_texture_desc::data_formats::dds, image);
	}

	if (save_succeeded)
	{
		texture_meta_data meta_data;
		meta_data.type = texture->get_type();
		meta_data.system_flags = texture->get_system_flags();
		meta_data.sampler = texture->get_sampler_desc();

		file_path.replace_extension(extension);
		return meta_data.serialize(file_path.string());
	}

	return false;
}

#if defined(USE_IMGUI)
bool texture_manager::decode_texture_from_file(texture_ptr& out_texture, std::string_view file_path)
{
	return load_texture_from_file_inner(out_texture, file_path);
}

namespace
{
	//	最大解像度に収まるよう縮小
	HRESULT fit_to_max_size(DirectX::ScratchImage& image, size_t max_size, bool& out_changed)
	{
		out_changed = false;
		const DirectX::TexMetadata meta = image.GetMetadata();
		if (max_size == 0 || (meta.width <= max_size && meta.height <= max_size))
			return S_OK;
		if (meta.dimension == DirectX::TEX_DIMENSION_TEXTURE3D)
			return E_NOTIMPL;

		auto mip_size = [](size_t size, size_t level) { return std::max<size_t>(size >> level, 1); };

		//	収まるミップ段を探す
		size_t skip = 0;
		while (skip < meta.mipLevels && (mip_size(meta.width, skip) > max_size || mip_size(meta.height, skip) > max_size))
			++skip;

		if (skip < meta.mipLevels)
		{
			DirectX::TexMetadata dropped_meta = meta;
			dropped_meta.width = mip_size(meta.width, skip);
			dropped_meta.height = mip_size(meta.height, skip);
			dropped_meta.mipLevels = meta.mipLevels - skip;

			DirectX::ScratchImage dropped;
			HRESULT hr = dropped.Initialize(dropped_meta);
			if (FAILED(hr))
				return hr;

			for (size_t item = 0; item < meta.arraySize; ++item)
			{
				for (size_t mip = 0; mip < dropped_meta.mipLevels; ++mip)
				{
					const DirectX::Image* source = image.GetImage(mip + skip, item, 0);
					const DirectX::Image* destination = dropped.GetImage(mip, item, 0);
					if (!source || !destination || source->slicePitch != destination->slicePitch)
						return E_UNEXPECTED;
					memcpy(destination->pixels, source->pixels, source->slicePitch);
				}
			}
			image = std::move(dropped);
			out_changed = true;
			return S_OK;
		}

		//	ミップが足りない場合は作り直す
		const bool is_compressed = DirectX::IsCompressed(meta.format);
		DirectX::ScratchImage decompressed;
		const DirectX::ScratchImage* source = &image;
		if (is_compressed)
		{
			HRESULT hr = DirectX::Decompress(image.GetImages(), image.GetImageCount(), meta, DXGI_FORMAT_UNKNOWN, decompressed);
			if (FAILED(hr))
				return hr;
			source = &decompressed;
		}

		//	アスペクト比を保って縮小
		const double scale = std::min(static_cast<double>(max_size) / meta.width, static_cast<double>(max_size) / meta.height);
		size_t width = std::max<size_t>(static_cast<size_t>(meta.width * scale), 1);
		size_t height = std::max<size_t>(static_cast<size_t>(meta.height * scale), 1);
		if (is_compressed)
		{
			width = std::max<size_t>(width & ~size_t(3), 4);
			height = std::max<size_t>(height & ~size_t(3), 4);
		}

		std::vector<DirectX::Image> top_images;
		for (size_t item = 0; item < meta.arraySize; ++item)
			top_images.push_back(*source->GetImage(0, item, 0));
		DirectX::TexMetadata top_meta = source->GetMetadata();
		top_meta.mipLevels = 1;

		DirectX::ScratchImage resized;
		HRESULT hr = DirectX::Resize(top_images.data(), top_images.size(), top_meta, width, height, DirectX::TEX_FILTER_DEFAULT | DirectX::TEX_FILTER_FORCE_NON_WIC, resized);
		if (FAILED(hr))
			return hr;

		if (meta.mipLevels > 1)
		{
			DirectX::ScratchImage mip_chain;
			hr = DirectX::GenerateMipMaps(resized.GetImages(), resized.GetImageCount(), resized.GetMetadata(), DirectX::TEX_FILTER_DEFAULT | DirectX::TEX_FILTER_FORCE_NON_WIC, 0, mip_chain);
			if (FAILED(hr))
				return hr;
			resized = std::move(mip_chain);
		}

		if (is_compressed)
		{
			DirectX::ScratchImage compressed;
			hr = DirectX::Compress(resized.GetImages(), resized.GetImageCount(), resized.GetMetadata(),
								   meta.format, DirectX::TEX_COMPRESS_PARALLEL, DirectX::TEX_THRESHOLD_DEFAULT, compressed);
			if (FAILED(hr))
				return hr;
			resized = std::move(compressed);
		}

		image = std::move(resized);
		out_changed = true;
		return S_OK;
	}
}

bool texture_manager::limit_dds_resolution(std::string_view dds_file_path, UINT max_size, bool* out_changed)
{
	if (out_changed)
		*out_changed = false;

	std::filesystem::path path(dds_file_path);
	DirectX::TexMetadata tex_meta_data;
	DirectX::ScratchImage scratch_image;
	if (FAILED(DirectX::LoadFromDDSFile(path.wstring().data(), DirectX::DDS_FLAGS_NONE, &tex_meta_data, scratch_image)))
		return false;

	bool changed = false;
	if (FAILED(fit_to_max_size(scratch_image, max_size, changed)))
		return false;
	if (!changed)
		return true;

	std::filesystem::path temp_path = path;
	temp_path += L".tmp";
	HRESULT hr = DirectX::SaveToDDSFile(scratch_image.GetImages(), scratch_image.GetImageCount(), scratch_image.GetMetadata(),
										DirectX::DDS_FLAGS_NONE, temp_path.wstring().data());
	if (FAILED(hr))
		return false;

	std::error_code error;
	std::filesystem::rename(temp_path, path, error);
	if (error)
	{
		std::filesystem::remove(temp_path, error);
		return false;
	}

	if (out_changed)
		*out_changed = true;
	return true;
}

bool texture_manager::convert_texture_to_dds(std::string_view source_file_path, std::string_view dds_file_path, bool generate_mipmaps, UINT max_size)
{
	std::filesystem::path source_path(source_file_path);

	DirectX::TexMetadata tex_meta_data;
	DirectX::ScratchImage scratch_image;
	if (!load_image_file(source_path.wstring().data(), tex_meta_data, scratch_image))
		return false;

	//	最大解像度に収まるよう縮小
	{
		bool changed = false;
		if (FAILED(fit_to_max_size(scratch_image, max_size, changed)))
			return false;
	}

	if (generate_mipmaps)
	{
		DirectX::ScratchImage mip_chain;
		HRESULT hr = DirectX::GenerateMipMaps(scratch_image.GetImages(),
											  scratch_image.GetImageCount(),
											  scratch_image.GetMetadata(),
											  DirectX::TEX_FILTER_DEFAULT,
											  0,	//	ミップマップを全て作る
											  mip_chain);
		if (SUCCEEDED(hr))
			scratch_image = std::move(mip_chain);
	}

	//	BC圧縮(アルファを実際に使っていればBC3、そうでなければBC1)
	if (!DirectX::IsCompressed(scratch_image.GetMetadata().format))
	{
		bool use_alpha = false;
		if (DirectX::HasAlpha(scratch_image.GetMetadata().format))
		{
			DirectX::EvaluateImage(*scratch_image.GetImage(0, 0, 0), [&use_alpha](const DirectX::XMVECTOR* pixels, size_t width, size_t)
			{
				if (use_alpha)
					return;
				for (size_t x = 0; x < width; ++x)
				{
					if (DirectX::XMVectorGetW(pixels[x]) < 0.999f)
					{
						use_alpha = true;
						break;
					}
				}
			});
		}

		DXGI_FORMAT compress_format = use_alpha ? DXGI_FORMAT_BC3_UNORM : DXGI_FORMAT_BC1_UNORM;
		DirectX::ScratchImage compressed_image;
		HRESULT compress_hr = DirectX::Compress(scratch_image.GetImages(), scratch_image.GetImageCount(), scratch_image.GetMetadata(),
			compress_format, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, compressed_image);
		if (SUCCEEDED(compress_hr))
			scratch_image = std::move(compressed_image);
	}

	std::filesystem::path dds_path(dds_file_path);
	dds_path.replace_extension(".dds");

	HRESULT hr = DirectX::SaveToDDSFile(scratch_image.GetImages(),
										scratch_image.GetImageCount(),
										scratch_image.GetMetadata(),
										DirectX::DDS_FLAGS_NONE,
										dds_path.wstring().data());
	return SUCCEEDED(hr);
}

#endif	//	defined(USE_IMGUI)

//////////////////////////////////////////////////////////////////////////////////////////////////////////
texture2d::texture2d(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view)
: texture(D3D11_RESOURCE_DIMENSION_TEXTURE2D, shader_resource_view, unordered_access_view)
{
	texture->GetDesc(&desc);

	//	初期設定
	set_flag(flags::cube_map, desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE);
}

texture2d::~texture2d()
{
}

#if defined(USE_IMGUI)
void texture2d::on_reload()
{
	//	Desc更新
	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	get_shader_resource_view()->GetResource(resource.GetAddressOf());
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	resource.As(&texture);

	texture->GetDesc(&desc);
}
#endif	//	defined(USE_IMGUI)

#undef	RETURN_SEARCH_FILE

