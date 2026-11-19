#include "Framework/application_desc.h"

int WINAPI WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance, _In_ LPSTR cmd_line, _In_ int cmd_show)
{
	application_desc desc;
	return run_application(instance, desc);
}
