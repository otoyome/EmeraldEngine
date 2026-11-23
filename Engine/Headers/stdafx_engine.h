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

#include "Core/hash.h"
#include "Core/uuid.h"

#include "system/misc.h"
#include "Core/high_resolution_timer.h"
#include "system/enum_class_attribute.h"
#include "system/system_units.h"
#include <fstream>
#include <d3d11.h>
#include <wrl.h>
#include <cassert>
#include "cereal/cereal.hpp"
#include "magic_enum/include/magic_enum/magic_enum.hpp"
#include "Core/logger.h"
#include "Core/cache_string.h"
#include "Core/charset.h"
#include "Core/string_utility.h"
#include "Core/path_utility.h"
#include "Core/bit_flag.h"
#include "Core/container_utility.h"
#include "Math/math_all.h"
#include "Math/math_serialize.h"
#include "Math/math_all.h"
#include "system/singleton.h"
#include "system/simple_helper.h"
#include "system/engine_asset_path.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl.h>
#include <cassert>


#include "Graphics/Core/render_data.h"
#include "Graphics/Pipeline/shader.h"
#include "Graphics/Core/graphics_utility.h"

#include "Material/material_resource.h"

#include "input/mouse.h"
#include "input/keyboard.h"
#include "input/gamepad.h"
#include "input/input_key.h"

#include "Graphics/Core/direct_x_helper.h"

#if	defined( USE_IMGUI )
#define	IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#endif	//	defined( USE_IMGUI )
