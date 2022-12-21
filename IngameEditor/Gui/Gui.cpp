#include "Gui/Gui.h"

#include "Gui/MainWindow.h"
#include "Utils/Hooking.h"
#include "Utils/OverheadBuilder.h"
#include "Utils/TargetManager.h"

#include "3rdparty/detours/Detours.h"
#include "3rdparty/ImGuizmo/ImGuizmo.h"

#include <RE/B/ButtonEvent.h>
#include <RE/C/CharEvent.h>
#include <RE/C/ControlMap.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <dinput.h>
#include <Windows.h>

#include <future>

#define WM_APP_THREAD_TASK (WM_APP + 1)
#define WM_APP_UPDATE_CURSOR (WM_APP + 2)

namespace SIE
{
	namespace SGui
	{
		bool IsMouseDragging() { return ImGui::IsMouseDragging(ImGuiMouseButton_Left); }

#define IM_VK_KEYPAD_ENTER (VK_RETURN + 256)
		static std::unordered_map<WPARAM, ImGuiKey> keyMap = { { VK_TAB, ImGuiKey_Tab },
			{ VK_LEFT, ImGuiKey_LeftArrow }, { VK_RIGHT, ImGuiKey_RightArrow },
			{ VK_UP, ImGuiKey_UpArrow }, { VK_DOWN, ImGuiKey_DownArrow },
			{ VK_PRIOR, ImGuiKey_PageUp }, { VK_NEXT, ImGuiKey_PageDown },
			{ VK_HOME, ImGuiKey_Home }, { VK_END, ImGuiKey_End }, { VK_INSERT, ImGuiKey_Insert },
			{ VK_DELETE, ImGuiKey_Delete }, { VK_BACK, ImGuiKey_Backspace },
			{ VK_SPACE, ImGuiKey_Space }, { VK_RETURN, ImGuiKey_Enter },
			{ VK_ESCAPE, ImGuiKey_Escape }, { VK_OEM_7, ImGuiKey_Apostrophe },
			{ VK_OEM_COMMA, ImGuiKey_Comma }, { VK_OEM_MINUS, ImGuiKey_Minus },
			{ VK_OEM_PERIOD, ImGuiKey_Period }, { VK_OEM_2, ImGuiKey_Slash },
			{ VK_OEM_1, ImGuiKey_Semicolon }, { VK_OEM_PLUS, ImGuiKey_Equal },
			{ VK_OEM_4, ImGuiKey_LeftBracket }, { VK_OEM_5, ImGuiKey_Backslash },
			{ VK_OEM_6, ImGuiKey_RightBracket }, { VK_OEM_3, ImGuiKey_GraveAccent },
			{ VK_CAPITAL, ImGuiKey_CapsLock }, { VK_SCROLL, ImGuiKey_ScrollLock },
			{ VK_NUMLOCK, ImGuiKey_NumLock }, { VK_SNAPSHOT, ImGuiKey_PrintScreen },
			{ VK_PAUSE, ImGuiKey_Pause }, { VK_NUMPAD0, ImGuiKey_Keypad0 },
			{ VK_NUMPAD1, ImGuiKey_Keypad1 }, { VK_NUMPAD2, ImGuiKey_Keypad2 },
			{ VK_NUMPAD3, ImGuiKey_Keypad3 }, { VK_NUMPAD4, ImGuiKey_Keypad4 },
			{ VK_NUMPAD5, ImGuiKey_Keypad5 }, { VK_NUMPAD6, ImGuiKey_Keypad6 },
			{ VK_NUMPAD7, ImGuiKey_Keypad7 }, { VK_NUMPAD8, ImGuiKey_Keypad8 },
			{ VK_NUMPAD9, ImGuiKey_Keypad9 }, { VK_DECIMAL, ImGuiKey_KeypadDecimal },
			{ VK_DIVIDE, ImGuiKey_KeypadDivide }, { VK_MULTIPLY, ImGuiKey_KeypadMultiply },
			{ VK_SUBTRACT, ImGuiKey_KeypadSubtract }, { VK_ADD, ImGuiKey_KeypadAdd },
			{ IM_VK_KEYPAD_ENTER, ImGuiKey_KeypadEnter }, { VK_LSHIFT, ImGuiKey_LeftShift },
			{ VK_LCONTROL, ImGuiKey_LeftCtrl }, { VK_LMENU, ImGuiKey_LeftAlt },
			{ VK_LWIN, ImGuiKey_LeftSuper }, { VK_RSHIFT, ImGuiKey_RightShift },
			{ VK_RCONTROL, ImGuiKey_RightCtrl }, { VK_RMENU, ImGuiKey_RightAlt },
			{ VK_RWIN, ImGuiKey_RightSuper }, { VK_APPS, ImGuiKey_Menu }, { '0', ImGuiKey_0 },
			{ '1', ImGuiKey_1 }, { '2', ImGuiKey_2 }, { '3', ImGuiKey_3 }, { '4', ImGuiKey_4 },
			{ '5', ImGuiKey_5 }, { '6', ImGuiKey_6 }, { '7', ImGuiKey_7 }, { '8', ImGuiKey_8 },
			{ '9', ImGuiKey_9 }, { 'A', ImGuiKey_A }, { 'B', ImGuiKey_B }, { 'C', ImGuiKey_C },
			{ 'D', ImGuiKey_D }, { 'E', ImGuiKey_E }, { 'F', ImGuiKey_F }, { 'G', ImGuiKey_G },
			{ 'H', ImGuiKey_H }, { 'I', ImGuiKey_I }, { 'J', ImGuiKey_J }, { 'K', ImGuiKey_K },
			{ 'L', ImGuiKey_L }, { 'M', ImGuiKey_M }, { 'N', ImGuiKey_N }, { 'O', ImGuiKey_O },
			{ 'P', ImGuiKey_P }, { 'Q', ImGuiKey_Q }, { 'R', ImGuiKey_R }, { 'S', ImGuiKey_S },
			{ 'T', ImGuiKey_T }, { 'U', ImGuiKey_U }, { 'V', ImGuiKey_V }, { 'W', ImGuiKey_W },
			{ 'X', ImGuiKey_X }, { 'Y', ImGuiKey_Y }, { 'Z', ImGuiKey_Z }, { VK_F1, ImGuiKey_F1 },
			{ VK_F2, ImGuiKey_F2 }, { VK_F3, ImGuiKey_F3 }, { VK_F4, ImGuiKey_F4 },
			{ VK_F5, ImGuiKey_F5 }, { VK_F6, ImGuiKey_F6 }, { VK_F7, ImGuiKey_F7 },
			{ VK_F8, ImGuiKey_F8 }, { VK_F9, ImGuiKey_F9 }, { VK_F10, ImGuiKey_F10 },
			{ VK_F11, ImGuiKey_F11 }, { VK_F12, ImGuiKey_F12 } };

