#include "Systems/CameraSystem.h"

#include "Gui/Gui.h"

#include <RE/B/ButtonEvent.h>
#include <RE/F/FreeCameraState.h>
#include <RE/N/NiNode.h>
#include <RE/P/PlayerCamera.h>
#include <RE/P/PlayerInputHandler.h>

namespace SIE
{
	namespace SCameraSystem
	{
		void IncrementFOV(RE::PlayerCamera& playerCamera, float delta)
		{
			playerCamera.worldFOV = max(1.f, min(179.f, playerCamera.worldFOV + delta));
			playerCamera.firstPersonFOV =
				max(1.f, min(179.f, playerCamera.firstPersonFOV + delta));
		}

		struct FreeCameraStateExtension
		{
			float roll = 0.f;
		} FreeCameraStateExtensionInstance;

		struct FreeCameraState_ProcessButton
		{
			static void thunk(RE::PlayerInputHandler* handler, RE::ButtonEvent* a_event,
				RE::PlayerControlsData* a_data)
			{
				func(handler, a_event, a_data);

				if (a_event->userEvent == "Favorites")
				{
					FreeCameraStateExtensionInstance.roll += 0.05f * a_event->value;
				}
				else if (a_event->userEvent == "Activate")
				{
					FreeCameraStateExtensionInstance.roll -= 0.05f * a_event->value;
				}
				else if (a_event->userEvent == "CameraZUp")
				{
					const auto playerCamera = RE::PlayerCamera::GetSingleton();
					IncrementFOV(*playerCamera, a_event->value);
				}
				else if (a_event->userEvent == "CameraZDown")
				{
					const auto playerCamera = RE::PlayerCamera::GetSingleton();
					IncrementFOV(*playerCamera, -a_event->value);
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr size_t idx = 0x4;
		};

		struct FreeCameraState_GetRotation
		{
			static void thunk(RE::FreeCameraState* state, RE::NiQuaternion& a_rotation)
			{
				static const REL::Relocation<void(RE::NiMatrix3*, float, float, float)>
					NiMatrix3__FromEulerZXY(RELOCATION_ID(68886, 70233));
				static const REL::Relocation<void(RE::NiQuaternion*, RE::NiMatrix3*)>
					NiQuaternion__FromMatrix(RELOCATION_ID(69467, 70844));

				RE::NiMatrix3 tmp;
				NiMatrix3__FromEulerZXY(&tmp, state->rotationInput.y, state->rotationInput.x,
					FreeCameraStateExtensionInstance.roll);
				NiQuaternion__FromMatrix(&a_rotation, &tmp);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr size_t idx = 0x4;
		};

		struct FreeCameraState_CanProcess
		{
			static bool thunk(RE::PlayerInputHandler* handler, RE::InputEvent* a_event)
			{
				if (Gui::Instance().Enabled())
				{
					return false;
				}
				const auto& userEvent = a_event->QUserEvent();
				return userEvent == "Run" || userEvent == "WorldZUp" || userEvent == "WorldZDown" ||
				       userEvent == "CameraZUp" || userEvent == "CameraZDown" ||
				       userEvent == "LockToZPlane" || userEvent == "Zoom In" ||
				       userEvent == "Zoom Out" || userEvent == "Favorites" || userEvent == "Activate";
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr size_t idx = 0x1;
		};

	}

	CameraSystem::CameraSystem() 
	{
		stl::write_vfunc<SCameraSystem::FreeCameraState_GetRotation>(RE::VTABLE_FreeCameraState[0]);
		stl::write_vfunc<SCameraSystem::FreeCameraState_CanProcess>(
			RE::VTABLE_FreeCameraState[1]);
		stl::write_vfunc<SCameraSystem::FreeCameraState_ProcessButton>(
			RE::VTABLE_FreeCameraState[1]);
	}

	void CameraSystem::HandleInput(UINT Msg, WPARAM wParam, LPARAM lParam) 
	{
		/*const auto playerCamera = RE::PlayerCamera::GetSingleton();
		if (playerCamera != nullptr && playerCamera->IsInFreeCameraMode())
		{
			if (Msg == WM_MOUSEWHEEL)
			{
				const auto wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
				SCameraSystem::IncrementFOV(*playerCamera, 0.05f * static_cast<float>(wheelDelta));
			}
			else if (Msg == WM_KEYDOWN && (wParam == 0x45 || wParam == 0x51))
			{
				if (const auto root = playerCamera->cameraRoot.get())
				{
					SCameraSystem::IncrementRoll(*root, (wParam == 0x45) ? 0.1f : -0.1f);
					playerCamera->yaw += (wParam == 0x45) ? 0.1f : -0.1f;
				}
			}
			logger::info("{} {} {}", playerCamera->yaw, playerCamera->unk158, playerCamera->unk15C);
		}*/
	}
}
