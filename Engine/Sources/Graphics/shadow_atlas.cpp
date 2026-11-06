#include "graphics/shadow_atlas.h"
#include "texture_factory.h"

#include "render_data.h"
#include "common_pipeline_states.h"
#include "System/console_manager.h"

#include <algorithm>
#include <string>

namespace
{
	//	コンソール変数
	console_variable<bool> enable_variable("r.shadow.atlas.enable", true, "影を落とす点光源/スポットライトのシャドウマップをシャドウマップアトラスにまとめる", console_flags::user_setting);
	console_variable<int> resolution_variable("r.shadow.atlas.resolution", 512, "シャドウマップアトラスの1スライスの解像度", console_flags::user_setting);
	console_variable<int> slices_variable("r.shadow.atlas.slices", 64, "シャドウマップアトラスのスライス数（点光源は6、スポットライトは1使う）", console_flags::user_setting);
	console_variable<int> max_lights_variable("r.shadow.atlas.max_lights", 32, "シャドウマップアトラスを割り当てるライトの最大数（入らないライトは従来の共有シャドウマップで描く）", console_flags::user_setting);
	console_variable<bool> cache_variable("r.shadow.atlas.cache", true, "ライトや影を落とすメッシュが変わっていなければ影を描き直さない", console_flags::user_setting);
	console_variable<int> keep_frames_variable("r.shadow.atlas.keep_frames", 120, "使われなくなったライトのスライスを持ち続けるフレーム数", console_flags::user_setting);
	bool get_enable() { return enable_variable.get(); }
	UINT get_resolution() { return static_cast<UINT>(std::clamp(resolution_variable.get(), 64, 4096)); }
	UINT get_slices() { return static_cast<UINT>(std::clamp(slices_variable.get(), 6, 2048)); }
	UINT get_max_lights_value() { return static_cast<UINT>((std::max)(max_lights_variable.get(), 0)); }
	bool get_cache() { return cache_variable.get(); }
	std::uint64_t get_keep_frames() { return static_cast<std::uint64_t>((std::max)(keep_frames_variable.get(), 0)); }

	shadow_atlas::statistics last_statistics;
}

bool shadow_atlas::is_enable()
{
	return get_enable();
}

UINT shadow_atlas::get_max_lights()
{
	return get_max_lights_value();
}

const shadow_atlas::statistics& shadow_atlas::get_last_statistics()
{
	return last_statistics;
}

void shadow_atlas::begin_frame()
{
	++frame;
	frame_statistics = {};
	if (!is_enable())
		return;

	const UINT desired_resolution = get_resolution();
	const UINT desired_slices = get_slices();
	if (texture && resolution == desired_resolution && slice_count == desired_slices)
		return;

	//	作り直し
	D3D11_TEXTURE2D_DESC texture_desc{};
	texture_desc.Width = desired_resolution;
	texture_desc.Height = desired_resolution;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = desired_slices;
	texture_desc.Format = common_pipeline_states::shadow_map_texture_format;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	srv_desc.Format = common_pipeline_states::shadow_map_shader_resource_view_format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srv_desc.Texture2DArray.MipLevels = 1;
	srv_desc.Texture2DArray.FirstArraySlice = 0;
	srv_desc.Texture2DArray.ArraySize = desired_slices;

	const std::string key = "shadow_atlas_" + std::to_string(desired_resolution) + "_" + std::to_string(desired_slices);
	depth_stencil_views.clear();
	texture.reset();
	texture_factory::create_texture2d(texture, key, texture_desc, &srv_desc);

	resolution = desired_resolution;
	slice_count = desired_slices;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(resolution);
	viewport.Height = static_cast<float>(resolution);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	//	中身が無くなったのでキャッシュも捨てる
	entries.clear();
	slice_owners.assign(slice_count, 0u);
}

bool shadow_atlas::find_free_slices(UINT count, UINT& out_first_slice) const
{
	//	連続した空きスライスを先頭から探す
	UINT run = 0;
	for (UINT slice = 0; slice < slice_count; ++slice)
	{
		run = (slice_owners[slice] == 0) ? run + 1 : 0;
		if (run == count)
		{
			out_first_slice = slice + 1 - count;
			return true;
		}
	}
	return false;
}

