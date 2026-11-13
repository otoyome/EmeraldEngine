#include "Framework/display_settings.h"

#include <algorithm>

#include "Graphics/Core/render_data.h"

display_settings::display_settings()
{
	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
	render_data::instance()->get_swap_chain()->GetDesc1(&swap_chain_desc);
	current_resolution = { swap_chain_desc.Width, swap_chain_desc.Height };

	collect_resolutions();
}

display_settings::~display_settings()
{
}

void display_settings::collect_resolutions()
{
	resolutions.clear();

	Microsoft::WRL::ComPtr<IDXGIOutput> output;
	if (SUCCEEDED(render_data::instance()->get_swap_chain()->GetContainingOutput(output.GetAddressOf())))
	{
		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
		render_data::instance()->get_swap_chain()->GetDesc1(&swap_chain_desc);

		UINT count = 0;
		output->GetDisplayModeList(swap_chain_desc.Format, 0, &count, nullptr);
		std::vector<DXGI_MODE_DESC> modes(count);
		if (count > 0 && SUCCEEDED(output->GetDisplayModeList(swap_chain_desc.Format, 0, &count, modes.data())))
		{
			//	リフレッシュレート違いなどで同じ大きさが並ぶので1つにまとめる
			for (const DXGI_MODE_DESC& mode : modes)
				resolutions.push_back({ mode.Width, mode.Height });
		}
	}

	resolutions.push_back(current_resolution);

	std::sort(resolutions.begin(), resolutions.end(),
		[](const resolution& l, const resolution& r) { return l.width != r.width ? l.width < r.width : l.height < r.height; });
	resolutions.erase(std::unique(resolutions.begin(), resolutions.end()), resolutions.end());
}

void display_settings::request_change(const resolution& size, window_modes mode)
{
	if (size.width == 0 || size.height == 0)
		return;
	request = change_request{ size, mode };
}

std::optional<display_settings::change_request> display_settings::take_request()
{
	std::optional<change_request> result = request;
	request.reset();
	return result;
}

void display_settings::set_current(const resolution& size, window_modes mode)
{
	current_resolution = size;
	window_mode = mode;

	engine_config_data::windows_data& windows = engine_config_data::instance()->windows;
	windows.screen_width = current_resolution.width;
	windows.screen_height = current_resolution.height;
	windows.window_mode = window_mode;
}

std::optional<display_settings::change_request> display_settings::get_saved_request() const
{
	const engine_config_data::windows_data& windows = engine_config_data::instance()->windows;
	const resolution saved{ windows.screen_width, windows.screen_height };
	if (saved.width == 0 || saved.height == 0)
		return std::nullopt;
	if (saved == current_resolution && windows.window_mode == window_mode)
		return std::nullopt;
	return change_request{ saved, windows.window_mode };
}
