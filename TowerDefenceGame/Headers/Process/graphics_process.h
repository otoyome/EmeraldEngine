#pragma once

#include "Process/framework_process.h"
#include "Framework/display_settings.h"

//	•`‰æŠÖŒWƒvƒƒZƒX
class graphics_process : public framework_process
{
public:
	using framework_process::framework_process;

	orders get_order() const override { return orders::graphics; }

	bool enter() override;
	void exit() override;
	void update(float elapsed_time) override;

private:
	bool apply_display(const display_settings::change_request& request);
};
