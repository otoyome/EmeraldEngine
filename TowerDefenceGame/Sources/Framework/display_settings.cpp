#include "Framework/display_settings.h"

#include <algorithm>

#include "Framework/app_window.h"
#include "Graphics/render_data.h"

display_settings::display_settings(app_window& window)
: window(window)
{
	//	@tool 今のバックバッファの大きさ
	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
	render_data::instance()->get_swap_chain()->GetDesc1(&swap_chain_desc);
	current_resolution = { swap_chain_desc.Width, swap_chain_desc.Height };

	collect_resolutions();

	//	@tool 保存されている設定と違えばすぐに切り替える（作成時はまだバックバッファの描画先をコピーしている側がいないので、2段階にしなくてよい）
	const engine_config_data::windows_data& windows = engine_config_data::instance()->windows;
	const resolution saved{ windows.screen_width, windows.screen_height };
	if (saved.width > 0 && saved.height > 0 && (saved != current_resolution || windows.window_mode != window_mode))
	{
		window.apply_window_mode(windows.window_mode, static_cast<int>(saved.width), static_cast<int>(saved.height));
		if (render_data::instance()->resize_back_buffer(saved.width, saved.height))
		{
			current_resolution = saved;
			window_mode = windows.window_mode;
		}
		else
		{
			window.apply_window_mode(window_mode, static_cast<int>(current_resolution.width), static_cast<int>(current_resolution.height));
		}
	}
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
			//	@tool リフレッシュレート違いなどで同じ大きさが並ぶので1つにまとめる
			for (const DXGI_MODE_DESC& mode : modes)
				resolutions.push_back({ mode.Width, mode.Height });
		}
	}

	//	@tool 今の大きさは必ず選べるようにする
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

void display_settings::update()
{
	if (!request || !render_data::is_valid())
		return;

	render_data& data = *render_data::instance();

	//	@tool 1フレーム目：描画パスを空にする。このフレームで使う側（world_renderer など）が描画先のコピーを手放す
	if (phase == phases::idle)
	{
		data.release_back_buffer();
		phase = phases::released;
		return;
	}

	//	@tool 2フレーム目：ウィンドウを合わせてからバックバッファを作り直す
	const change_request next = *request;
	request.reset();
	phase = phases::idle;

	window.apply_window_mode(next.mode, static_cast<int>(next.size.width), static_cast<int>(next.size.height));
	if (!data.resize_back_buffer(next.size.width, next.size.height))
	{
		//	@tool 失敗したらウィンドウを元に戻す（バックバッファは元の大きさのまま）
		window.apply_window_mode(window_mode, static_cast<int>(current_resolution.width), static_cast<int>(current_resolution.height));
		return;
	}

	current_resolution = next.size;
	window_mode = next.mode;

	engine_config_data::windows_data& windows = engine_config_data::instance()->windows;
	windows.screen_width = current_resolution.width;
	windows.screen_height = current_resolution.height;
	windows.window_mode = window_mode;
}
