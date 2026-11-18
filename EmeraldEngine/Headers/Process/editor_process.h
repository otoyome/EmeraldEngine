#pragma once

#if defined(USE_IMGUI)
#include "Process/framework_process.h"

//	エディタープロセス
class editor_process : public framework_process
{
public:
	using framework_process::framework_process;

	orders get_order() const override { return orders::editor; }

	bool enter() override;
	void exit() override;
	void update(float elapsed_time) override;
	void imgui_render() override;
	void release_display_resources() override;
	bool create_display_resources() override;
};
#endif	//	defined(USE_IMGUI)
