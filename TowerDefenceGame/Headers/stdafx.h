#pragma once

#define	NOMINMAX
#include <windows.h>
#include <tchar.h>
#include <sstream>
#include <algorithm>

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>

#include "system/misc.h"
#include "system/high_resolution_timer.h"
#include "system/enum_class_attribute.h"
#include "system/singleton.h"
#include "system/system_units.h"
#include "system/system_utility.h"
#include "Math/math_all.h"
#include "system/simple_helper.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl.h>
#include <cassert>

#include "nameof.hpp"
#include "magic_enum.hpp"
#include "render_data.h"
#include "shader.h"
#include "graphics_utility.h"

#include "material_resource.h"

#include "input/mouse.h"
#include "input/keyboard.h"
#include "input/gamepad.h"
#include "input/input_key.h"

#include "Graphics/direct_x_helper.h"

#if	defined( USE_IMGUI )
#define	IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "ImGuiExternal/imgui_external.h"
#endif	//	defined( USE_IMGUI )

#include "Graphics/debug_primitive_collector.h"

#include "object/object_manager.h"
