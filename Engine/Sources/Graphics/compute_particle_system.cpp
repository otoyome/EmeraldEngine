#include "Graphics/compute_particle_system.h"

#include "custom_pipeline_state.h"

namespace
{
	//	定数バッファ関係
	static constexpr UINT common_constant_buffer_index = 10;
	static constexpr UINT fill_unused_index_constant_buffer_index = 11;
	static constexpr UINT bitonic_sort_constant_buffer_index = 12;
	static constexpr UINT range_particles_constant_buffer_index = 11;
	static constexpr UINT build_primitive_constant_buffer_index = 11;

	static constexpr UINT max_erase_particle_list = 128;

	//	その他定数
	static constexpr UINT number_of_vertices_per_particle = 6;
	static constexpr UINT max_emitter = 1024;
	static constexpr UINT vertex_buffer_uav_max = std::min(4, D3D11_1_UAV_SLOT_COUNT - 1);

	//	2の累乗の数値に変換
	UINT calculate_adjust_to_power_of_2(UINT count)
	{
		float	f_exponent = log2f(static_cast<float>(count));
		int		exponent = static_cast<int>(ceilf(f_exponent) + 0.5f);
		count = static_cast<UINT>(pow(2, exponent) + 0.5f);
		count = std::max(std::min(count, 1u << 27), 1u << 7);
		return count;
	}

	//	エミッター情報生成関数
	struct spawn_emitter_data
	{
		UINT emitter_binary_size;
		UINT total_particle_count;
		UINT particle_size;
		UINT particle_binary_size;
	};
	spawn_emitter_data calculate_spawn_emitter_data(const particle_system_item::emitter_system_resource& system)
	{
		spawn_emitter_data data;
		data.emitter_binary_size = system.max_update_item_size;
		//	CopyResourceの関係で4の倍数に変換しておく
		data.total_particle_count = system.total_particle_count + system.total_particle_count % 4;
		data.particle_size = system.particle_type_size;
		data.particle_binary_size = data.total_particle_count * data.particle_size;
		return data;
	}
	;
	//	クリア関数
	void clear_resources(ID3D11DeviceContext* immediate_context)
	{
		ID3D11UnorderedAccessView* clear_uavs[D3D11_1_UAV_SLOT_COUNT]{};
		ID3D11ShaderResourceView* clear_srvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
		immediate_context->CSSetUnorderedAccessViews(0, D3D11_1_UAV_SLOT_COUNT, clear_uavs, nullptr);
		immediate_context->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, clear_srvs);
	};

	//	汎用情報定義
	struct common_constants
	{
		float				elapsed_time;			//	経過時間
		DirectX::XMUINT3	x_of_shift_seed;		//	XorShift乱数種
		UINT				total_emitter_count;	//	エミッター最大数
		UINT				total_particle_count;	//	パーティクル最大数
		UINT				total_cpu_spawn_count;	//	CPU側での発生予定パーティクル数(厳密にはシェーダー側で管理)
		UINT				erase_id_count;			//	破棄Id数
		DirectX::XMUINT4	erase_id_list[max_erase_particle_list / 4];		//	エミッター破棄Idリスト
	};

	//	汎用情報定義
	struct fill_unused_constants
	{
		UINT	emitter_particle_index_head;	//	エミッター毎のパーティクル開始
		UINT	emitter_particle_index_end;		//	エミッター毎のパーティクル終端
		DirectX::XMUINT2	emitter_data_dummy;
	};

	//	バイトニックソート情報定義
	struct bitonic_sort_constants
	{
		UINT	increment;
		UINT	direction;
		DirectX::XMUINT2	dummy;
	};
	static constexpr UINT BitonicSortB2Thread = 256;
	static constexpr UINT BitonicSortC2Thread = 512;

	//	パーティクル生存/死亡範囲調査情報定義
	struct range_particles_constants
	{
		UINT	offset_index;
		DirectX::XMUINT3	dummy;
	};

	//	プリミティブ構築用情報定義
	struct build_primitive_constants
	{
		DirectX::XMUINT4	emitter_ids[vertex_buffer_uav_max / 4];
	};

	//	コンピュートシェーダー
	static constexpr std::string_view compute_shader_path[] =
	{
		"Shaders/ComputeParticle/System/compute_particle_clear_system_cs.hlsl",
		"Shaders/ComputeParticle/System/compute_particle_clear_particles_cs.hlsl",

		"Shaders/ComputeParticle/System/compute_particle_begin_update_cs.hlsl",
		"Shaders/ComputeParticle/System/compute_particle_fill_unused_index_cs.hlsl",
		"Shaders/ComputeParticle/System/compute_particle_spawn_particles_cs.hlsl",
		"Shaders/ComputeParticle/System/compute_particle_initialize_particles_cs.hlsl",

		"Shaders/ComputeParticle/System/compute_particle_update_particles_cs.hlsl",

		"Shaders/ComputeParticle/System/compute_particle_bitonic_sort_b2_cs.hlsl",
		"Shaders/ComputeParticle/System/compute_particle_bitonic_sort_c2_cs.hlsl",

		"Shaders/ComputeParticle/System/compute_particle_range_particles_cs.hlsl",
		"Shaders/ComputeParticle/System/compute_particle_terminate_particles_cs.hlsl",

		"Shaders/ComputeParticle/System/compute_particle_build_emitter_draw_arguments_cs.hlsl",
		"Shaders/ComputeParticle/System/compute_particle_build_primitive_cs.hlsl",
	};
}

