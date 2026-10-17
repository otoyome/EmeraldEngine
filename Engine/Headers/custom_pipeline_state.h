#pragma once

#include "common_pipeline_states.h"
#include "material.h"

//	描画用カスタムパイプラインステート
//	@tool シェーダーのリフレクションから割り当て配置を作り、ステートはマテリアルの設定から描画毎に選ぶ
class graphics_custom_pipeline_state : public graphics_pipeline_state
{
	template<typename Shader> const std::shared_ptr<Shader>& get_internal_shader() const {return std::shared_ptr<Shader>();}
	template<> const std::shared_ptr<vertex_shader>& get_internal_shader<vertex_shader>() const {return shaders.vs;}
	template<> const std::shared_ptr<hull_shader>& get_internal_shader<hull_shader>() const {return shaders.hs;}
	template<> const std::shared_ptr<domain_shader>& get_internal_shader<domain_shader>() const {return shaders.ds;}
	template<> const std::shared_ptr<geometry_shader>& get_internal_shader<geometry_shader>() const {return shaders.gs;}
	template<> const std::shared_ptr<pixel_shader>& get_internal_shader<pixel_shader>() const {return shaders.ps;}

	enum class arrangement
	{
		vertex,
		hull,
		domain,
		geometry,
		pixel,

		max,
	};

public:
	graphics_custom_pipeline_state();
	~graphics_custom_pipeline_state() override;

	void prepare() override;
	bool needs_prepare() const override { return dirty || dirty_input_resource_datas; }

	void generate_input_resource_descs(std::vector<input_resource_desc>& out_data) override;

public:
	inline bool is_dirty() const {return dirty;}

	void set_shader(const std::shared_ptr<vertex_shader>& shader);
	void set_shader(const std::shared_ptr<hull_shader>& shader);
	void set_shader(const std::shared_ptr<domain_shader>& shader);
	void set_shader(const std::shared_ptr<geometry_shader>& shader);
	void set_shader(const std::shared_ptr<pixel_shader>& shader);

	template<typename Shader>
	inline std::shared_ptr<Shader>& get_shader() const {return get_internal_shader<Shader>();}

	void set_stencil_ref(UINT stencil_ref) {this->stencil_ref = stencil_ref;}

private:
	void update_register_datas();

#if defined(USE_IMGUI)
	void on_update_core(const shader_base* shader_base) override;
#endif	//	defined(USE_IMGUI)

private:
	bool dirty = true;
	bool dirty_input_resource_datas = true;

	std::vector<input_resource_desc> input_resource_desces;

#if defined(USE_IMGUI)
	UINT vs_reload_callback_id = UINT_MAX;
	UINT hs_reload_callback_id = UINT_MAX;
	UINT ds_reload_callback_id = UINT_MAX;
	UINT gs_reload_callback_id = UINT_MAX;
	UINT ps_reload_callback_id = UINT_MAX;
#endif	//	defined(USE_IMGUI)
};
