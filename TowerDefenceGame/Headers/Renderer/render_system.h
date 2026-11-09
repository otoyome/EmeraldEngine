#pragma once

#include <functional>
#include <memory>

#include "Renderer/render_job_executor.h"

//	•`‰æƒVƒXƒeƒ€
class render_system
{
public:
	using overlay_function = std::function<void()>;

public:
	render_system();
	~render_system();

	render_system(const render_system&) = delete;
	render_system& operator=(const render_system&) = delete;
	render_system(render_system&&) noexcept = delete;
	render_system& operator=(render_system&&) noexcept = delete;

	const std::shared_ptr<render_job_executor>& get_executor() const { return executor; }

	//	
	void set_overlay(overlay_function overlay);

	void begin_frame();
	void end_frame();

private:
	std::shared_ptr<render_job_executor> executor;
	overlay_function overlay;
};
