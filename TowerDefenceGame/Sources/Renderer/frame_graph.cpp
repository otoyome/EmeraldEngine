#include "Renderer/frame_graph.h"

#include "Renderer/render_job_executor.h"
#include "render_data.h"

#include <algorithm>
#include <fstream>
#include <limits>

#if defined(USE_IMGUI)
#include "imgui.h"
#endif	//	defined(USE_IMGUI)

namespace
{
	//	@tool 使われなくなった一時テクスチャ・描画先ビューを破棄するまでのフレーム数
	constexpr std::uint64_t keep_frames = 120;

	bool is_same_desc(const D3D11_TEXTURE2D_DESC& l, const D3D11_TEXTURE2D_DESC& r)
	{
		return memcmp(&l, &r, sizeof(D3D11_TEXTURE2D_DESC)) == 0;
	}

	//	@tool テクスチャの実体（シェーダーリソースビュー・UAV のどちらかから取得する）
	Microsoft::WRL::ComPtr<ID3D11Resource> get_resource(const texture_ptr& texture)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		if (!texture)
			return resource;
		if (auto view = texture->get_shader_resource_view())
			view->GetResource(resource.GetAddressOf());
		else if (auto uav = texture->get_unordered_access_view())
			uav->GetResource(resource.GetAddressOf());
		return resource;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
frame_graph::builder::builder(frame_graph& graph, UINT pass_index) : graph(graph), pass_index(pass_index)
{
}

frame_graph::texture_handle frame_graph::builder::create(std::string_view name, const D3D11_TEXTURE2D_DESC& desc)
{
	auto& target = graph.textures.emplace_back();
	target.name = name;
	target.desc = desc;
	target.transient = true;

	auto& node = graph.nodes.emplace_back();
	node.texture = static_cast<UINT>(graph.textures.size() - 1);
	node.version = 0;
	return { static_cast<UINT>(graph.textures.size() - 1) };
}

frame_graph::texture_handle frame_graph::builder::read(texture_handle handle, texture_usage usage)
{
	graph.read_texture(pass_index, handle);
	graph.record_access(pass_index, handle, usage);
	return handle;
}

frame_graph::texture_handle frame_graph::builder::write(texture_handle handle, texture_usage usage)
{
	graph.write_texture(pass_index, handle);
	graph.record_access(pass_index, handle, usage);
	return handle;
}

UINT frame_graph::builder::add_render_pass(std::vector<color_attachment> colors, std::optional<depth_attachment> depth)
{
	for (const auto& color : colors)
	{
		//	@tool グラフの外の描画先に描くパスは除かない
		if (!color.texture.is_valid())
		{
			graph.passes[pass_index].side_effect = true;
			continue;
		}
		//	@tool クリアしない描画先は、それまでの内容に重ねて描くので読みとしても数える
		if (!color.clear)
			graph.read_texture(pass_index, color.texture);
		graph.write_texture(pass_index, color.texture);
		graph.record_access(pass_index, color.texture, texture_usage::render_target);
	}
	if (depth)
	{
		if (!depth->clear_depth || !depth->clear_stencil)
			graph.read_texture(pass_index, depth->texture);
		graph.write_texture(pass_index, depth->texture);
		graph.record_access(pass_index, depth->texture, texture_usage::depth_write);
	}

	auto& target = graph.passes[pass_index].render_passes.emplace_back();
	target.colors = std::move(colors);
	target.depth = depth;
	return static_cast<UINT>(graph.passes[pass_index].render_passes.size() - 1);
}

void frame_graph::builder::side_effect()
{
	graph.passes[pass_index].side_effect = true;
}

void frame_graph::builder::use_immediate_context(bool wait_for_recordings)
{
	graph.passes[pass_index].use_immediate_context = true;
	graph.passes[pass_index].wait_for_recordings = wait_for_recordings;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
frame_graph::resources::resources(std::shared_ptr<const frame_data> frame, UINT pass_index)
: frame(std::move(frame)), pass_index(pass_index)
{
}

texture_ptr frame_graph::resources::get_texture(texture_handle handle) const
{
	if (!handle.is_valid() || handle.index >= frame->textures.size())
		return nullptr;
	return frame->textures[handle.index].texture;
}

D3D11_VIEWPORT frame_graph::resources::get_viewport(texture_handle handle) const
{
	D3D11_VIEWPORT viewport{};
	viewport.MaxDepth = 1.0f;
	if (!handle.is_valid() || handle.index >= frame->textures.size())
		return viewport;

	const auto& target = frame->textures[handle.index];
	viewport.Width = static_cast<float>(target.desc.Width);
	viewport.Height = static_cast<float>(target.desc.Height);
	return viewport;
}

const render_pass& frame_graph::resources::get_render_pass(UINT index) const
{
	return frame->render_passes[pass_index][index];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
frame_graph::frame_graph()
{
	begin_frame();
}

frame_graph::~frame_graph()
{
}

void frame_graph::begin_frame()
{
	textures.clear();
	nodes.clear();
	passes.clear();

	//	前のフレームの実体は、そのフレームの記録処理が持ち続ける（ここでは新しく作り直す）
	frame = std::make_shared<resources::frame_data>();
	++frame_count;
}

frame_graph::texture_handle frame_graph::import_texture(std::string_view name, const texture_ptr& texture)
{
	for (UINT index = 0; index < static_cast<UINT>(textures.size()); ++index)
	{
		const auto& target = textures[index];
		if (!target.transient && target.texture == texture)
			return { index };
	}

	auto& target = textures.emplace_back();
	target.name = name;
	target.texture = texture;
	if (auto texture_2d = std::dynamic_pointer_cast<texture2d>(texture))
		target.desc = texture_2d->get_desc();

	auto& node = nodes.emplace_back();
	node.texture = static_cast<UINT>(textures.size() - 1);
	node.version = 0;
	return { static_cast<UINT>(textures.size() - 1) };
}

UINT frame_graph::begin_pass(std::string_view name, const renderer_context& context)
{
	auto& target = passes.emplace_back();
	target.name = name;
	target.owner_name = owner_name;
	target.context = context;
	return static_cast<UINT>(passes.size() - 1);
}

void frame_graph::end_pass(UINT pass_index, execute_function execute)
{
	auto& target = passes[pass_index];
	target.execute = std::move(execute);

	//	@tool 何も書かないパスは、グラフの外に結果を残すものとして扱う
	if (target.writes.empty())
		target.side_effect = true;
}

UINT frame_graph::read_texture(UINT pass_index, texture_handle handle)
{
	if (!handle.is_valid() || handle.index >= textures.size())
		return UINT_MAX;

	//	@tool その時点の最新版を読む
	const UINT version = textures[handle.index].version;
	for (UINT index = static_cast<UINT>(nodes.size()); index > 0; --index)
	{
		auto& node = nodes[index - 1];
		if (node.texture != handle.index || node.version != version)
			continue;
		auto& reads = passes[pass_index].reads;
		if (std::find(reads.begin(), reads.end(), index - 1) == reads.end())
		{
			reads.emplace_back(index - 1);
			++node.read_count;
		}
		return index - 1;
	}
	return UINT_MAX;
}

UINT frame_graph::write_texture(UINT pass_index, texture_handle handle)
{
	if (!handle.is_valid() || handle.index >= textures.size())
		return UINT_MAX;

	//	@tool 同じパスで同じテクスチャへ何度書いても版は1つだけ進める
	auto& writes = passes[pass_index].writes;
	for (UINT node_index : writes)
	{
		if (nodes[node_index].texture == handle.index)
			return node_index;
	}

	auto& target = textures[handle.index];
	++target.version;
	auto& node = nodes.emplace_back();
	node.texture = handle.index;
	node.version = target.version;
	node.producer = static_cast<int>(pass_index);
	writes.emplace_back(static_cast<UINT>(nodes.size() - 1));
	return static_cast<UINT>(nodes.size() - 1);
}

void frame_graph::record_access(UINT pass_index, texture_handle handle, texture_usage usage)
{
	if (!handle.is_valid() || handle.index >= textures.size())
		return;
	auto& accesses = passes[pass_index].accesses;
	if (!accesses.empty() && accesses.back().first == handle.index && accesses.back().second == usage)
		return;
	accesses.emplace_back(handle.index, usage);
}

void frame_graph::build_transitions()
{
	//	@tool 取り込んだテクスチャはグラフの外での使い方から、一時テクスチャは内容不定から始まる
	std::vector<texture_usage> states(textures.size());
	for (UINT index = 0; index < static_cast<UINT>(textures.size()); ++index)
		states[index] = textures[index].transient ? texture_usage::undefined : texture_usage::external;

	for (auto& target : passes)
	{
		target.transitions.clear();
		if (target.culled)
			continue;

		std::vector<UINT> touched;
		for (const auto& [texture, usage] : target.accesses)
		{
			const bool inside_pass = std::find(touched.begin(), touched.end(), texture) != touched.end();
			if (!inside_pass)
				touched.emplace_back(texture);

			//	@tool 使い方が変わる所が状態遷移。UAV が続く場合も、前のパスの書き込みを待つ必要がある（D3D12 の UAV バリア）
			const bool uav_barrier = !inside_pass && usage == texture_usage::unordered_access && states[texture] == texture_usage::unordered_access;
			if (states[texture] != usage || uav_barrier)
				target.transitions.push_back({ texture, states[texture], usage, inside_pass });
			states[texture] = usage;
		}
	}
}

void frame_graph::cull_passes()
{
	//	取り込んだテクスチャの最終版はグラフの外で使われるので、読まれたものとして扱う
	std::vector<UINT> read_counts(nodes.size());
	for (UINT index = 0; index < static_cast<UINT>(nodes.size()); ++index)
	{
		read_counts[index] = nodes[index].read_count;
		const auto& texture = textures[nodes[index].texture];
		if (!texture.transient && nodes[index].version == texture.version)
			++read_counts[index];
	}

	std::vector<UINT> pass_counts(passes.size());
	for (UINT index = 0; index < static_cast<UINT>(passes.size()); ++index)
		pass_counts[index] = static_cast<UINT>(passes[index].writes.size());

	//	@tool 読まれない版を書いたパスの参照を減らし、全ての書き込みが読まれないパスを除く
	std::vector<UINT> unused;
	for (UINT index = 0; index < static_cast<UINT>(nodes.size()); ++index)
	{
		if (read_counts[index] == 0)
			unused.emplace_back(index);
	}
	while (!unused.empty())
	{
		const UINT node_index = unused.back();
		unused.pop_back();

		const int producer = nodes[node_index].producer;
		if (producer < 0)
			continue;
		auto& target = passes[producer];
		if (--pass_counts[producer] > 0 || target.side_effect || target.culled)
			continue;

		target.culled = true;
		for (UINT read : target.reads)
		{
			if (--read_counts[read] == 0)
				unused.emplace_back(read);
		}
	}
}

void frame_graph::allocate_transient_textures()
{
	//	@tool 宣言から一時テクスチャを使う期間（最初と最後のパス番号）を求める（除いたパスは数えない）
	constexpr UINT unused = UINT_MAX;
	std::vector<UINT> first_use(textures.size(), unused);
	std::vector<UINT> last_use(textures.size(), 0);
	for (UINT pass_index = 0; pass_index < static_cast<UINT>(passes.size()); ++pass_index)
	{
		const auto& target = passes[pass_index];
		if (target.culled)
			continue;
		auto mark = [&](UINT node_index)
		{
			const UINT texture = nodes[node_index].texture;
			first_use[texture] = (std::min)(first_use[texture], pass_index);
			last_use[texture] = (std::max)(last_use[texture], pass_index);
		};
		for (UINT node_index : target.reads)
			mark(node_index);
		for (UINT node_index : target.writes)
			mark(node_index);
	}

#if defined(USE_IMGUI)
	//	@tool 書く前に読んでいる一時テクスチャは内容が不定なので知らせる
	for (const auto& target : passes)
	{
		if (target.culled)
			continue;
		for (UINT node_index : target.reads)
		{
			const auto& node = nodes[node_index];
			if (textures[node.texture].transient && node.version == 0)
				DEBUG_WARNING("frame_graph : transient texture [%s] is read before written in pass [%s]", textures[node.texture].name.c_str(), target.name.c_str());
		}
	}
#endif	//	defined(USE_IMGUI)

	//	@tool 使い始めの早い順に割り当てる
	std::vector<UINT> order;
	for (UINT index = 0; index < static_cast<UINT>(textures.size()); ++index)
	{
		if (textures[index].transient && first_use[index] != unused)
			order.emplace_back(index);
	}
	std::stable_sort(order.begin(), order.end(), [&](UINT l, UINT r) { return first_use[l] < first_use[r]; });

	//	@tool プールの各テクスチャが、このフレームで何番目のパスまで使われるか（-1 は未使用）
	std::vector<std::int64_t> busy_until(texture_pool.size(), -1);
	auto device = render_data::instance()->get_device();
	for (UINT index : order)
	{
		auto& target = textures[index];

		//	@tool 同じ形式で、使用期間が重ならないものを探す
		int found = -1;
		for (int pool_index = 0; pool_index < static_cast<int>(texture_pool.size()); ++pool_index)
		{
			if (busy_until[pool_index] >= static_cast<std::int64_t>(first_use[index]))
				continue;
			if (!is_same_desc(texture_pool[pool_index].desc, target.desc))
				continue;
			found = pool_index;
			break;
		}

		//	@tool 無ければ作る
		if (found < 0)
		{
			pooled_texture created;
			created.desc = target.desc;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> resource;
			HRESULT hr = device->CreateTexture2D(&target.desc, nullptr, resource.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			if (FAILED(hr))
				continue;

			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
			if (system_utility::is_bitflag(target.desc.BindFlags, D3D11_BIND_SHADER_RESOURCE))
			{
				hr = device->CreateShaderResourceView(resource.Get(), nullptr, shader_resource_view.GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			}
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view;
			if (system_utility::is_bitflag(target.desc.BindFlags, D3D11_BIND_UNORDERED_ACCESS))
			{
				hr = device->CreateUnorderedAccessView(resource.Get(), nullptr, unordered_access_view.GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			}
			created.texture = std::make_shared<texture2d>(resource, shader_resource_view, unordered_access_view);

			texture_pool.emplace_back(std::move(created));
			busy_until.emplace_back(-1);
			found = static_cast<int>(texture_pool.size() - 1);
		}

		auto& pooled = texture_pool[found];
		pooled.last_used_frame = frame_count;
		busy_until[found] = static_cast<std::int64_t>(last_use[index]);

		target.texture = pooled.texture;
		target.pool_index = found;
	}

	//	@tool しばらく使われていない物を破棄する（記録中・提出中のフレームは実体を参照し続けているので、そちらが持っている間は残る）
	texture_pool.erase(std::remove_if(texture_pool.begin(), texture_pool.end(), [&](const pooled_texture& pooled)
	{
		return frame_count - pooled.last_used_frame > keep_frames;
	}), texture_pool.end());
}

ID3D11RenderTargetView* frame_graph::acquire_render_target_view(const texture_ptr& texture)
{
	Microsoft::WRL::ComPtr<ID3D11Resource> resource = get_resource(texture);
	if (!resource)
		return nullptr;

	auto& target = view_cache[{ resource.Get(), DXGI_FORMAT_UNKNOWN }];
	target.last_used_frame = frame_count;
	if (!target.render_target_view)
	{
		target.resource = resource;
		HRESULT hr = render_data::instance()->get_device()->CreateRenderTargetView(resource.Get(), nullptr, target.render_target_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	return target.render_target_view.Get();
}

ID3D11DepthStencilView* frame_graph::acquire_depth_stencil_view(const texture_ptr& texture, DXGI_FORMAT format)
{
	Microsoft::WRL::ComPtr<ID3D11Resource> resource = get_resource(texture);
	if (!resource)
		return nullptr;

	auto& target = view_cache[{ resource.Get(), format }];
	target.last_used_frame = frame_count;
	if (!target.depth_stencil_view)
	{
		target.resource = resource;
		D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
		desc.Format = format;
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;
		HRESULT hr = render_data::instance()->get_device()->CreateDepthStencilView(resource.Get(), &desc, target.depth_stencil_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	return target.depth_stencil_view.Get();
}

void frame_graph::build_render_passes()
{
	frame->textures.resize(textures.size());
	for (UINT index = 0; index < static_cast<UINT>(textures.size()); ++index)
	{
		frame->textures[index].texture = textures[index].texture;
		frame->textures[index].desc = textures[index].desc;
	}

	frame->render_passes.resize(passes.size());
	for (UINT pass_index = 0; pass_index < static_cast<UINT>(passes.size()); ++pass_index)
	{
		const auto& target = passes[pass_index];
		if (target.culled)
			continue;
		auto& out_passes = frame->render_passes[pass_index];
		for (const auto& desc : target.render_passes)
		{
			render_pass& out = out_passes.emplace_back();
			for (const auto& color : desc.colors)
			{
				render_target_view& view = out.render_targets.emplace_back();
				view.view = color.texture.is_valid() ? acquire_render_target_view(textures[color.texture.index].texture) : color.external_view.Get();
				view.clear_color = color.clear;
				for (int component = 0; component < 4; ++component)
					view.clear_color_value[component] = color.clear_value[component];
			}
			if (desc.depth)
			{
				out.depth_stencil.view = acquire_depth_stencil_view(textures[desc.depth->texture.index].texture, desc.depth->format);
				out.depth_stencil.clear_depth = desc.depth->clear_depth;
				out.depth_stencil.clear_stencil = desc.depth->clear_stencil;
				out.depth_stencil.clear_depth_value = desc.depth->clear_depth_value;
				out.depth_stencil.clear_stencil_value = desc.depth->clear_stencil_value;
			}
			else
			{
				out.depth_stencil.clear_depth = false;
				out.depth_stencil.clear_stencil = false;
			}
		}
	}

	//	@tool しばらく使われていないビューを破棄する
	for (auto it = view_cache.begin(); it != view_cache.end();)
	{
		if (frame_count - it->second.last_used_frame > keep_frames)
			it = view_cache.erase(it);
		else
			++it;
	}
}

void frame_graph::execute(render_job_executor& executor)
{
	cull_passes();
	build_transitions();
	allocate_transient_textures();
	build_render_passes();

	std::vector<report_pass> executed_passes;
	executed_passes.reserve(passes.size());

	//	@tool 追加した順に提出物へ積む（提出も追加順になる）
	const std::shared_ptr<const resources::frame_data> frame_resources = frame;
	for (UINT pass_index = 0; pass_index < static_cast<UINT>(passes.size()); ++pass_index)
	{
		auto& target = passes[pass_index];
		executed_passes.push_back({ target.name, target.owner_name, target.use_immediate_context, target.culled });
		for (const auto& transition : target.transitions)
		{
			std::string text = textures[transition.texture].name;
			text += " : ";
			text += magic_enum::enum_name(transition.before);
			text += " -> ";
			text += magic_enum::enum_name(transition.after);
			if (transition.inside_pass)
				text += " (in pass)";
			executed_passes.back().transitions.emplace_back(std::move(text));
		}
		if (target.culled)
			continue;

		executor.execute([frame_resources, pass_index, execute = std::move(target.execute)](graphics_context& context)
		{
			execute(resources(frame_resources, pass_index), context);
		}, target.name, target.context, target.use_immediate_context, target.wait_for_recordings, target.owner_name);
	}
	passes.clear();

	std::vector<std::pair<std::string, int>> transient_textures;
	for (const auto& target : textures)
	{
		if (target.transient)
			transient_textures.emplace_back(target.name, target.pool_index);
	}

	std::lock_guard<std::mutex> lock(report_mutex);
	last_passes = std::move(executed_passes);
	last_transient_textures = std::move(transient_textures);
	last_pool_size = static_cast<UINT>(texture_pool.size());
}

#if defined(USE_IMGUI)
void frame_graph::imgui_render() const
{
	std::lock_guard<std::mutex> lock(report_mutex);
	ImGui::Text("pass count : %d", static_cast<int>(last_passes.size()));
	for (std::size_t index = 0; index < last_passes.size(); ++index)
	{
		const auto& target = last_passes[index];
		ImGui::Text("%3d : [%s] %s%s%s", static_cast<int>(index), target.owner_name.c_str(), target.name.c_str(), target.immediate ? " (immediate)" : "", target.culled ? " (culled)" : "");
		//	@tool 状態遷移（D3D12 のバリアに相当）
		for (const auto& transition : target.transitions)
			ImGui::TextDisabled("        %s", transition.c_str());
	}

	//	@tool 一時テクスチャと、割り当てたプールの番号（同じ番号は同じ実体を使い回している）
	ImGui::Separator();
	ImGui::Text("transient textures : %d  pool : %d", static_cast<int>(last_transient_textures.size()), static_cast<int>(last_pool_size));
	for (const auto& target : last_transient_textures)
	{
		ImGui::Text("  %s -> pool[%d]", target.first.c_str(), target.second);
	}
}
#endif	//	defined(USE_IMGUI)

void frame_graph::write_report(const std::string& file_path) const
{
	std::ofstream ostream(file_path);
	if (!ostream)
		return;

	std::lock_guard<std::mutex> lock(report_mutex);
	ostream << "passes " << last_passes.size() << "\n";
	for (const auto& target : last_passes)
	{
		ostream << "  [" << target.owner_name << "] " << target.name << (target.immediate ? " (immediate)" : "") << (target.culled ? " (culled)" : "") << "\n";
		for (const auto& transition : target.transitions)
			ostream << "      barrier " << transition << "\n";
	}
	ostream << "transient_textures " << last_transient_textures.size() << " pool " << last_pool_size << "\n";
	for (const auto& target : last_transient_textures)
		ostream << "  " << target.first << " -> pool[" << target.second << "]\n";
}
