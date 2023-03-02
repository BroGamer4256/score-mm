#include "SigScan.h"
#include "helpers.h"
#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <iostream>

SIG_SCAN (sigHitState, 0x14026BB4C, "\xE8\x00\x00\x00\x00\x48\x8B\x4D\xE8\x89\x01", "x????xxxxxx");
SIG_SCAN (sigHitStateInternal, 0x14026D2E0, "\x66\x44\x89\x4C\x24\x00\x53", "xxxxx?x");

extern LRESULT ImGui_ImplWin32_WndProcHandler (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ID3D11DeviceContext *pContext                = NULL;
ID3D11RenderTargetView *mainRenderTargetView = NULL;
WNDPROC oWndProc;

LRESULT __stdcall WndProc (const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler (hWnd, uMsg, wParam, lParam)) return true;
	if (ImGui::GetCurrentContext () != 0 && ImGui::GetIO ().WantCaptureMouse) {
		switch (uMsg) {
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONUP:
		case WM_MOUSEWHEEL: return true;
		}
	}

	return CallWindowProc (oWndProc, hWnd, uMsg, wParam, lParam);
}

typedef enum : i32 {
	Cool        = 0,
	Fine        = 1,
	Safe        = 2,
	Bad         = 3,
	Wrong_Red   = 4,
	Wrong_Grey  = 5,
	Wrong_Green = 6,
	Wrong_Blue  = 7,
	Miss        = 8,
	NA          = 21,
} hitState;

float timings[64];
hitState ratings[64];

float lastTiming = 0.0f;
bool sliding     = false;
i32 timingIndex  = 0;

ImColor safeColour = ImColor (252, 54, 110, 184);
ImColor fineColour = ImColor (0, 251, 55, 184);
ImColor coolColour = ImColor (94, 241, 251, 184);

HOOK (hitState, __fastcall, CheckHitState, (u64)sigHitState () + readUnalignedU32 ((void *)((u64)sigHitState () + 1)) + 5, void *a1, bool *a2, void *a3, void *a4, i32 a5, void *a6, u32 *multiCount,
      u32 *a8, i32 *a9, bool *a10, bool *slide, bool *slide_chain, bool *slide_chain_start, bool *slide_chain_max, bool *slide_chain_continues) {
	if (slide) sliding = *slide;
	else sliding = true;
	hitState result = originalCheckHitState (a1, a2, a3, a4, a5, a6, multiCount, a8, a9, a10, slide, slide_chain, slide_chain_start, slide_chain_max, slide_chain_continues);
	if (*slide_chain_continues || sliding) return result;
	if (result >= Bad) return result;

	timings[timingIndex] = lastTiming;
	ratings[timingIndex] = result;
	timingIndex++;
	if (timingIndex >= COUNTOFARR (timings) - 1) timingIndex = 0;
	return result;
}

HOOK (hitState, __stdcall, CheckHitStateInternal, sigHitStateInternal (), void *a1, void *a2, u16 a3, u16 a4) {
	hitState result = originalCheckHitStateInternal (a1, a2, a3, a4);
	if (result >= Bad || sliding) return result;
	lastTiming = *(float *)((u64)a2 + 0x18) - *(float *)((u64)a1 + 0x13264);
	return result;
}

float
average (float *arr, i32 size) {
	float sum = 0.0f;
	i32 count = 0;
	for (i32 i = 0; i < size; i++) {
		if (arr[i] > 0.1f) continue;
		sum += arr[i];
		count++;
	}
	if (count == 0) return 0.0f;
	return sum / count;
}

float
weirdnessToWindow (float weirdness, float min, float max) {
	float sensible = weirdness * -1.0f;
	sensible += 0.10;
	sensible *= 5;
	float difference = max - min;
	float scaled     = sensible * difference;
	return scaled + min;
}

