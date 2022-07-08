#include "SigScan.h"
#include "helpers.h"
#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <iostream>

SIG_SCAN (sigHitState, 0x14026D3D0, "\x66\x44\x89\x4C\x24\x00\x53", "xxxxx?x");

extern LRESULT ImGui_ImplWin32_WndProcHandler (HWND hWnd, UINT msg,
											   WPARAM wParam, LPARAM lParam);

ID3D11DeviceContext *pContext = NULL;
ID3D11RenderTargetView *mainRenderTargetView = NULL;
WNDPROC oWndProc;

LRESULT __stdcall WndProc (const HWND hWnd, UINT uMsg, WPARAM wParam,
						   LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler (hWnd, uMsg, wParam, lParam))
		return true;
	if (ImGui::GetCurrentContext () != 0 && ImGui::GetIO ().WantCaptureMouse) {
		switch (uMsg) {
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONUP:
		case WM_MOUSEWHEEL:
			return true;
		}
	}

	return CallWindowProc (oWndProc, hWnd, uMsg, wParam, lParam);
}

typedef enum : i32 {
	Cool = 0,
	Fine = 1,
	Safe = 2,
	Bad = 3,
	Wrong_Red = 4,
	Wrong_Grey = 5,
	Wrong_Green = 6,
	Wrong_Blue = 7,
	Miss = 8,
	NA = 21,
} hitState;

float timings[40];
hitState ratings[40];

i32 cools = 0;
i32 fines = 0;
i32 safes = 0;
i32 bads = 0;
i32 wrongs = 0;
i32 misses = 0;
i32 timingIndex = 0;

ImColor safeColour = ImColor (252, 54, 110, 184);
ImColor fineColour = ImColor (0, 251, 55, 184);
ImColor coolColour = ImColor (94, 241, 251, 184);

HOOK (hitState, __stdcall, CheckHitState, sigHitState (), void *a1, void *a2,
	  u16 a3, u16 a4) {
	hitState result = originalCheckHitState (a1, a2, a3, a4);
	switch (result) {
	case Cool:
		cools++;
		break;
	case Fine:
		fines++;
		break;
	case Safe:
		safes++;
		break;
	case Bad:
		bads++;
		break;
	case Wrong_Red:
	case Wrong_Grey:
	case Wrong_Green:
	case Wrong_Blue:
		wrongs++;
		break;
	case Miss:
		misses++;
		break;
	case NA:
		break;
	}
	if (result >= Bad)
		return result;

	timings[timingIndex]
		= *(float *)((u64)a2 + 0x18) - *(float *)((u64)a1 + 0x13264);
	ratings[timingIndex] = result;
	timingIndex++;
	if (timingIndex >= COUNTOFARR (timings) - 1)
		timingIndex = 0;
	return result;
}

// Timing can be between 0.10 for the lowest and -0.10 for the highest
// Returns 0 to 1 scale
float
weirdnessToSensible (float weird) {
	float sensible = weird * -1.0f;
	sensible += 0.10;
	sensible *= 5;
	return sensible;
}

float
sensibleToWindow (float sensible, float min, float max) {
	float difference = max - min;
	float scaled = sensible * difference;
	return scaled + min;
}

#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllexport) void init () {
	INSTALL_HOOK (CheckHitState);
	for (int i = 0; i < COUNTOFARR (timings); i++) {
		timings[i] = 1.0f;
		ratings[i] = NA;
	}
	toml_table_t *config = openConfig("config.toml");
	if (!config) return;
	toml_table_t *safeColourSection = openConfigSection(config, "safeColour");
	int r,g,b,a = 0;
	if (safeColourSection) {
		r = readConfigInt(safeColourSection, "r", 252);
		g = readConfigInt(safeColourSection, "g", 54);
		b = readConfigInt(safeColourSection, "b", 110);
		a = readConfigInt(safeColourSection, "a", 184);
		safeColour = ImColor(r,g,b,a);
	}
	toml_table_t *fineColourSection = openConfigSection(config, "fineColour");
	if (fineColourSection) {
		r = readConfigInt(fineColourSection, "r", 0);
		g = readConfigInt(fineColourSection, "g", 251);
		b = readConfigInt(fineColourSection, "b", 55);
		a = readConfigInt(fineColourSection, "a", 184);
		fineColour = ImColor(r,g,b,a);
	}
	toml_table_t *coolColourSection = openConfigSection(config, "coolColour");
	if (coolColourSection) {
		r = readConfigInt(coolColourSection, "r", 94);
		g = readConfigInt(coolColourSection, "g", 241);
		b = readConfigInt(coolColourSection, "b", 251);
		a = readConfigInt(coolColourSection, "a", 184);
		coolColour = ImColor(r,g,b,a);
	}
}