		static ImGuiKey VirtualKeyToImGuiKey(WPARAM wParam)
		{
			auto it = keyMap.find(wParam);
			if (it == keyMap.end())
			{
				return ImGuiKey_None;
			}
			return it->second;
		}
	}

    Gui& Gui::Instance()
    {
        static Gui instance;
		return instance;
	}

    Gui::Gui()
	{
		const auto moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));

		D3D11CreateDeviceAndSwapChainFunc = Detours::IATHook(moduleBase, "d3d11.dll",
			"D3D11CreateDeviceAndSwapChain", reinterpret_cast<uintptr_t>(&D3D11CreateDeviceAndSwapChainThunk));
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

		SkyrimWindow = pSwapChainDesc->OutputWindow;

		return result;
	}

	HRESULT Gui::IDXGISwapChainPresentThunk(IDXGISwapChain* This, UINT SyncInterval, UINT Flags)
	{
		EndFrame();

		if (IsEnabled && GetForegroundWindow() == SkyrimWindow)
		{
			RECT rcClip;
			GetWindowRect(SkyrimWindow, &rcClip);

			// 1 pixel of padding
			rcClip.left += 1;
			rcClip.top += 1;

			rcClip.right -= 1;
			rcClip.bottom -= 1;

			ClipCursor(&rcClip);
		}

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

	void Gui::Initialize(HWND window, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
	{
		ImGui::CreateContext();

		ImGui::GetIO().MouseDrawCursor = true;

		ImGui_ImplWin32_Init(window);
		ImGui_ImplDX11_Init(device, deviceContext);

		IsInitialized = true;
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
		ImGuizmo::BeginFrame();
	}

	void Gui::EndFrame()
	{
		if (!IsInFrame)
		{
			return;
		}

		MainWindow();

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		IsInFrame = false;
	}

	RE::BSEventNotifyControl Gui::ProcessEvent(RE::InputEvent* const* a_event,
		RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
	{
		if (!a_event || !a_eventSource)
			return RE::BSEventNotifyControl::kContinue;

		auto& io = ImGui::GetIO();

		for (auto event = *a_event; event; event = event->next)
		{
			if (event->eventType == RE::INPUT_EVENT_TYPE::kChar)
			{
				if (IsEnabled)
				{
					io.AddInputCharacter(static_cast<RE::CharEvent*>(event)->keyCode);
				}
			}
			else if (event->eventType == RE::INPUT_EVENT_TYPE::kButton)
			{
				const auto button = static_cast<RE::ButtonEvent*>(event);
				if (!button || (button->IsPressed() && !button->IsDown()))
					continue;

				auto scan_code = button->GetIDCode();
				uint32_t key = MapVirtualKeyEx(scan_code, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0));
				switch (scan_code)
				{
				case DIK_LEFTARROW:
					key = VK_LEFT;
					break;
				case DIK_RIGHTARROW:
					key = VK_RIGHT;
					break;
				case DIK_UPARROW:
					key = VK_UP;
					break;
				case DIK_DOWNARROW:
					key = VK_DOWN;
					break;
				case DIK_DELETE:
					key = VK_DELETE;
					break;
				case DIK_END:
					key = VK_END;
					break;
				case DIK_HOME:
					key = VK_HOME;
					break;  // pos1
				case DIK_PRIOR:
					key = VK_PRIOR;
					break;  // page up
				case DIK_NEXT:
					key = VK_NEXT;
					break;  // page down
				case DIK_INSERT:
					key = VK_INSERT;
					break;
				case DIK_NUMPAD0:
					key = VK_NUMPAD0;
					break;
				case DIK_NUMPAD1:
					key = VK_NUMPAD1;
					break;
				case DIK_NUMPAD2:
					key = VK_NUMPAD2;
					break;
				case DIK_NUMPAD3:
					key = VK_NUMPAD3;
					break;
				case DIK_NUMPAD4:
					key = VK_NUMPAD4;
					break;
				case DIK_NUMPAD5:
					key = VK_NUMPAD5;
					break;
				case DIK_NUMPAD6:
					key = VK_NUMPAD6;
					break;
				case DIK_NUMPAD7:
					key = VK_NUMPAD7;
					break;
				case DIK_NUMPAD8:
					key = VK_NUMPAD8;
					break;
				case DIK_NUMPAD9:
					key = VK_NUMPAD9;
					break;
				case DIK_DECIMAL:
					key = VK_DECIMAL;
					break;
				case DIK_NUMPADENTER:
					key = IM_VK_KEYPAD_ENTER;
					break;
				case DIK_RMENU:
					key = VK_RMENU;
					break;  // right alt
				case DIK_RCONTROL:
					key = VK_RCONTROL;
					break;  // right control
				case DIK_LWIN:
					key = VK_LWIN;
					break;  // left win
				case DIK_RWIN:
					key = VK_RWIN;
					break;  // right win
				case DIK_APPS:
					key = VK_APPS;
					break;
				default:
					break;
				}

				switch (button->device.get())
				{
				case RE::INPUT_DEVICE::kMouse:
					if (IsEnabled)
					{
						if (scan_code > 7)  // middle scroll
							io.AddMouseWheelEvent(0, button->Value() * (scan_code == 8 ? 1 : -1));
						else
						{
							if (scan_code > 5)
								scan_code = 5;
							io.AddMouseButtonEvent(scan_code, button->IsPressed());

							if (IsEnabled && scan_code == 0 && !ImGui::GetIO().WantCaptureMouse &&
								!button->IsPressed())
							{
								tagPOINT point;
								if (GetCursorPos(&point) && ScreenToClient(SkyrimWindow, &point))
								{
									TargetManager::Instance().TrySetTargetAt(point.x, point.y);
								}
							}
						}
					}
					break;
				case RE::INPUT_DEVICE::kKeyboard:
					if (key == VK_F7 && !button->IsPressed())
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
						//io.SetAppAcceptingEvents(IsEnabled);
						WasEnabled = true;
					}
					else if (IsEnabled)
					{
						io.AddKeyEvent(SGui::VirtualKeyToImGuiKey(key), button->IsPressed());
					}
					break;
				case RE::INPUT_DEVICE::kGamepad:
					// not implemented yet
					// key = GetGamepadIndex((RE::BSWin32GamepadDevice::Key)key);
					break;
				default:
					continue;
				}
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}
}
