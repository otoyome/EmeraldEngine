#pragma once

#include "Graphics/common_pipeline_states.h"
#include "Material/material_instance.h"

//	描画用カスタムパイプラインステート
class graphics_custom_pipeline_state : public graphics_pipeline_state
{
	template<typename Shader> const std::shared_ptr<Shader>& get_internal_shader() const {return std::shared_ptr<Shader>();}
	template<> const std::shared_ptr<vertex_shader>& get_internal_shader<vertex_shader>() const {return vs;}
	template<> const std::shared_ptr<hull_shader>& get_internal_shader<hull_shader>() const {return hs;}
	template<> const std::shared_ptr<domain_shader>& get_internal_shader<domain_shader>() const {return ds;}
	template<> const std::shared_ptr<geometry_shader>& get_internal_shader<geometry_shader>() const {return gs;}
	template<> const std::shared_ptr<pixel_shader>& get_internal_shader<pixel_shader>() const {return ps;}

	enum class arrangement
	{
		vertex,
		hull,
		domain,
		geometry,
		pixel,
	};

	struct index_map
	{
		struct indices
		{
			UINT ids[magic_enum::enum_count<arrangement>()];
			system_utility::hash_key hash = -1;
			indices()
			{
				for (auto& index : ids)
				{
					index = -1;
				}
			}
		};
		std::vector<std::pair<std::string, indices>> cbv;
		std::vector<std::pair<std::string, indices>> srv;
		std::vector<std::pair<std::string, indices>> sampler;
		std::vector<int> srv_sampler_indices;
		void clear()
		{
			cbv.clear();
			srv.clear();
			sampler.clear();
			srv_sampler_indices.clear();
		}
	};

public:
	graphics_custom_pipeline_state();
	~graphics_custom_pipeline_state() override;

	void rebuild_bindings() override;
	void begin(const graphics_context& context) override;
	void update_parameters(const graphics_context& context, const material_instance& material, const buffer_datas* buffer_data) override;
	void end(const graphics_context& context) override;

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
	UINT get_stencil_ref() const { return stencil_ref; }

private:
	void update_register_datas();

#if defined(USE_IMGUI)
	void on_update_core(const shader_base* shader_base) override;

	bool is_failed() override;
#endif	//	defined(USE_IMGUI)

private:
	bool dirty = true;
	bool dirty_input_resource_datas = true;
	UINT stencil_ref = 0;
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<hull_shader> hs;
	std::shared_ptr<domain_shader> ds;
	std::shared_ptr<geometry_shader> gs;
	std::shared_ptr<pixel_shader> ps;

	index_map index_map;
	std::vector<input_resource_desc> input_resource_desces;

	struct constant_buffer_data
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	};
	constant_buffer_data deferred_light_buffer_data;
	constant_buffer_data deferred_light_probe_buffer_data;
	constant_buffer_data forward_light_buffer_data;
	constant_buffer_data shadow_map_buffer_data;
	constant_buffer_data parameter_buffer_data[magic_enum::enum_count<arrangement>()];

	//	本来は無くて良い。良いのだが面倒なのでここでやる
	struct primitive_datas
	{
		bool use;
		UINT cbv_indices[magic_enum::enum_count<arrangement>()];
	};

#if defined(USE_IMGUI)
	UINT vs_reload_callback_id = UINT_MAX;
	UINT hs_reload_callback_id = UINT_MAX;
	UINT ds_reload_callback_id = UINT_MAX;
	UINT gs_reload_callback_id = UINT_MAX;
	UINT ps_reload_callback_id = UINT_MAX;
#endif	//	defined(USE_IMGUI)
};
