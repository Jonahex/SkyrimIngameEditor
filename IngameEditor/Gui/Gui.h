#pragma once

#include <RE/N/NiSmartPointer.h>

#include <d3d11.h>

namespace RE
{
	class TESObjectREFR;
}

namespace SIE
{
    class Gui
    {
	public:
		static Gui& Instance();

    private:
		Gui();

		static HRESULT D3D11CreateDeviceAndSwapChainThunk(IDXGIAdapter* pAdapter,
			D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
			const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
			const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain,
			ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel,
			ID3D11DeviceContext** ppImmediateContext);

		static HRESULT IDXGISwapChainPresentThunk(IDXGISwapChain* This, UINT SyncInterval,
			UINT Flags);

		static HWND CreateWindowExAThunk(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
			DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu,
			HINSTANCE hInstance, LPVOID lpParam);

		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		static DWORD WINAPI MessageThread(LPVOID);

		static void Initialize(HWND window, ID3D11Device* device,
			ID3D11DeviceContext* deviceContext);
		static void HandleInput(HWND Wnd, UINT Msg, WPARAM wParam, LPARAM lParam);
		static void BeginFrame();
		static void EndFrame();

		static inline REL::Relocation<decltype(D3D11CreateDeviceAndSwapChainThunk)>
			D3D11CreateDeviceAndSwapChainFunc;
		static inline REL::Relocation<decltype(IDXGISwapChainPresentThunk)>
			IDXGISwapChainPresentFunc;
		static inline REL::Relocation<decltype(CreateWindowExAThunk)> CreateWindowExAFunc;

		static inline HWND SkyrimWindow = nullptr;
		static inline WNDPROC OriginalWndProc = nullptr;
		static inline DWORD MessageThreadId = 0;

		static inline bool IsInitialized = false;
		static inline bool IsEnabled = false;
		static inline bool IsInFrame = false;
		static inline bool WasEnabled = false;

		static inline POINT LastCursorPos;
    };
}
