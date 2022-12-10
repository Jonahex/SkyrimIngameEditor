#include "Gui/Gui.h"

#include "Gui/MainWindow.h"
#include "Utils/Hooking.h"
#include "Utils/OverheadBuilder.h"
#include "Utils/TargetManager.h"

#include "3rdparty/detours/Detours.h"
#include "3rdparty/dinput/dinput8.h"

#include <RE/C/ControlMap.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <Windows.h>

#include <future>

#define WM_APP_THREAD_TASK (WM_APP + 1)
#define WM_APP_UPDATE_CURSOR (WM_APP + 2)

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
	LPARAM lParam);

namespace SIE
{
	namespace SGui
	{
		bool IsMouseDragging() { return ImGui::IsMouseDragging(ImGuiMouseButton_Left); }
	}

    Gui& Gui::Instance()
    {
        static Gui instance;
		return instance;
	}

    Gui::Gui()
	{
		const auto moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));

		const auto mainLoopAddress =
			REL::Relocation<std::uintptr_t>(RELOCATION_ID(35551, 36544), OFFSET(0x40, 0x82))
				.address();
#ifdef SKYRIM_SUPPORT_AE
		PatchMemory(mainLoopAddress, PBYTE("\xE9\xD2\x00\x00\x00"), 5);
#else
		PatchMemory(mainLoopAddress, PBYTE("\xE9\xD3\x00\x00\x00"), 5);
#endif

		CreateThread(nullptr, 0, &MessageThread, nullptr, 0,
			&MessageThreadId);
		CreateWindowExAFunc = Detours::IATHook(moduleBase, "USER32.DLL", "CreateWindowExA",
			reinterpret_cast<uintptr_t>(&CreateWindowExAThunk));

		D3D11CreateDeviceAndSwapChainFunc = Detours::IATHook(moduleBase, "d3d11.dll",
			"D3D11CreateDeviceAndSwapChain", reinterpret_cast<uintptr_t>(&D3D11CreateDeviceAndSwapChainThunk));

		Detours::IATHook(moduleBase, "dinput8.dll", "DirectInput8Create",
			reinterpret_cast<uintptr_t>(hk_DirectInput8Create));
    }

	HRESULT Gui::D3D11CreateDeviceAndSwapChainThunk(IDXGIAdapter* pAdapter,
		D3D_DRIVER_TYPE DriverType,
		HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels,
		UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain,
		ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel,
		ID3D11DeviceContext** ppImmediateContext)
	{
		auto result = D3D11CreateDeviceAndSwapChainFunc(pAdapter, DriverType, Software, Flags,
			pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice,
			pFeatureLevel, ppImmediateContext);

		*reinterpret_cast<uintptr_t*>(&IDXGISwapChainPresentFunc) = Detours::X64::DetourClassVTable(
			*reinterpret_cast<uintptr_t*>(*ppSwapChain), &IDXGISwapChainPresentThunk, 8);

		Gui::Initialize(pSwapChainDesc->OutputWindow, *ppDevice, *ppImmediateContext);

		return result;
	}

	HRESULT Gui::IDXGISwapChainPresentThunk(IDXGISwapChain* This, UINT SyncInterval, UINT Flags)
	{
		EndFrame();

		const auto currentTime = std::chrono::high_resolution_clock::now();
		const auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
			currentTime - previousProcessTime);
		previousProcessTime = currentTime;

#ifdef OVERHEAD_TOOL
		OverheadBuilder::Instance().Process(delta);