__declspec(dllexport) void onFrame (IDXGISwapChain *chain) {
	static bool inited = false;
	static bool locked = false;
	static u64 original = 0;
	if (!inited) {
		ID3D11Device *pDevice;
		DXGI_SWAP_CHAIN_DESC sd;
		chain->GetDevice (__uuidof(ID3D11Device), (void **)&pDevice);
		chain->GetDesc (&sd);
		pDevice->GetImmediateContext (&pContext);
		ID3D11Texture2D *pBackBuffer;
		chain->GetBuffer (0, __uuidof(ID3D11Texture2D),
						  (LPVOID *)&pBackBuffer);
		pDevice->CreateRenderTargetView (pBackBuffer, NULL,
										 &mainRenderTargetView);
		pBackBuffer->Release ();
		HWND window = sd.OutputWindow;
		oWndProc = (WNDPROC)SetWindowLongPtrA (window, GWLP_WNDPROC,
											   (LONG_PTR)WndProc);
		ImGui::CreateContext ();
		ImGuiIO &io = ImGui::GetIO ();
		io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
		ImGui_ImplWin32_Init (window);
		ImGui_ImplDX11_Init (pDevice, pContext);

		inited = true;
	}

	if (ImGui::GetIO ().WantCaptureKeyboard && !locked) {
		original = *(u64 *)PROC_ADDRESS ("user32.dll", "GetAsyncKeyState");
		WRITE_MEMORY (PROC_ADDRESS ("user32.dll", "GetAsyncKeyState"), u8,
					  0x66, 0x31, 0xC0, 0xC3); // xor ax, ax; ret
		locked = true;
	} else if (!ImGui::GetIO ().WantCaptureKeyboard && locked) {
		WRITE_MEMORY (PROC_ADDRESS ("user32.dll", "GetAsyncKeyState"), u64,
					  original);
		locked = false;
	}

	ImGui_ImplDX11_NewFrame ();
	ImGui_ImplWin32_NewFrame ();
	ImGui::NewFrame ();

	ImGui::Begin ("Judgement Line", 0, 0);

	ImDrawList *draw_list = ImGui::GetWindowDrawList ();
	ImVec2 p = ImGui::GetCursorScreenPos ();
	float startX = p.x + 4.0f;
	float startY = p.y + 4.0f;
	ImVec2 max = ImGui::GetWindowContentRegionMax ();
	float endX = startX + (max.x - 16.0f);
	float endY = startY + (max.y - 36.0f);

	float horizontalStartY = startY + (max.y - 36.0f) / 3.0f;
	float horizontalEndY = horizontalStartY + (max.y - 36.0f) / 2.0f;

	float blueStartX
		= sensibleToWindow (weirdnessToSensible (0.07f), startX, endX);
	float blueEndX
		= sensibleToWindow (weirdnessToSensible (-0.07f), startX, endX);

	float greenStartX
		= sensibleToWindow (weirdnessToSensible (0.03f), startX, endX);
	float greenEndX
		= sensibleToWindow (weirdnessToSensible (-0.03f), startX, endX);

	draw_list->AddRectFilled (ImVec2 (startX, horizontalStartY),
							  ImVec2 (endX, horizontalEndY),
							  safeColour);
	draw_list->AddRectFilled (ImVec2 (blueStartX, horizontalStartY),
							  ImVec2 (blueEndX, horizontalEndY),
							  fineColour);
	draw_list->AddRectFilled (ImVec2 (greenStartX, horizontalStartY),
							  ImVec2 (greenEndX, horizontalEndY),
							  coolColour);

	for (int i = 0; i < COUNTOFARR (timings); i++) {
		if (timings[i] > 0.15f)
			continue;

		float position = sensibleToWindow (weirdnessToSensible (timings[i]),
										   startX, endX);
		ImColor colour;
		switch (ratings[i]) {
		case Cool:
			colour = coolColour;
			break;
		case Fine:
			colour = fineColour;
			break;
		case Safe:
			colour = safeColour;
			break;
		default:
			colour = safeColour;
		}
		draw_list->AddLine (ImVec2 (position, startY), ImVec2 (position, endY),
							colour);
	}

	ImGui::End ();

	ImGui::Begin ("Scores", 0, 0);
	ImGui::Text ("Cool: %d", cools);
	ImGui::Text ("Fine: %d", fines);
	ImGui::Text ("Safe: %d", safes);
	ImGui::Text ("Bad: %d", bads);
	ImGui::Text ("Wrong: %d", wrongs);
	ImGui::Text ("Miss: %d", misses);
	if (ImGui::Button ("Reset")) {
		cools = 0;
		fines = 0;
		safes = 0;
		bads = 0;
		wrongs = 0;
		misses = 0;
		for (int i = 0; i < COUNTOFARR (timings); i++) {
			timings[i] = 1.0f;
			ratings[i] = NA;
		}
	}
	ImGui::End ();

	ImGui::EndFrame ();
	ImGui::Render ();
	pContext->OMSetRenderTargets (1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData (ImGui::GetDrawData ());
}
#ifdef __cplusplus
}
#endif
