#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

//	スプラッシュウィンドウ
class splash_window
{
public:
	splash_window() = default;
	~splash_window();

	splash_window(const splash_window&) = delete;
	splash_window& operator=(const splash_window&) = delete;

	bool show(HINSTANCE instance, const wchar_t* image_path);
	void close();

	//	進捗と状況の文字
	void set_progress(float value);
	void set_text(std::wstring value);

private:
	void thread_main(HINSTANCE instance, std::wstring image_path);
	static LRESULT CALLBACK window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	void paint(HDC hdc);

	//	WIC で画像を読み込んで GDI のビットマップにする
	static HBITMAP load_image(const wchar_t* path, int& width, int& height);

private:
	std::thread thread;
	std::atomic<HWND> hwnd{ nullptr };
	std::atomic<bool> ready{ false };

	HBITMAP image = nullptr;
	int image_width = 0;
	int image_height = 0;

	std::mutex mutex;
	std::wstring text;
	std::atomic<float> progress{ 0.0f };
};