void shadow_atlas::release(UINT light_id)
{
	auto it = entries.find(light_id);
	if (it == entries.end())
		return;

	for (UINT slice = it->second.first_slice; slice < it->second.first_slice + it->second.slice_count && slice < slice_count; ++slice)
		slice_owners[slice] = 0;
	entries.erase(it);
}

bool shadow_atlas::acquire(UINT light_id, UINT count, std::uint64_t signature, bool force_render, UINT& out_first_slice, bool& out_needs_render)
{
	if (!texture || light_id == 0 || count == 0)
		return false;

	//	持ち続けているスライスがあればそのまま使う
	auto it = entries.find(light_id);
	if (it != entries.end() && it->second.slice_count == count)
	{
		entry& current = it->second;
		out_needs_render = force_render || !get_cache() || current.signature != signature;
		current.signature = signature;
		current.last_used_frame = frame;
		out_first_slice = current.first_slice;
		++frame_statistics.allocated_lights;
		if (out_needs_render)
			++frame_statistics.rendered_lights;
		return true;
	}
	if (it != entries.end())
		release(light_id);

	//	空きが無ければ、今回使っていないライトを使われていない順に解放する
	UINT first_slice = 0;
	if (!find_free_slices(count, first_slice))
	{
		std::vector<std::pair<std::uint64_t, UINT>> unused;
		for (const auto& pair : entries)
		{
			if (pair.second.last_used_frame < frame)
				unused.emplace_back(pair.second.last_used_frame, pair.first);
		}
		std::sort(unused.begin(), unused.end());

		bool found = false;
		for (const auto& candidate : unused)
		{
			release(candidate.second);
			if (find_free_slices(count, first_slice))
			{
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	for (UINT slice = first_slice; slice < first_slice + count; ++slice)
		slice_owners[slice] = light_id;

	entry& created = entries[light_id];
	created.first_slice = first_slice;
	created.slice_count = count;
	created.signature = signature;
	created.last_used_frame = frame;

	out_first_slice = first_slice;
	out_needs_render = true;
	++frame_statistics.allocated_lights;
	++frame_statistics.rendered_lights;
	return true;
}

void shadow_atlas::end_frame()
{
	//	しばらく使われていないライトのスライスを解放する
	const std::uint64_t keep_frames = get_keep_frames();
	std::vector<UINT> expired;
	for (const auto& pair : entries)
	{
		if (frame - pair.second.last_used_frame > keep_frames)
			expired.emplace_back(pair.first);
	}
	for (UINT light_id : expired)
		release(light_id);

	//	統計算出
	frame_statistics.cached_lights = static_cast<UINT>(entries.size());
	frame_statistics.used_slices = static_cast<UINT>(std::count_if(slice_owners.begin(), slice_owners.end(), [](UINT owner) { return owner != 0; }));
	frame_statistics.slice_count = slice_count;
	last_statistics = frame_statistics;
}

ID3D11DepthStencilView* shadow_atlas::get_depth_stencil_view(UINT first_slice, UINT count)
{
	if (!texture)
		return nullptr;

	//	スライス範囲ごとに作って使い回す
	const UINT key = first_slice * 8 + count;
	auto it = depth_stencil_views.find(key);
	if (it != depth_stencil_views.end())
		return it->second.Get();

	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	texture->get_shader_resource_view()->GetResource(resource.GetAddressOf());

	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
	dsv_desc.Format = common_pipeline_states::shadow_map_depth_stencil_view_format;
	dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	dsv_desc.Texture2DArray.MipSlice = 0;
	dsv_desc.Texture2DArray.FirstArraySlice = first_slice;
	dsv_desc.Texture2DArray.ArraySize = count;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil_view;
	HRESULT hr = render_data::instance()->get_device()->CreateDepthStencilView(resource.Get(), &dsv_desc, depth_stencil_view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	return (depth_stencil_views[key] = depth_stencil_view).Get();
}
