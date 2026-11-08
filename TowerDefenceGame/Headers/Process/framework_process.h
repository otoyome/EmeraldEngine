#pragma once

#include <memory>

class framework;

//	フレームワークプロセス
class framework_process
{
public:
	explicit framework_process(framework& owner) : owner(owner) {}
	virtual ~framework_process() = default;

	framework_process(const framework_process&) = delete;
	framework_process& operator=(const framework_process&) = delete;

public:
	virtual bool enter() { return true; }
	virtual void exit() {}
	virtual void update(float elapsed_time) {}
	virtual void render(float elapsed_time) {}
#if defined(USE_IMGUI)
	//	@tool エディター窓の表示
	virtual void imgui_render() {}
#endif	//	defined(USE_IMGUI)
	virtual std::unique_ptr<framework_process> next_process() { return nullptr; }

protected:
	framework& get_owner() { return owner; }

private:
	framework& owner;
};
