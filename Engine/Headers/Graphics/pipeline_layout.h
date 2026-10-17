#pragma once

#include "texture.h"

//	@tool シェーダーステージ（定数・リソースを割り当てる対象）
enum class shader_stage : UINT
{
	vertex,
	hull,
	domain,
	geometry,
	pixel,
	compute,

	max,
};

//	@tool 描画パイプラインで扱うステージ数（頂点からピクセルまで）
constexpr UINT graphics_shader_stage_count = static_cast<UINT>(shader_stage::pixel) + 1;

//	@tool 割り当て内容をどこから取るか
enum class binding_source : uint8_t
{
	table,		//	割り当て表（パイプラインの設定時に割り当てる）
	material,	//	割り当て表 → マテリアルの順に探す（描画毎に割り当てる）
	parameter,	//	パラメーター定数（マテリアルとテクスチャの有無から作る）
	primitive,	//	描画単位の定数（buffer_datas）
	fixed,		//	パイプラインが持つ固定のリソース（サンプラー・テクスチャ）
};

//	@tool 割り当て1つ分（名前 1 つに対し、ステージ毎のスロット番号を持つ）
struct pipeline_binding
{
	static constexpr UINT unused_slot = UINT_MAX;

	std::string name;
	system_utility::hash_key key = 0;
	binding_source source = binding_source::table;
	UINT slots[graphics_shader_stage_count] = { unused_slot, unused_slot, unused_slot, unused_slot, unused_slot };

	//	@tool SRV と対になるサンプラーの番号（samplers の添字。無ければ -1）
	int paired_sampler = -1;

	//	@tool source が fixed の場合の中身
	Microsoft::WRL::ComPtr<ID3D11SamplerState> fixed_sampler;
	texture_ptr fixed_texture;
};

//	@tool パイプラインの割り当て配置（どのスロットに何を割り当てるか）
//	@tool シェーダーのリフレクションから作るか、固定のパイプラインでは明示的に並べる。作った後は変更しない
class pipeline_layout
{
public:
	void clear();

	//	@tool 明示的な指定（固定のパイプライン用）
	void add_constant_buffer(shader_stage stage, UINT slot, system_utility::hash_key key, binding_source source = binding_source::table);
	void add_shader_resource(shader_stage stage, UINT slot, system_utility::hash_key key, binding_source source = binding_source::table);
	void add_shader_resource(shader_stage stage, UINT slot, const texture_ptr& texture);
	void add_sampler(shader_stage stage, UINT slot, const Microsoft::WRL::ComPtr<ID3D11SamplerState>& sampler);

	//	@tool リフレクションからの登録（名前で纏め、finalize_reflection で取得元と対のサンプラーを決める）
	void add_reflected_constant_buffer(shader_stage stage, UINT slot, const std::string& name);
	void add_reflected_shader_resource(shader_stage stage, UINT slot, const std::string& name);
	void add_reflected_sampler(shader_stage stage, UINT slot, const std::string& name);
	void finalize_reflection();

	const std::vector<pipeline_binding>& get_constant_buffers() const { return constant_buffers; }
	const std::vector<pipeline_binding>& get_shader_resources() const { return shader_resources; }
	const std::vector<pipeline_binding>& get_samplers() const { return samplers; }

private:
	static pipeline_binding& acquire(std::vector<pipeline_binding>& bindings, const std::string& name, system_utility::hash_key key, binding_source source);

private:
	std::vector<pipeline_binding> constant_buffers;
	std::vector<pipeline_binding> shader_resources;
	std::vector<pipeline_binding> samplers;
};