//	ヘッダー側に範囲を追加
//	ヘッダーに範囲を記録
//	インデックス番号を元にジオメトリシェーダーを用いてプリミティブ生成
compute_particle_system::compute_particle_system(ID3D11Device* device,
	UINT num_emitter, UINT emitter_data_size,
	UINT num_particle, UINT particle_data_size)
{
	HRESULT hr;
	one_shot_initialize = false;

	//	パーティクル数をスレッド数に合わせて制限
	num_particle = calculate_adjust_to_power_of_2(num_particle);
	total_particle_count = ((num_particle + (NumDispatchThread - 1)) / NumDispatchThread) * NumDispatchThread;
	total_emitter_count = std::min(num_emitter, max_emitter);

	//	エミッター/パーティクル一つの最大サイズから総数のサイズに変更
	this->emitter_binary_buffer_size = emitter_data_size * total_emitter_count;
	this->particle_binary_buffer_size = particle_data_size * total_particle_count;

	emitter_binary_allocator.initialize(this->emitter_binary_buffer_size);
	particle_binary_allocator.initialize(this->particle_binary_buffer_size);
	particle_index_allocator.initialize(total_particle_count);

	//	定数バッファ
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		{
			buffer_desc.ByteWidth = sizeof(common_constants);
			hr = device->CreateBuffer(&buffer_desc, nullptr, common_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		{
			buffer_desc.ByteWidth = sizeof(fill_unused_constants);
			hr = device->CreateBuffer(&buffer_desc, nullptr, fill_unused_index_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		{
			buffer_desc.ByteWidth = sizeof(bitonic_sort_constants);
			hr = device->CreateBuffer(&buffer_desc, nullptr, bitonic_sort_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		{
			buffer_desc.ByteWidth = sizeof(range_particles_constants);
			hr = device->CreateBuffer(&buffer_desc, nullptr, range_particles_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		{
			buffer_desc.ByteWidth = sizeof(build_primitive_constants);
			hr = device->CreateBuffer(&buffer_desc, nullptr, build_primitive_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	}

	//	エミッターテーブルバッファ生成
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = sizeof(emitter_table) * total_emitter_count;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(emitter_table);
		desc.Usage = D3D11_USAGE_DEFAULT;
		D3D11_SUBRESOURCE_DATA initialize_data = {};
		std::vector<emitter_table>	initialize_buffer(total_emitter_count);
		for (UINT index = 0; index < initialize_buffer.size(); ++index)
		{
			initialize_buffer[index] = invalid_tag;
		}
		initialize_data.pSysMem = initialize_buffer.data();
		initialize_data.SysMemPitch = desc.ByteWidth;
		hr = device->CreateBuffer(&desc, &initialize_data, emitter_table_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = device->CreateShaderResourceView(emitter_table_buffer.Get(), nullptr, emitter_table_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(emitter_table_buffer.Get(), nullptr, emitter_table_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	エミッターヘッダーバッファ生成
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = sizeof(emitter_header) * total_emitter_count;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(emitter_header);
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, emitter_header_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = device->CreateShaderResourceView(emitter_header_buffer.Get(), nullptr, emitter_header_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(emitter_header_buffer.Get(), nullptr, emitter_header_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	エミッター範囲バッファ生成
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = sizeof(emitter_range) * total_emitter_count;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(emitter_range);
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, emitter_range_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = device->CreateShaderResourceView(emitter_range_buffer.Get(), nullptr, emitter_range_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(emitter_range_buffer.Get(), nullptr, emitter_range_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	エミッターデータバッファ生成
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = sizeof(emitter_data) * total_emitter_count;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(emitter_data);
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, emitter_data_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = device->CreateShaderResourceView(emitter_data_buffer.Get(), nullptr, emitter_data_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(emitter_data_buffer.Get(), nullptr, emitter_data_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	エミッターバイナリーバッファ生成
	for (UINT index = 0; index < ARRAYSIZE(emitter_binary_buffer); ++index)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = this->emitter_binary_buffer_size;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, emitter_binary_buffer[index].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		srv_desc.BufferEx.FirstElement = 0;
		srv_desc.BufferEx.NumElements = desc.ByteWidth / sizeof(UINT);
		srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
		hr = device->CreateShaderResourceView(emitter_binary_buffer[index].Get(), &srv_desc, emitter_binary_shader_resource_view[index].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.NumElements = desc.ByteWidth / sizeof(UINT);
		uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		hr = device->CreateUnorderedAccessView(emitter_binary_buffer[index].Get(), &uav_desc, emitter_binary_unordered_access_view[index].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	パーティクルテーブルバッファ生成
	for (UINT index = 0; index < ARRAYSIZE(emitter_binary_buffer); ++index)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = sizeof(UINT) * total_particle_count;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(UINT);
		desc.Usage = D3D11_USAGE_DEFAULT;
		D3D11_SUBRESOURCE_DATA initialize_data = {};
		std::vector<UINT>	initialize_buffer(total_particle_count);
		for (UINT index = 0; index < initialize_buffer.size(); ++index)
		{
			initialize_buffer[index] = invalid_tag;
		}
		initialize_data.pSysMem = initialize_buffer.data();
		initialize_data.SysMemPitch = desc.ByteWidth;
		hr = device->CreateBuffer(&desc, &initialize_data, particle_index_buffer[index].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = device->CreateShaderResourceView(particle_index_buffer[index].Get(), nullptr, particle_index_shader_resource_view[index].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(particle_index_buffer[index].Get(), nullptr, particle_index_unordered_access_view[index].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	パーティクルヘッダーバッファ生成
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = sizeof(particle_header) * total_particle_count;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(particle_header);
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, particle_header_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		hr = device->CreateShaderResourceView(particle_header_buffer.Get(), nullptr, particle_header_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		hr = device->CreateUnorderedAccessView(particle_header_buffer.Get(), nullptr, particle_header_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	パーティクルバイナリーバッファ生成
	for (UINT index = 0; index < ARRAYSIZE(emitter_binary_buffer); ++index)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = this->particle_binary_buffer_size;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, particle_binary_buffer[index].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		srv_desc.BufferEx.FirstElement = 0;
		srv_desc.BufferEx.NumElements = desc.ByteWidth / sizeof(UINT);
		srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
		hr = device->CreateShaderResourceView(particle_binary_buffer[index].Get(), &srv_desc, particle_binary_shader_resource_view[index].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.NumElements = desc.ByteWidth / sizeof(UINT);
		uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		hr = device->CreateUnorderedAccessView(particle_binary_buffer[index].Get(), &uav_desc, particle_binary_unordered_access_view[index].GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	更新/描画管理用バッファ
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = FixedIndirectDataSize;
		desc.ByteWidth += sizeof(draw_indirect) * total_emitter_count;
		desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = device->CreateBuffer(&desc, nullptr, indirect_data_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		//	ByteAddressBuffer用にUAVを作成
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.NumElements = desc.ByteWidth / sizeof(UINT);
		uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		hr = device->CreateUnorderedAccessView(indirect_data_buffer.Get(), &uav_desc, indirect_data_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	コンピュートシェーダー読み込み
	for (int index = 0; index < static_cast<int>(dispatch_shaderes::max); ++index)
	{
		load_shader_file desc;
		desc.file_path = compute_shader_path[index];
#if defined(_DEBUG)
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
		system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)
		if (!shader_manager::instance()->load_shader<compute_shader>(compute_shaderes[index], desc))
		{
			assert(!L"not found compute_particle system shader.");
			return;
		}
	}

	//	基本的な描画シェーダー
	//	ここは別途パーティクル用マテリアルに変換するか？
	{
		//	頂点シェーダー読み込み
		{
			load_shader_file desc;
			desc.file_path = "Shaders/ComputeParticle/Renderer/compute_particle_default_render_vs.hlsl";
			desc.input_element_desc.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 });
			desc.input_element_desc.push_back({ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 });
			desc.input_element_desc.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 });

#if defined(_DEBUG)
			system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
			system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

			shader_manager::instance()->load_shader<vertex_shader>(default_draw_vertex_shader, desc);
		}

		//	ピクセルシェーダー読み込み
		{
			load_shader_file desc;
			desc.file_path = "Shaders/ComputeParticle/Renderer/compute_particle_default_render_ps.hlsl";
#if defined(_DEBUG)
			system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
			system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

			shader_manager::instance()->load_shader<pixel_shader>(default_draw_pixel_shader, desc);
		}
	}

	//	エミッター管理情報関係初期化
	{
		emitter_headers.resize(total_emitter_count);
		update_emitter_datas.resize(total_emitter_count);
		emitter_resources.resize(total_emitter_count);
#if 01
		for (UINT index = total_emitter_count - 1; index > 0; --index)
		{
			emitter_id_stack.push(index);
		}
		emitter_id_stack.push(0);
#else	//	テスト用
		for (UINT index = 0; index < total_emitter_count; ++index)
		{
			emitter_id_stack.push(index);
		}
#endif
	}
}

compute_particle_system::~compute_particle_system()
{
}

UINT compute_particle_system::register_emitter(ID3D11Device* device, const particle_system_item::emitter_resource& emitter_resource)
{
	if (emitter_id_stack.empty())
		return invalid_emitter_id;

	//	各種バッファ確保
	spawn_emitter_data data = calculate_spawn_emitter_data(emitter_resource.system);
	UINT emitter_binary_position = emitter_binary_allocator.allocate(data.emitter_binary_size);
	UINT particle_binary_position = particle_binary_allocator.allocate(data.particle_binary_size);
	UINT total_particle_position = particle_index_allocator.allocate(data.total_particle_count);
	if (emitter_binary_position == variable_position_allocator::allocate_failed_code ||
		particle_binary_position == variable_position_allocator::allocate_failed_code ||
		total_particle_position == variable_position_allocator::allocate_failed_code)
	{
		//	確保失敗したので破棄
		emitter_binary_allocator.deallocate(emitter_binary_position);
		particle_binary_allocator.deallocate(particle_binary_position);
		particle_index_allocator.deallocate(total_particle_position);
		return invalid_emitter_id;
	}

	//	ID確保
	UINT emitter_id = emitter_id_stack.top();
	emitter_id_stack.pop();
	emitter_id_table.emplace_back(emitter_id);
	additional_emitter_ids.push(emitter_id);

	//	リソース類登録
	auto& em_resource = emitter_resources[emitter_id];
	auto& em_header = emitter_headers[emitter_id];
	em_resource.resource = emitter_resource;

	//	頂点バッファ確保
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = emitter_resource.system.total_particle_count * number_of_vertices_per_particle;
		//	軌跡の場合は別途軌跡数を加算
		if (emitter_resource.render.renderer_type == particle_system_item::renderer_types::ribon)
			desc.ByteWidth *= emitter_resource.render.trail_count;
		desc.ByteWidth *= sizeof(vertex_data);
		desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		desc.Usage = D3D11_USAGE_DEFAULT;
		HRESULT hr = device->CreateBuffer(&desc, nullptr, em_resource.buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		//	ByteAddressBuffer用にUAVを作成
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.NumElements = desc.ByteWidth / sizeof(UINT);
		uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		hr = device->CreateUnorderedAccessView(em_resource.buffer.Get(), &uav_desc, em_resource.unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	em_header.emitter_binary_head = emitter_binary_position;
	em_header.particle_binary_head = particle_binary_position;
	em_header.particle_index_head = total_particle_position;
	em_header.particle_size = data.particle_size;
	return emitter_id;
}

bool compute_particle_system::unregister_emitter(UINT emitter_id)
{
	//	テーブルに登録されているか？
	if (std::find_if(emitter_id_table.begin(),
		emitter_id_table.end(),
		[emitter_id](UINT a) {return a == emitter_id; }) == emitter_id_table.end())
	{
		return false;
	}

	//	登録済みなら除外
	if (std::find_if(remove_emitter_ids.begin(),
		remove_emitter_ids.end(),
		[emitter_id](UINT a) {return a == emitter_id; }) == remove_emitter_ids.end())
	{
		remove_emitter_ids.push_back(emitter_id);
	}
	return true;
}

void compute_particle_system::update_emitter(UINT emitter_id, const update_emitter_data& data)
{
	if (emitter_id == invalid_emitter_id)
		return;

	update_emitter_datas[emitter_id] = data;
}

void compute_particle_system::update(graphics_context& context, float elapsed_time)
{
	//	定数バッファ設定
	context.device_context->CSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());

	//	エミッターガベージコレクション
	bool change_emitter_id_table = !additional_emitter_ids.empty();
	if (!garbage_correct_emitter_ids.empty())
	{
		//	emitter_id_tableをなめていって、破棄IDであれば詰め直しをする

		//	一旦全バッファ破棄
		for (auto& emitter_id : emitter_id_table)
		{
			//	必要な情報
			auto& emitter_resource = emitter_resources[emitter_id];
			auto& emitter_header = emitter_headers[emitter_id];

			//	各種バッファ解放
			emitter_binary_allocator.deallocate(emitter_header.emitter_binary_head);
			particle_binary_allocator.deallocate(emitter_header.particle_binary_head);
			particle_index_allocator.deallocate(emitter_header.particle_index_head);
		}

		//	ガベージコレクション対象のIdを全て破棄
		while (!garbage_correct_emitter_ids.empty())
		{
			//	対象IDを取得
			UINT remove_emitter_id = garbage_correct_emitter_ids.back();
			garbage_correct_emitter_ids.pop_back();

			//	管理テーブルから破棄して追加対象に入れる
			auto it_table = std::find_if(emitter_id_table.begin(),
				emitter_id_table.end(),
				[remove_emitter_id](UINT a) {return a == remove_emitter_id; });
			emitter_id_table.erase(it_table);
			emitter_id_stack.push(remove_emitter_id);

			//	データ初期化
			auto& emitter_resource = emitter_resources[remove_emitter_id];
			auto& emitter_header = emitter_headers[remove_emitter_id];
			emitter_resource = {};
			emitter_header = {};
		}

		//	@remainder	残っているIdをサイズ順にソートしておかないといけない、emitter_id_table

		//	バッファの詰め直しを行う
		UINT new_buffer_index = (current_buffer_index + 1) % ARRAYSIZE(emitter_binary_buffer);
		UINT old_buffer_index = current_buffer_index;
		for (auto& emitter_id : emitter_id_table)
		{
			//	必要な情報
			auto& emitter_resource = emitter_resources[emitter_id];
			auto& emitter_header = emitter_headers[emitter_id];

			//	新しい位置を再確保
			spawn_emitter_data data = calculate_spawn_emitter_data(emitter_resource.resource.system);
			UINT emitter_binary_position = emitter_binary_allocator.allocate(data.emitter_binary_size);
			UINT particle_binary_position = particle_binary_allocator.allocate(data.particle_binary_size);
			UINT particle_index_position = particle_index_allocator.allocate(data.total_particle_count);

			//	元座標から新座標へコピーリソースしていく
			{
				D3D11_BOX box = { emitter_header.emitter_binary_head, 0, 0,
					emitter_header.emitter_binary_head + data.emitter_binary_size, 1, 1 };
				context.device_context->CopySubresourceRegion(
					emitter_binary_buffer[new_buffer_index].Get(), 0, emitter_binary_position, 0, 0,
					emitter_binary_buffer[old_buffer_index].Get(), 0, &box);
			}
			{
				D3D11_BOX box = { emitter_header.particle_binary_head, 0, 0,
					emitter_header.particle_binary_head + data.particle_binary_size, 1, 1 };
				context.device_context->CopySubresourceRegion(
					particle_binary_buffer[new_buffer_index].Get(), 0, particle_binary_position, 0, 0,
					particle_binary_buffer[old_buffer_index].Get(), 0, &box);
			}
			{
				D3D11_BOX box = { emitter_header.particle_index_head * sizeof(UINT), 0, 0,
					(emitter_header.particle_index_head + data.total_particle_count) * sizeof(UINT), 1, 1};
				context.device_context->CopySubresourceRegion(
					particle_index_buffer[new_buffer_index].Get(), 0, particle_index_position, 0, 0,
					particle_index_buffer[old_buffer_index].Get(), 0, &box);
			}
			//	エミッターヘッダー更新
			emitter_header.emitter_binary_head = emitter_binary_position;
			emitter_header.particle_binary_head = particle_binary_position;
			emitter_header.particle_index_head = particle_index_position;
			emitter_header.particle_size = data.particle_size;
		}

		//	エミッターヘッダーバッファ更新
		{
			D3D11_BOX write_box = {};
			write_box.left = 0;
			write_box.right = sizeof(emitter_header) * static_cast<UINT>(emitter_headers.size());
			write_box.top = 0;
			write_box.bottom = 1;
			write_box.front = 0;
			write_box.back = 1;
			context.device_context->UpdateSubresource(emitter_header_buffer.Get(),
				0,
				&write_box,
				emitter_headers.data(),
				sizeof(emitter_header) * static_cast<UINT>(emitter_headers.size()),
				0);
		}

		//	番号フリップ
		current_buffer_index = new_buffer_index;
		change_emitter_id_table = true;
	}

	//	エミッター破棄処理
	UINT erase_id_count = 0; //	破棄Id数
	UINT erase_id_list[max_erase_particle_list]; //	エミッター破棄Idリスト
	while (!remove_emitter_ids.empty())
	{
		//	破棄カウントが最大数を超えた場合はいったん無視する
		if (erase_id_count >= max_erase_particle_list)
			break;

		//	IDをガベージコレクション対象に設定
		UINT remove_emitter_id = remove_emitter_ids.back();
		garbage_correct_emitter_ids.emplace_back(remove_emitter_id);
		remove_emitter_ids.pop_back();

		//	強制破棄リストとして登録して Update 時に全て消す。
		erase_id_list[erase_id_count++] = remove_emitter_id;
	}

	//	エミッターバイナリー更新
	UINT total_spawn_cpu_particle_count = 0;
	for (auto& emitter_id : emitter_id_table)
	{
		//	更新アイテム類取得
		auto& emitter_resource = emitter_resources[emitter_id];
		auto& update_emitter_data = update_emitter_datas[emitter_id];

		//	アイテム情報をバイナリに書き込む
		std::vector<char> data_buffer;
		data_buffer.resize(emitter_resource.resource.system.max_update_item_size);
		memset(data_buffer.data(), 0, data_buffer.size());
		UINT offset = 0;
		auto register_buffer = [&](const void* buffer, std::size_t size)
		{
			memcpy_s(data_buffer.data() + offset,
				data_buffer.size() - offset, buffer, static_cast<UINT>(size));
			offset += static_cast<UINT>(size);
		};
		register_buffer(&update_emitter_data.common, sizeof(update_emitter_data.common));
		register_buffer(update_emitter_data.item_binary.data(), update_emitter_data.item_binary.size());
		total_spawn_cpu_particle_count += update_emitter_data.common.spawn_count;

		//	バッファの書き込み
		D3D11_BOX write_box = {};
		write_box.left = emitter_headers[emitter_id].emitter_binary_head;
		write_box.right = write_box.left + static_cast<UINT>(data_buffer.size());
		write_box.top = 0;
		write_box.bottom = 1;
		write_box.front = 0;
		write_box.back = 1;
		context.device_context->UpdateSubresource(emitter_binary_buffer[current_buffer_index].Get(),
			0,
			&write_box,
			data_buffer.data(),
			static_cast<UINT>(data_buffer.size()),
			0);
	}

	//	共用の定数バッファ設定
	{
		common_constants constant;
		constant.elapsed_time = elapsed_time;
		constant.x_of_shift_seed.x = rand();
		constant.x_of_shift_seed.y = rand();
		constant.x_of_shift_seed.z = rand();
		constant.total_emitter_count = total_emitter_count;	//	emitter_id_table.size();
		constant.total_particle_count = total_particle_count;
		constant.total_cpu_spawn_count = total_spawn_cpu_particle_count;
		constant.erase_id_count = erase_id_count;
		for (UINT index = 0; index < constant.erase_id_count; ++index)
			(&constant.erase_id_list[0].x)[index] = erase_id_list[index];
		context.device_context->UpdateSubresource(common_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
		context.device_context->CSSetConstantBuffers(common_constant_buffer_index, 1, common_constant_buffer.GetAddressOf());
	}

	//	システム初期化処理
	if (!one_shot_initialize)
	{
		one_shot_initialize = true;

		//	システム初期化
		{
			context.device_context->CSSetUnorderedAccessViews(0, 1, indirect_data_unordered_access_view.GetAddressOf(), nullptr);
			context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::clear_system_shader)]->get_shader().Get(), nullptr, 0);
			context.device_context->Dispatch(1, 1, 1);
			clear_resources(context.device_context);
		}

		//	パーティクルリストクリア
		{
			context.device_context->CSSetUnorderedAccessViews(0, 1, particle_header_unordered_access_view.GetAddressOf(), nullptr);
			context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::clear_particles)]->get_shader().Get(), nullptr, 0);
			context.device_context->Dispatch(total_particle_count / NumDispatchThread, 1, 1);
			clear_resources(context.device_context);
		}
	}

	//	テーブルID更新
	if (change_emitter_id_table)
	{
		std::vector<emitter_table>	initialize_buffer(total_emitter_count);
		for (UINT index = 0; index < emitter_id_table.size(); ++index)
		{
			initialize_buffer[index] = emitter_id_table[index];
		}
		//	違反タグで埋めておく
		for (std::size_t index = emitter_id_table.size(); index < initialize_buffer.size(); ++index)
		{
			initialize_buffer[index] = invalid_tag;
		}
		context.device_context->UpdateSubresource(emitter_table_buffer.Get(),
			0,
			nullptr,
			initialize_buffer.data(),
			sizeof(UINT) * static_cast<UINT>(initialize_buffer.size()),
			0);
	}

	//	更新開始処理
	{
		context.device_context->CSSetUnorderedAccessViews(0, 1, indirect_data_unordered_access_view.GetAddressOf(), nullptr);
		context.device_context->CSSetShaderResources(0, 1, emitter_table_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShaderResources(1, 1, emitter_data_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::begin_update)]->get_shader().Get(), nullptr, 0);
		context.device_context->Dispatch(1, 1, 1);
		clear_resources(context.device_context);
	}

	//	エミッター生成時に初期化時の処理
	while (!additional_emitter_ids.empty())
	{
		//	追加テーブルID
		UINT additional_emitter_id = additional_emitter_ids.top();
		additional_emitter_ids.pop();

		auto& resource = emitter_resources[additional_emitter_id];

		//	ヘッダーバッファ更新
		auto& header = emitter_headers[additional_emitter_id];
		{
			D3D11_BOX write_box = {};
			write_box.left = sizeof(emitter_header) * additional_emitter_id;
			write_box.right = write_box.left + sizeof(emitter_header);
			write_box.top = 0;
			write_box.bottom = 1;
			write_box.front = 0;
			write_box.back = 1;
			context.device_context->UpdateSubresource(emitter_header_buffer.Get(),
				0,
				&write_box,
				emitter_headers.data() + additional_emitter_id,
				sizeof(emitter_header),
				0);
		}

		//	エミッター情報バッファ更新
		{
			emitter_data emitter_data;
			emitter_data.spawn_head = 0;
			emitter_data.spawn_num = 0;
			emitter_data.particle_num = 0;
			emitter_data.particle_max = resource.resource.system.total_particle_count;

			D3D11_BOX write_box = {};
			write_box.left = sizeof(emitter_data) * additional_emitter_id;
			write_box.right = write_box.left + sizeof(emitter_data);
			write_box.top = 0;
			write_box.bottom = 1;
			write_box.front = 0;
			write_box.back = 1;
			context.device_context->UpdateSubresource(emitter_data_buffer.Get(),
				0,
				&write_box,
				&emitter_data,
				sizeof(emitter_data),
				0);
		}

		//	定数バッファ更新
		{
			fill_unused_constants constant;
			constant.emitter_particle_index_head = header.particle_index_head;
			constant.emitter_particle_index_end = header.particle_index_head
												+ resource.resource.system.total_particle_count;
			context.device_context->UpdateSubresource(fill_unused_index_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
			context.device_context->CSSetConstantBuffers(fill_unused_index_constant_buffer_index, 1, fill_unused_index_constant_buffer.GetAddressOf());
		}

		//	パーティクルヘッダーの範囲を未使用インデックスとして埋める
		{
			context.device_context->CSSetUnorderedAccessViews(0, 1, particle_index_unordered_access_view[current_buffer_index].GetAddressOf(), nullptr);
			context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::fill_unused_index)]->get_shader().Get(), nullptr, 0);
			UINT particle_count = resource.resource.system.total_particle_count;
			particle_count += ((NumDispatchThread - particle_count) % NumDispatchThread);
			context.device_context->Dispatch(particle_count / NumDispatchThread, 1, 1);
			clear_resources(context.device_context);
		}
	}

	//	パーティクル発生処理
	{
		//	発生パーティクル割り当て
		{
			context.device_context->CSSetUnorderedAccessViews(0, 1, particle_header_unordered_access_view.GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(1, 1, emitter_range_unordered_access_view.GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(2, 1, emitter_data_unordered_access_view.GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(3, 1, indirect_data_unordered_access_view.GetAddressOf(), nullptr);

			context.device_context->CSSetShaderResources(0, 1, emitter_table_shader_resource_view.GetAddressOf());
			context.device_context->CSSetShaderResources(1, 1, emitter_header_shader_resource_view.GetAddressOf());
			context.device_context->CSSetShaderResources(2, 1, emitter_binary_shader_resource_view[current_buffer_index].GetAddressOf());

			context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::spawn_particles)]->get_shader().Get(), nullptr, 0);
			context.device_context->Dispatch(static_cast<UINT>(emitter_id_table.size()), 1, 1);
			clear_resources(context.device_context);
		}

		//	発生パーティクル初期化
		{
			context.device_context->CSSetUnorderedAccessViews(0, 1, particle_header_unordered_access_view.GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(1, 1, emitter_data_unordered_access_view.GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(2, 1, particle_index_unordered_access_view[current_buffer_index].GetAddressOf(), nullptr);
			context.device_context->CSSetUnorderedAccessViews(3, 1, particle_binary_unordered_access_view[current_buffer_index].GetAddressOf(), nullptr);

			context.device_context->CSSetShaderResources(0, 1, emitter_table_shader_resource_view.GetAddressOf());
			context.device_context->CSSetShaderResources(1, 1, emitter_header_shader_resource_view.GetAddressOf());
			context.device_context->CSSetShaderResources(2, 1, emitter_binary_shader_resource_view[current_buffer_index].GetAddressOf());

			context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::initialize_particles)]->get_shader().Get(), nullptr, 0);
			context.device_context->Dispatch(static_cast<UINT>(emitter_id_table.size()), 1, 1);
			clear_resources(context.device_context);
		}
	}

	//	パーティクル更新処理
	{
		context.device_context->CSSetUnorderedAccessViews(0, 1, particle_header_unordered_access_view.GetAddressOf(), nullptr);
		context.device_context->CSSetUnorderedAccessViews(1, 1, particle_binary_unordered_access_view[current_buffer_index].GetAddressOf(), nullptr);

		context.device_context->CSSetShaderResources(0, 1, emitter_header_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShaderResources(1, 1, emitter_binary_shader_resource_view[current_buffer_index].GetAddressOf());

		context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::update_particles)]->get_shader().Get(), nullptr, 0);
		context.device_context->DispatchIndirect(indirect_data_buffer.Get(), UpdateParticleDispatchIndirect);
		clear_resources(context.device_context);
	}

	//	ソート処理
	{
		//	バイトニックソート
		//	https://www.bealto.com/gpu-sorting_parallel-bitonic-1.html
		context.device_context->CSSetUnorderedAccessViews(0, 1, particle_header_unordered_access_view.GetAddressOf(), nullptr);
		context.device_context->CSSetConstantBuffers(bitonic_sort_constant_buffer_index, 1, bitonic_sort_constant_buffer.GetAddressOf());
		UINT exponent = static_cast<UINT>(ceilf(log2f(static_cast<float>(total_particle_count))) + 0.5f);
		for (UINT i = 0; i < exponent; ++i)
		{
			context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::sort_b2)]->get_shader().Get(), nullptr, 0);
			UINT increment = 1 << i;
			for (UINT j = 0; j < i + 1; ++j)
			{
				bitonic_sort_constants	constant;
				constant.increment = increment;
				constant.direction = 2 << i;
				context.device_context->UpdateSubresource(bitonic_sort_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);

				if (increment <= BitonicSortC2Thread)
				{
					context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::sort_c2)]->get_shader().Get(), nullptr, 0);
					context.device_context->Dispatch(total_particle_count / 2 / BitonicSortC2Thread, 1, 1);
					break;
				}

				context.device_context->Dispatch(total_particle_count / 2 / BitonicSortB2Thread, 1, 1);
				increment /= 2;
			}
		}
		clear_resources(context.device_context);
	}

	//	範囲調査
	{
		context.device_context->CSSetUnorderedAccessViews(0, 1, emitter_range_unordered_access_view.GetAddressOf(), nullptr);

		context.device_context->CSSetShaderResources(0, 1, particle_header_shader_resource_view.GetAddressOf());

		context.device_context->CSSetConstantBuffers(range_particles_constant_buffer_index, 1, range_particles_constant_buffer.GetAddressOf());

		context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::range_particles)]->get_shader().Get(), nullptr, 0);

		//	全パーティクルをなめて範囲チェック
		for (int index = 0; index < 2; ++index)
		{
			range_particles_constants constant;
			constant.offset_index = index;
			context.device_context->UpdateSubresource(range_particles_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);

			context.device_context->Dispatch(total_particle_count / NumDispatchThread / 2, 1, 1);
		}
		clear_resources(context.device_context);
	}

	//	死亡パーティクルの番号をパーティクルインデックスバッファに返していく
	{
		context.device_context->CSSetUnorderedAccessViews(0, 1, particle_index_unordered_access_view[current_buffer_index].GetAddressOf(), nullptr);
		context.device_context->CSSetUnorderedAccessViews(1, 1, emitter_data_unordered_access_view.GetAddressOf(), nullptr);
		context.device_context->CSSetUnorderedAccessViews(2, 1, particle_header_unordered_access_view.GetAddressOf(), nullptr);

		context.device_context->CSSetShaderResources(0, 1, emitter_table_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShaderResources(1, 1, emitter_header_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShaderResources(2, 1, emitter_range_shader_resource_view.GetAddressOf());

		context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::terminate_particles)]->get_shader().Get(), nullptr, 0);
		context.device_context->Dispatch(1, 1, 1);
		clear_resources(context.device_context);
	}

	//	エミッター毎の描画情報を構築していく
	{
		context.device_context->CSSetUnorderedAccessViews(0, 1, particle_index_unordered_access_view[current_buffer_index].GetAddressOf(), nullptr);
		context.device_context->CSSetUnorderedAccessViews(1, 1, emitter_data_unordered_access_view.GetAddressOf(), nullptr);
		context.device_context->CSSetUnorderedAccessViews(2, 1, indirect_data_unordered_access_view.GetAddressOf(), nullptr);

		context.device_context->CSSetShaderResources(0, 1, emitter_table_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShaderResources(1, 1, emitter_header_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShaderResources(2, 1, emitter_range_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShaderResources(3, 1, emitter_binary_shader_resource_view[current_buffer_index].GetAddressOf());
		context.device_context->CSSetShaderResources(4, 1, particle_header_shader_resource_view.GetAddressOf());

		context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::build_emitter_draw_arguments)]->get_shader().Get(), nullptr, 0);
		context.device_context->Dispatch(1, 1, 1);	//	エミッターを並列で処理させる
		clear_resources(context.device_context);
	}

	//	頂点バッファ構築
	{
		context.device_context->CSSetShaderResources(0, 1, emitter_range_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShaderResources(1, 1, emitter_header_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShaderResources(2, 1, emitter_binary_shader_resource_view[current_buffer_index].GetAddressOf());
		context.device_context->CSSetShaderResources(3, 1, particle_header_shader_resource_view.GetAddressOf());
		context.device_context->CSSetShaderResources(4, 1, particle_binary_shader_resource_view[current_buffer_index].GetAddressOf());
		context.device_context->CSSetShader(compute_shaderes[static_cast<int>(dispatch_shaderes::build_primitive)]->get_shader().Get(), nullptr, 0);

		//	頂点バッファの登録ごとに処理
		for (UINT index = 0; index < emitter_id_table.size(); index += vertex_buffer_uav_max)
		{
			//	UAVと定数バッファ設定
			build_primitive_constants constant;
			ID3D11UnorderedAccessView* uavs[vertex_buffer_uav_max] = {};
			for (UINT id = 0; id < vertex_buffer_uav_max && id < emitter_id_table.size(); ++id)
			{
				UINT emitter_id = emitter_id_table[index + id];
				if (emitter_id == invalid_tag)
					continue;

				(&constant.emitter_ids[0].x)[id] = emitter_id;
				uavs[id] = emitter_resources[emitter_id].unordered_access_view.Get();
			}
			context.device_context->UpdateSubresource(build_primitive_constant_buffer.Get(), 0, nullptr, &constant, 0, 0);
			context.device_context->CSSetConstantBuffers(build_primitive_constant_buffer_index, 1, build_primitive_constant_buffer.GetAddressOf());
			context.device_context->CSSetUnorderedAccessViews(0, vertex_buffer_uav_max, uavs, nullptr);

			//	処理
			context.device_context->DispatchIndirect(indirect_data_buffer.Get(), UpdateParticleDispatchIndirect);
		}
		clear_resources(context.device_context);
	}
}

void compute_particle_system::render(graphics_context& context)
{
	context.clear_index_buffer();

	UINT strides[] = { sizeof(vertex_data) };
	UINT offsets[] = { 0 };

	//	定数バッファ設定
	context.device_context->VSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());
	context.device_context->PSSetConstantBuffers(common_pipeline_states::scene_cbv_index, 1, context.scene_constant_buffer.GetAddressOf());

	//	図形設定
	context.device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//	エミッター毎に処理する
	for (UINT index = 0; index < emitter_id_table.size(); ++index)
	{
		UINT emitter_id = emitter_id_table[index];
		if (emitter_id == invalid_tag)
			continue;

		auto& emitter_resource = emitter_resources[emitter_id];

		//	バッファ設定
		context.device_context->IASetVertexBuffers(0, 1, emitter_resource.buffer.GetAddressOf(), strides, offsets);

		//	シェーダー設定(場合によっては変更する)
		context.device_context->VSSetShader(default_draw_vertex_shader->get_shader().Get(), nullptr, 0);
		context.device_context->IASetInputLayout(default_draw_vertex_shader->get_input_layout().Get());
		context.device_context->PSSetShader(default_draw_pixel_shader->get_shader().Get(), nullptr, 0);

		//	リソース設定
		context.device_context->PSSetShaderResources(0, 1, emitter_resource.resource.render.texture->get_shader_resource_view().GetAddressOf());
		context.device_context->PSSetSamplers(0, 1, emitter_resource.resource.render.sampler_state.GetAddressOf());

		//	描画
		context.device_context->DrawInstancedIndirect(indirect_data_buffer.Get(),
			FixedIndirectDataSize + sizeof(draw_indirect) * emitter_id);
	}

	//	リソースクリア
	context.clear_shader_slots();
	context.clear_constant_buffers();
	context.clear_shader_resorce_views();
	context.clear_unordered_access_views();
}



#if 0



{
	//	リソース設定
	set_scene_constant(common_pipeline_states::scene_cbv_index);

	//	破棄テスト
	if (::GetAsyncKeyState('A') & 0x8000 && emitter_datas[Trail].emitter_index != 0xFFFFFFFF)
	{
		compute_particle->unregister_emitter(emitter_datas[Trail].emitter_index);
		emitter_datas[Trail].emitter_index = 0xFFFFFFFF;
	}
	if (::GetAsyncKeyState('S') & 0x8000 && emitter_datas[Bomb].emitter_index != 0xFFFFFFFF)
	{
		compute_particle->unregister_emitter(emitter_datas[Bomb].emitter_index);
		emitter_datas[Bomb].emitter_index = 0xFFFFFFFF;
	}

	//	更新回数を算出
	static float accumulate_time = 0;
	accumulate_time += elapsed_time;
	UINT loop_max = 0;
	if (particle_update_limit_fps60)
	{
		if (accumulate_time >= 1 / 60.0f)
		{
			loop_max = static_cast<UINT>(accumulate_time * 60);
			accumulate_time -= loop_max / 60.0f;
		}
	}
	else
	{
		accumulate_time = 0;
		loop_max = 1;
	}

	//	パーティクル更新
	for (UINT index = 0; index < loop_max; ++index)
	{
		//	経過時間
		if (particle_update_limit_fps60)
			elapsed_time = 1 / 60.0f;
#if 01
		//	爆発処理
		if (emitter_datas[Bomb].emitter_index != 0xFFFFFFFF)
		{
			auto& emitter_data = emitter_datas[Bomb];
			particle_system_item::emitter::common common_data;
			{
				//	パーティクル生成情報
				common_data.emitter_state;

				//	エミッター姿勢情報
				common_data.scale = emitter_data.scale;
				DirectX::XMStoreFloat4(&common_data.rotation,
					DirectX::XMQuaternionRotationRollPitchYaw(emitter_data.rotation.x, emitter_data.rotation.y, emitter_data.rotation.z));
				common_data.translation = emitter_data.translation;

				//	パーティクル発生情報
				common_data.spawn_life_time_range = { 1.5f, 2.5f };
				common_data.spawn_scale_range = { 3.0f, 6.0f };
				if ((::GetAsyncKeyState('X') & 0x8000))
					common_data.spawn_count = 40;
				else
					common_data.spawn_count = 0;

				common_data.spawn_area_min = { -40, 0, -40 };
				common_data.spawn_area_max = { +40, 0, +40 };
				common_data.gravity = { 0.0f, -9.8f, 0.0f };
				common_data.gravity = { 0.0f, 0.0f, 0.0f };
			}

			//	エミッター更新情報登録
			compute_particle_system::update_emitter_data data;

			//	各種アイテム登録
			{
				UINT offset = 0;
				//	スプライトアニメーションアイテム登録
				{
					particle_system_item::emitter::sprite_animation sprite_animation;
					sprite_animation.split_count = { 5, 5 };
					sprite_animation.increment_time = 1.0f / 25;
					UINT write_position = static_cast<UINT>(data.item_binary.size());
					UINT write_size = sizeof(particle_system_item::emitter::sprite_animation);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&sprite_animation, sizeof(sprite_animation));
					common_data.active_items |= 1 << particle_system_item::emitter::sprite_animation::data_id;
				}

				//	速度アイテム登録
				{
					particle_system_item::emitter::velocity velocity;
					velocity.angle_range = { 0.5f, 1.5f };
					velocity.direction.x = static_cast<float>(rand() % 100 - 50) * 0.01f;
					velocity.direction.y = static_cast<float>(rand() % 100 - 50) * 0.01f;
					velocity.direction.z = static_cast<float>(rand() % 100 - 50) * 0.01f;
					float length = 0;
					length = sqrtf(velocity.direction.x * velocity.direction.x +
						velocity.direction.y * velocity.direction.y +
						velocity.direction.z * velocity.direction.z);
					velocity.direction.x /= length;
					velocity.direction.y /= length;
					velocity.direction.z /= length;
					velocity.power_range = { 2.5f, 5.0f };
					velocity.acceleration.x = static_cast<float>(rand() % 100 - 50) * 0.1f;
					velocity.acceleration.y = static_cast<float>(rand() % 100 - 50) * 0.1f;
					velocity.acceleration.z = static_cast<float>(rand() % 100 - 50) * 0.1f;
					length = sqrtf(velocity.acceleration.x * velocity.acceleration.x +
						velocity.acceleration.y * velocity.acceleration.y +
						velocity.acceleration.z * velocity.acceleration.z);
					velocity.acceleration.x /= length;
					velocity.acceleration.y /= length;
					velocity.acceleration.z /= length;
					if (emitter_data.emitter_index == 0)
					{
						velocity.angle_range = { 0.0f, 0.5f };
						velocity.power_range = { 0, 0.1f };
					}

					UINT write_position = static_cast<UINT>(data.item_binary.size());
					UINT write_size = sizeof(particle_system_item::emitter::velocity);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&velocity, sizeof(velocity));
					common_data.active_items |= 1 << particle_system_item::emitter::velocity::data_id;
				}
			}
			data.common = common_data;

			//	エミッター更新アイテム登録
			compute_particle->update_emitter(emitter_data.emitter_index, data);
		}

		//	軌跡処理
		if (emitter_datas[Trail].emitter_index != 0xFFFFFFFF)
		{
			auto& emitter_data = emitter_datas[Trail];
			particle_system_item::emitter::common common_data;
			{
				//	パーティクル生成情報
				common_data.emitter_state;

				//	エミッター姿勢情報
				common_data.scale = emitter_data.scale;
				DirectX::XMStoreFloat4(&common_data.rotation,
					DirectX::XMQuaternionRotationRollPitchYaw(emitter_data.rotation.x, emitter_data.rotation.y, emitter_data.rotation.z));
				common_data.translation = emitter_data.translation;

				//	パーティクル発生情報
				common_data.spawn_life_time_range = { 1.5f, 2.5f };
				common_data.spawn_scale_range = { 0.15f, 0.5f };
				if ((::GetAsyncKeyState('Z') & 0x8000))
					common_data.spawn_count = 40;
				else
					common_data.spawn_count = 0;

				common_data.spawn_area_min = { -40, 0, -40 };
				common_data.spawn_area_max = { +40, 0, +40 };
				common_data.gravity = { 0.0f, -9.8f, 0.0f };
			}

			//	エミッター更新情報登録
			compute_particle_system::update_emitter_data data;

			//	各種アイテム登録
			{
				UINT offset = 0;
				//	軌跡アイテム登録
				{
					particle_system_item::emitter::trail trail;
					trail.max = TRAIL_BODY_MAX;

					UINT write_position = static_cast<UINT>(data.item_binary.size());
					UINT write_size = sizeof(particle_system_item::emitter::trail);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&trail, sizeof(trail));
					common_data.active_items |= 1 << particle_system_item::emitter::trail::data_id;
				}

				//	速度アイテム登録
				{
					particle_system_item::emitter::velocity velocity;
					velocity.angle_range = { 0.5f, 1.5f };
					velocity.direction.x = static_cast<float>(rand() % 100 - 50) * 0.01f;
					velocity.direction.y = static_cast<float>(rand() % 100 - 50) * 0.01f;
					velocity.direction.z = static_cast<float>(rand() % 100 - 50) * 0.01f;
					float length = 0;
					length = sqrtf(velocity.direction.x * velocity.direction.x +
						velocity.direction.y * velocity.direction.y +
						velocity.direction.z * velocity.direction.z);
					velocity.direction.x /= length;
					velocity.direction.y /= length;
					velocity.direction.z /= length;
					velocity.power_range = { 2.5f, 5.0f };
					velocity.acceleration.x = static_cast<float>(rand() % 100 - 50) * 0.1f;
					velocity.acceleration.y = static_cast<float>(rand() % 100 - 50) * 0.1f;
					velocity.acceleration.z = static_cast<float>(rand() % 100 - 50) * 0.1f;
					length = sqrtf(velocity.acceleration.x * velocity.acceleration.x +
						velocity.acceleration.y * velocity.acceleration.y +
						velocity.acceleration.z * velocity.acceleration.z);
					velocity.acceleration.x /= length;
					velocity.acceleration.y /= length;
					velocity.acceleration.z /= length;
					if (emitter_data.emitter_index == 0)
					{
						velocity.angle_range = { 0.0f, 0.5f };
						velocity.power_range = { 0, 0.1f };
					}

					UINT write_position = static_cast<UINT>(data.item_binary.size());
					UINT write_size = sizeof(particle_system_item::emitter::velocity);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&velocity, sizeof(velocity));
					common_data.active_items |= 1 << particle_system_item::emitter::velocity::data_id;
				}
			}
			data.common = common_data;

			//	エミッター更新アイテム登録
			compute_particle->update_emitter(emitter_data.emitter_index, data);
		}
#else
		//	エミッター情報更新
		for (UINT emitter_index = 0; emitter_index < Max; ++emitter_index)
		{
			auto& emitter_data = emitter_datas[emitter_index];
			particle_system_item::emitter::common common_data;
			{
				//	パーティクル生成情報
				common_data.emitter_state;

				//	エミッター姿勢情報
				common_data.scale = emitter_data.scale;
				DirectX::XMStoreFloat4(&common_data.rotation,
					DirectX::XMQuaternionRotationRollPitchYaw(emitter_data.rotation.x, emitter_data.rotation.y, emitter_data.rotation.z));
				common_data.translation = emitter_data.translation;

				//	パーティクル発生情報
				common_data.spawn_life_time_range = { 2.5f - 0.001f, 2.5f };
				common_data.spawn_scale_range = { 0.15f, 0.5f };
				common_data.spawn_count = 40;

				common_data.spawn_area_min = { -40, 0, -40 };
				common_data.spawn_area_max = { +40, 0, +40 };
				common_data.gravity = { 0.0f, -9.8f, 0.0f };
				if (emitter_data.emitter_index == 0)
				{
					common_data.gravity = { 0.0f, 0.0f, 0.0f };
					common_data.spawn_scale_range = { 3.0f, 6.0f };
				}
			}

			//	エミッター更新情報登録
			compute_particle_system::update_emitter_data data;

			//	各種アイテム登録
			{
				UINT offset = 0;

				//	軌跡アイテム登録
				if (emitter_index == Trail)
				{
					particle_system_item::emitter::trail trail;
					trail.max = TRAIL_BODY_MAX;

					UINT write_position = data.item_binary.size();
					UINT write_size = sizeof(particle_system_item::emitter::trail);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&trail, sizeof(trail));
					common_data.active_items |= 1 << particle_system_item::emitter::trail::data_id;
				}

				//	スプライトアニメーションアイテム登録
				if (emitter_index == Bomb)
				{
					particle_system_item::emitter::sprite_animation sprite_animation;
					sprite_animation.split_count = {5, 5};
					sprite_animation.increment_time = 1.0f / 25;
					UINT write_position = data.item_binary.size();
					UINT write_size = sizeof(particle_system_item::emitter::sprite_animation);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&sprite_animation, sizeof(sprite_animation));
					common_data.active_items |= 1 << particle_system_item::emitter::sprite_animation::data_id;
				}

				//	速度アイテム登録
				{
					particle_system_item::emitter::velocity velocity;
					velocity.angle_range = { 0.5f, 1.5f };
					velocity.direction.x = static_cast<float>(rand() % 100 - 50) * 0.01f;
					velocity.direction.y = static_cast<float>(rand() % 100 - 50) * 0.01f;
					velocity.direction.z = static_cast<float>(rand() % 100 - 50) * 0.01f;
					float length = 0;
					length = sqrtf(velocity.direction.x * velocity.direction.x +
						velocity.direction.y * velocity.direction.y +
						velocity.direction.z * velocity.direction.z);
					velocity.direction.x /= length;
					velocity.direction.y /= length;
					velocity.direction.z /= length;
					velocity.power_range = { 2.5f, 5.0f };
					velocity.acceleration.x = static_cast<float>(rand() % 100 - 50) * 0.1f;
					velocity.acceleration.y = static_cast<float>(rand() % 100 - 50) * 0.1f;
					velocity.acceleration.z = static_cast<float>(rand() % 100 - 50) * 0.1f;
					length = sqrtf(velocity.acceleration.x * velocity.acceleration.x +
						velocity.acceleration.y * velocity.acceleration.y +
						velocity.acceleration.z * velocity.acceleration.z);
					velocity.acceleration.x /= length;
					velocity.acceleration.y /= length;
					velocity.acceleration.z /= length;
					if (emitter_data.emitter_index == 0)
					{
						velocity.angle_range = { 0.0f, 0.5f };
						velocity.power_range = { 0, 0.1f };
					}

					UINT write_position = data.item_binary.size();
					UINT write_size = sizeof(particle_system_item::emitter::velocity);
					data.item_binary.resize(write_position + write_size);
					memcpy_s(data.item_binary.data() + write_position, data.item_binary.size() - write_position,
						&velocity, sizeof(velocity));
					common_data.active_items |= 1 << particle_system_item::emitter::velocity::data_id;
				}
			}
			data.common = common_data;

			//	エミッター更新アイテム登録
			compute_particle->update_emitter(emitter_data.emitter_index, data);
		}
#endif
		//	パーティクルシステム更新
		get_renderdata()->debug_begin_event(L"Update Particle");
		compute_particle->update(get_renderdata()->context.device_context.Get(), elapsed_time);
		get_renderdata()->debug_end_event();
	}
}
#endif