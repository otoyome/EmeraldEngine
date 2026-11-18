#pragma once

#include <queue>

#include "renderer_domain.h"
#include "renderer_context.h"
#include "render_job_executor.h"


//	描画担当
class renderer
{
public:
	virtual renderer_domain get_domain() const = 0;
	UINT get_render_basis_sort_order() const {return static_cast<int>(get_domain()) * 10000;}

public:
	renderer(render_job_registrar job_executor) : job_executor(job_executor){}
	virtual ~renderer() {}

	virtual void render(renderer_context& context) = 0;
	virtual void resize_buffer(int width, int height) = 0;

	//	プロファイラー表示用の名前
	std::string_view get_profile_name() const { return job_executor.get_owner_name(); }
	void set_profile_name(std::string_view name) { job_executor.set_owner_name(name); }

protected:
	render_job_registrar job_executor;
};
