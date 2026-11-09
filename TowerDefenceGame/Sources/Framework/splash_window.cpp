#include "Framework/splash_window.h"

#include <algorithm>
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")

namespace
{
	constexpr const wchar_t* class_name = L"SplashWindow";
	constexpr UINT redraw_message = WM_APP + 1;	//	再描画の依頼
	constexpr int default_width = 640;			//	画像が読めなかった時の大きさ
	constexpr int default_height = 360;
	constexpr int bar_margin = 24;				//	進捗バーの左右の余白
	constexpr int bar_height = 6;				//	進捗バーの高さ
	constexpr int bar_bottom = 24;				//	進捗バーの下からの位置
}

splash_window::~splash_window()
{
	close();
}

bool splash_window::show(HINSTANCE instance, const wchar_t* image_path)
{
	if (thread.joinable())
		return true;

	ready = false;
	thread = std::thread(&splash_window::thread_main, this, instance, std::wstring(image_path));

	//	ウィンドウができるまで待つ
	while (!ready)
		std::this_thread::yield();
	return hwnd != nullptr;
}

void splash_window::close()
{
	if (!thread.joinable())
		return;

	if (HWND target = hwnd.load())
		PostMessageW(target, WM_CLOSE, 0, 0);
	thread.join();
}

void splash_window::set_progress(float value)
{
	progress = std::clamp(value, 0.0f, 1.0f);
	if (HWND target = hwnd.load())
		PostMessageW(target, redraw_message, 0, 0);
}

void splash_window::set_text(std::wstring value)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		text = std::move(value);
	}
	if (HWND target = hwnd.load())
		PostMessageW(target, redraw_message, 0, 0);
}

void splash_window::thread_main(HINSTANCE instance, std::wstring image_path)
{
	//	このスレッドで WIC を使う
	const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	image = load_image(image_path.c_str(), image_width, image_height);
	if (!image)
	{
		image_width = default_width;
		image_height = default_height;
	}

	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(wcex);
	wcex.lpfnWndProc = &splash_window::window_procedure;
	wcex.hInstance = instance;
	wcex.hCursor = LoadCursor(nullptr, IDC_APPSTARTING);
	wcex.lpszClassName = class_name;
	RegisterClassExW(&wcex);

	//	画面の中央に、枠の無いウィンドウを出す
	const int x = (GetSystemMetrics(SM_CXSCREEN) - image_width) / 2;
	const int y = (GetSystemMetrics(SM_CYSCREEN) - image_height) / 2;
	HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, class_name, L"", WS_POPUP,
		x, y, image_width, image_height, nullptr, nullptr, instance, this);
	hwnd = window;
	ready = true;

	if (window)
	{
		ShowWindow(window, SW_SHOWNOACTIVATE);
		UpdateWindow(window);

		MSG msg{};
		while (GetMessageW(&msg, nullptr, 0, 0) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	hwnd = nullptr;
	if (image)
		DeleteObject(image);
	image = nullptr;
	UnregisterClassW(class_name, instance);
	if (SUCCEEDED(com_result))
		CoUninitialize();
}

LRESULT CALLBACK splash_window::window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_NCCREATE)
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams));
	splash_window* self = reinterpret_cast<splash_window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

	switch (msg)
	{
		case redraw_message:
			InvalidateRect(hwnd, nullptr, FALSE);
			return 0;
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			if (self)
				self->paint(hdc);
			EndPaint(hwnd, &ps);
			return 0;
		}
		case WM_CLOSE:
			DestroyWindow(hwnd);
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void splash_window::paint(HDC hdc)
{
	//	裏で描いてから一度に写す(ちらつき防止)
	HDC memory_dc = CreateCompatibleDC(hdc);
	HBITMAP back_buffer = CreateCompatibleBitmap(hdc, image_width, image_height);
	HGDIOBJ old_bitmap = SelectObject(memory_dc, back_buffer);

	//	画像
	if (image)
	{
		HDC image_dc = CreateCompatibleDC(hdc);
		HGDIOBJ old_image = SelectObject(image_dc, image);
		BitBlt(memory_dc, 0, 0, image_width, image_height, image_dc, 0, 0, SRCCOPY);
		SelectObject(image_dc, old_image);
		DeleteDC(image_dc);
	}
	else
	{
		RECT all{ 0, 0, image_width, image_height };
		FillRect(memory_dc, &all, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
	}

	//	進捗バー(背景と中身)
	RECT bar{ bar_margin, image_height - bar_bottom - bar_height, image_width - bar_margin, image_height - bar_bottom };
	HBRUSH bar_back = CreateSolidBrush(RGB(48, 48, 48));
	FillRect(memory_dc, &bar, bar_back);
	DeleteObject(bar_back);

	RECT fill = bar;
	fill.right = bar.left + static_cast<LONG>((bar.right - bar.left) * progress.load());
	HBRUSH bar_front = CreateSolidBrush(RGB(255, 255, 255));
	FillRect(memory_dc, &fill, bar_front);
	DeleteObject(bar_front);

	//	状況の文字(バーの上)
	std::wstring current;
	{
		std::lock_guard<std::mutex> lock(mutex);
		current = text;
	}
	SetBkMode(memory_dc, TRANSPARENT);
	SetTextColor(memory_dc, RGB(220, 220, 220));
	HFONT font = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Yu Gothic UI");
	HGDIOBJ old_font = SelectObject(memory_dc, font);
	RECT text_rect{ bar_margin, bar.top - 28, image_width - bar_margin, bar.top - 4 };
	DrawTextW(memory_dc, current.c_str(), -1, &text_rect, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS);
	SelectObject(memory_dc, old_font);
	DeleteObject(font);

	BitBlt(hdc, 0, 0, image_width, image_height, memory_dc, 0, 0, SRCCOPY);

	SelectObject(memory_dc, old_bitmap);
	DeleteObject(back_buffer);
	DeleteDC(memory_dc);
}

HBITMAP splash_window::load_image(const wchar_t* path, int& width, int& height)
{
	using Microsoft::WRL::ComPtr;

	ComPtr<IWICImagingFactory> factory;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
		return nullptr;

	ComPtr<IWICBitmapDecoder> decoder;
	if (FAILED(factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)))
		return nullptr;

	ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(decoder->GetFrame(0, &frame)))
		return nullptr;

	//	GDI で扱える 32bit BGR に変換する
	ComPtr<IWICFormatConverter> converter;
	if (FAILED(factory->CreateFormatConverter(&converter)) ||
		FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGR, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
		return nullptr;

	UINT w = 0;
	UINT h = 0;
	converter->GetSize(&w, &h);

	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = static_cast<LONG>(w);
	info.bmiHeader.biHeight = -static_cast<LONG>(h);	//	上から下へ
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	void* bits = nullptr;
	HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (!bitmap)
		return nullptr;

	if (FAILED(converter->CopyPixels(nullptr, w * 4, w * h * 4, static_cast<BYTE*>(bits))))
	{
		DeleteObject(bitmap);
		return nullptr;
	}

	width = static_cast<int>(w);
	height = static_cast<int>(h);
	return bitmap;
}