#endif

		Core::GetInstance().Process(delta);

        const auto result = IDXGISwapChainPresentFunc(This, SyncInterval, Flags);

        BeginFrame();

		return result;
	}

	LRESULT CALLBACK Gui::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
        HandleInput(hwnd, uMsg, wParam, lParam);

		// Always-forwarded game wndproc commands
		switch (uMsg)
		{
		case WM_WINDOWPOSCHANGED:
		case WM_NCACTIVATE:
		case WM_DESTROY:
		case WM_SIZE:
		case WM_SYSCOMMAND:
		case WM_MOUSEMOVE:
		case WM_IME_SETCONTEXT:
			return CallWindowProc(OriginalWndProc, hwnd, uMsg, wParam, lParam);
		}

		// Fix for mouse cursor not staying within fullscreen area
		if (uMsg == WM_APP_UPDATE_CURSOR)
		{
			if (Gui::IsInitialized && SGui::IsMouseDragging())
			{
				// Free roam
				ClipCursor(nullptr);
			}
			else
			{
				RECT rcClip;
				GetWindowRect(hwnd, &rcClip);

				// 1 pixel of padding
				rcClip.left += 1;
				rcClip.top += 1;

				rcClip.right -= 1;
				rcClip.bottom -= 1;

				ClipCursor(&rcClip);
			}

			return 0;
		}

		// Handle window getting/losing focus (hide/show cursor)
		if (uMsg == WM_ACTIVATEAPP || uMsg == WM_ACTIVATE || uMsg == WM_SETFOCUS)
		{
			// Gained focus
			if ((uMsg == WM_ACTIVATEAPP && wParam == TRUE) ||
				(uMsg == WM_ACTIVATE && wParam != WA_INACTIVE) || (uMsg == WM_SETFOCUS))
			{
				while (ShowCursor(FALSE) >= 0) {}

				if (GetForegroundWindow() == SkyrimWindow)
					ProxyIDirectInputDevice8A::ToggleGlobalInput(true);
			}

			// Lost focus
			if ((uMsg == WM_ACTIVATEAPP && wParam == FALSE) ||
				(uMsg == WM_ACTIVATE && wParam == WA_INACTIVE))
			{
				while (ShowCursor(TRUE) < 0) {}

				ProxyIDirectInputDevice8A::ToggleGlobalInput(false);
			}

			return 0;
		}

		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	DWORD WINAPI Gui::MessageThread(LPVOID)
	{
		SetThreadName(GetCurrentThreadId(), "Game Message Loop");

		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0) > 0)
		{
			if (msg.message == WM_APP_THREAD_TASK)
			{
				// Check for hk_CreateWindowExA wanting to execute here
				(*reinterpret_cast<std::packaged_task<HWND()>*>(msg.wParam))();
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			// GetForegroundWindow hack since alt+tab or windows key don't always play nice
			if (msg.message == WM_MOUSEMOVE && msg.hwnd == SkyrimWindow &&
				msg.hwnd == GetForegroundWindow())
				WindowProc(msg.hwnd, WM_APP_UPDATE_CURSOR, 0, 0);
		}

		// Message loop exited (WM_QUIT) or there was an error
		return 0;
	}

	HWND Gui::CreateWindowExAThunk(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
		DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu,
		HINSTANCE hInstance, LPVOID lpParam)
	{
		// Create this window on a separate thread
		auto threadTask = std::packaged_task<HWND()>(
			[&]()
			{
                const HWND wnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y,
					nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

				if (wnd && !SkyrimWindow && !strcmp(lpClassName, "Skyrim Special Edition"))
				{
					// The original pointer must be saved BEFORE swapping it out
					SkyrimWindow = wnd;
					OriginalWndProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(wnd, GWLP_WNDPROC));

					SetWindowLongPtr(wnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc));
				}

				return wnd;
			});

		// Wait for completion...
		auto taskVar = threadTask.get_future();
		PostThreadMessage(MessageThreadId, WM_APP_THREAD_TASK, reinterpret_cast<WPARAM>(&threadTask), 0);

		return taskVar.get();
	}

	void Gui::Initialize(HWND window, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
	{
		ImGui::CreateContext();

		ImGui::GetIO().MouseDrawCursor = true;

		ImGui_ImplWin32_Init(window);
		ImGui_ImplDX11_Init(device, deviceContext);

		IsInitialized = true;
	}

	void Gui::HandleInput(HWND Wnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		if (Msg == WM_KEYDOWN && wParam == VK_F7)
		{
			if (WasEnabled)
			{
				if (IsEnabled)
				{
					GetCursorPos(&LastCursorPos);
				}
				else
				{
					SetCursorPos(LastCursorPos.x, LastCursorPos.y);
				}
			}
			IsEnabled = !IsEnabled;
			if (const auto controlMap = RE::ControlMap::GetSingleton())
			{
				controlMap->ignoreKeyboardMouse = IsEnabled;
			}
			WasEnabled = true;
		}
		if (IsEnabled)
		{
			ImGui_ImplWin32_WndProcHandler(Wnd, Msg, wParam, lParam);
			if (Msg == WM_LBUTTONDOWN && !ImGui::GetIO().WantCaptureMouse)
			{
				const auto msgPos = GetMessagePos();
				TargetManager::Instance().TrySetTargetAt(LOWORD(msgPos), HIWORD(msgPos));
			}
		}
		else
		{
			Core::GetInstance().HandleInput(Msg, wParam, lParam);
		}
	}

	void Gui::BeginFrame()
	{
		if (!IsEnabled)
		{
			return;
		}

		IsInFrame = true;

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Draw a fullscreen overlay that renders over the game but not other menus
		ImGui::SetNextWindowPos(ImVec2(-1000.0f, -1000.0f));
		ImGui::SetNextWindowSize(ImVec2(1.0f, 1.0f));
		ImGui::Begin("##InvisiblePreOverlay", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
		ImGui::GetWindowDrawList()->PushClipRectFullScreen();
	}

	void Gui::EndFrame()
	{
		if (!IsInFrame)
		{
			return;
		}

		ImGui::GetWindowDrawList()->PopClipRect();
		ImGui::End();

		MainWindow();

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		IsInFrame = false;
	}
}