#ifdef __cplusplus
extern "C" {
#endif
__declspec (dllexport) void init () {
	INSTALL_HOOK (CheckHitState);
	INSTALL_HOOK (CheckHitStateInternal);

	for (int i = 0; i < COUNTOFARR (timings); i++) {
		timings[i] = 1.0f;
		ratings[i] = NA;
	}

	toml_table_t *config = openConfig ((char *)"config.toml");
	if (!config) return;

	int r, g, b, a = 0;
	toml_table_t *safeColourSection = openConfigSection (config, (char *)"safeColour");
	if (safeColourSection) {
		r          = readConfigInt (safeColourSection, (char *)"r", 252);
		g          = readConfigInt (safeColourSection, (char *)"g", 54);
		b          = readConfigInt (safeColourSection, (char *)"b", 110);
		a          = readConfigInt (safeColourSection, (char *)"a", 184);
		safeColour = ImColor (r, g, b, a);
	}

	toml_table_t *fineColourSection = openConfigSection (config, (char *)"fineColour");
	if (fineColourSection) {
		r          = readConfigInt (fineColourSection, (char *)"r", 0);
		g          = readConfigInt (fineColourSection, (char *)"g", 251);
		b          = readConfigInt (fineColourSection, (char *)"b", 55);
		a          = readConfigInt (fineColourSection, (char *)"a", 184);
		fineColour = ImColor (r, g, b, a);
	}

	toml_table_t *coolColourSection = openConfigSection (config, (char *)"coolColour");
	if (coolColourSection) {
		r          = readConfigInt (coolColourSection, (char *)"r", 94);
		g          = readConfigInt (coolColourSection, (char *)"g", 241);
		b          = readConfigInt (coolColourSection, (char *)"b", 251);
		a          = readConfigInt (coolColourSection, (char *)"a", 184);
		coolColour = ImColor (r, g, b, a);
	}
}

__declspec (dllexport) void D3DInit (IDXGISwapChain *swapChain, ID3D11Device *device, ID3D11DeviceContext *deviceContext) {
	DXGI_SWAP_CHAIN_DESC sd;
	ID3D11Texture2D *pBackBuffer;

	pContext = deviceContext;
	swapChain->GetDesc (&sd);
	swapChain->GetBuffer (0, __uuidof (ID3D11Texture2D), (void **)&pBackBuffer);
	device->CreateRenderTargetView (pBackBuffer, NULL, &mainRenderTargetView);
	pBackBuffer->Release ();

	HWND window = sd.OutputWindow;
	oWndProc    = (WNDPROC)SetWindowLongPtrA (window, GWLP_WNDPROC, (LONG_PTR)WndProc);

	ImGui::CreateContext ();
	ImGuiIO &io    = ImGui::GetIO ();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	ImGui_ImplWin32_Init (window);
	ImGui_ImplDX11_Init (device, pContext);
}

__declspec (dllexport) void onResize (IDXGISwapChain *swapChain) {
	ID3D11Texture2D *pBackBuffer;
	ID3D11Device *device;

	swapChain->GetDevice (__uuidof (ID3D11Device), (void **)&device);
	device->GetImmediateContext (&pContext);
	swapChain->GetBuffer (0, __uuidof (ID3D11Texture2D), (void **)&pBackBuffer);
	device->CreateRenderTargetView (pBackBuffer, NULL, &mainRenderTargetView);
	pBackBuffer->Release ();
}

__declspec (dllexport) void onFrame (IDXGISwapChain *chain) {
	ImGui_ImplDX11_NewFrame ();
	ImGui_ImplWin32_NewFrame ();
	ImGui::NewFrame ();

	ImGui::SetNextWindowSize (ImVec2 (700, 70), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos (ImVec2 (0, 0), ImGuiCond_FirstUseEver);
	if (ImGui::Begin ("Judgement Line", 0, 0)) {

		ImDrawList *draw_list = ImGui::GetWindowDrawList ();
		ImVec2 p              = ImGui::GetCursorScreenPos ();
		float startX          = p.x + 4.0f;
		float startY          = p.y + 4.0f;
		ImVec2 max            = ImGui::GetWindowContentRegionMax ();
		float endX            = startX + (max.x - 16.0f);
		float endY            = startY + (max.y - 36.0f);

		float horizontalStartY = startY + (max.y - 36.0f) / 3.0f;
		float horizontalEndY   = horizontalStartY + (max.y - 36.0f) / 2.0f;

		float blueStartX = weirdnessToWindow (0.07f, startX, endX);
		float blueEndX   = weirdnessToWindow (-0.07f, startX, endX);

		float greenStartX = weirdnessToWindow (0.03f, startX, endX);
		float greenEndX   = weirdnessToWindow (-0.03f, startX, endX);

		float middleX     = weirdnessToWindow (0.0f, startX, endX);
		float mean        = average (timings, COUNTOFARR (timings));
		float meanX       = weirdnessToWindow (mean, startX, endX);
		float leftOfMeanX = weirdnessToWindow (mean + 0.0025f, startX, endX);
		float rightOMeanX = weirdnessToWindow (mean - 0.0025f, startX, endX);

		draw_list->AddRectFilled (ImVec2 (startX, horizontalStartY), ImVec2 (endX, horizontalEndY), safeColour);
		draw_list->AddRectFilled (ImVec2 (blueStartX, horizontalStartY), ImVec2 (blueEndX, horizontalEndY), fineColour);
		draw_list->AddRectFilled (ImVec2 (greenStartX, horizontalStartY), ImVec2 (greenEndX, horizontalEndY), coolColour);

		for (int i = 0; i < COUNTOFARR (timings); i++) {
			if (timings[i] > 0.15f) continue;

			float position = weirdnessToWindow (timings[i], startX, endX);
			ImColor colour;
			switch (ratings[i]) {
			case Cool: colour = coolColour; break;
			case Fine: colour = fineColour; break;
			case Safe: colour = safeColour; break;
			default: colour = safeColour;
			}
			draw_list->AddLine (ImVec2 (position, startY), ImVec2 (position, endY), colour);
		}

		draw_list->AddLine (ImVec2 (middleX, startY), ImVec2 (middleX, endY), ImColor (255, 255, 255, 255));
		draw_list->AddTriangleFilled (ImVec2 (leftOfMeanX, startY), ImVec2 (rightOMeanX, startY), ImVec2 (meanX, horizontalStartY), ImColor (255, 255, 255, 255));
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
