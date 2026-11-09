#pragma once

#include "Process/framework_process.h"

//	起動時に最低限必要なシェーダーコンパイルのプロセス
class boot_shader_compile_process : public framework_process
{
public:
	using framework_process::framework_process;

	bool enter() override;
	std::unique_ptr<framework_process> next_process() override;
};
