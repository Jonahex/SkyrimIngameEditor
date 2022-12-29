#include "Utils/OverheadBuilder.h"

#include "Utils/Engine.h"

#include <RE/C/ControlMap.h>
#include <RE/E/ExtraOcclusionShape.h>
#include <RE/M/Main.h>
#include <RE/N/NiCamera.h>
#include <RE/N/NiRTTI.h>
#include <RE/P/PlayerCamera.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/Sky.h>
#include <RE/T/TES.h>
#include <RE/T/TESImageSpace.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESWeather.h>
#include <RE/T/TESWorldspace.h>
#include <RE/U/UI.h>

#include <magic_enum.hpp>

namespace SIE
{
	namespace SOverheadBuilder
	{
		void SetupWeather() 
		{ 
			const auto sky = RE::Sky::GetSingleton();
			if (const auto weather = static_cast<RE::TESWeather*>(RE::TESForm::LookupByEditorID("SkyrimClear")))
			{
				weather->fogData.dayNear = 1000000.f;
				weather->fogData.dayFar = 1000000.f;
				for (size_t time = 0; time < RE::TESWeather::ColorTimes::kTotal; ++time)
				{
					weather->imageSpaces[time]->data.hdr.eyeAdaptSpeed = 100.f;
					//weather->imageSpaces[time]->data.hdr.eyeAdaptStrength = 0.f;
				}
				sky->ForceWeather(weather, false);
			}
		}

		RE::NiCamera* FindNiCamera(RE::NiAVObject* object) 
		{ 
			if (object != nullptr)
			{
				if (std::strcmp(object->GetRTTI()->name, "NiCamera") == 0)
				{
					return static_cast<RE::NiCamera*>(object);
				}
				if (const auto node = object->AsNode())
				{
					for (const auto& child : node->children)
					{
						if (const auto found = FindNiCamera(child.get()))
						{
							return found;
						}
					}
				}
			}
			return nullptr;
		}

		RE::TESObjectCELL* LoadTileCells(RE::TESWorldSpace& worldSpace, int16_t centerX, int16_t centerY, int16_t halfGrid) 
		{
			static const REL::Relocation<RE::TESObjectCELL*(RE::TESWorldSpace*, int16_t, int16_t)>
				LoadCell(RELOCATION_ID(20026, 20460));

			RE::TESObjectCELL* result = nullptr;
			for (int16_t x = centerX - halfGrid; x <= centerX + halfGrid; ++x)
			{
				for (int16_t y = centerY - halfGrid; y <= centerY + halfGrid; ++y)
				{
					RE::TESObjectCELL* cell = nullptr;
					auto it = worldSpace.cellMap.find(RE::CellID(x, y));
					if (it == worldSpace.cellMap.cend())
					{
						cell = LoadCell(&worldSpace, x, y);
					}
					else
					{
						cell = it->second;
					}
					if (x == centerX && y == centerY)
					{
						result = cell;
					}
				}
			}

			return result;
		}

		void DisableObstacles() 
		{ 
			const auto tes = RE::TES::GetSingleton();
			tes->ForEachReference(
				[](RE::TESObjectREFR& refr)
				{
					if (refr.GetBaseObject()->GetFormType() == RE::FormType::NPC ||
						refr.extraList.HasType<RE::ExtraOcclusionShape>())
					{
						refr.Disable();
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
		}
	}

	OverheadBuilder& OverheadBuilder::Instance()
	{
		static OverheadBuilder instance;
		return instance;
	}

	void OverheadBuilder::Start()
	{ 
		isRunning = true;
		firstFrame = true;

		const auto worldSpace = RE::TES::GetSingleton()->worldSpace;
		currentX = minX + halfGrid;
		currentY = minY + halfGrid;

		left = -2048 * (2 * halfGrid + 1);
		right = 2048 * (2 * halfGrid + 1);
		top = 2048 * (2 * halfGrid + 1);
		bottom = -2048 * (2 * halfGrid + 1);
		ortho = true;
	}

	void OverheadBuilder::Process(std::chrono::microseconds deltaTime)
	{
		auto playerCamera = RE::PlayerCamera::GetSingleton();
		if (playerCamera != nullptr)
		{
			if (const auto root = playerCamera->cameraRoot.get();
				root != nullptr && root->children.size() > 0 &&
				std::strcmp(root->children[0]->GetRTTI()->name, "NiCamera") == 0)
			{
				auto niCamera = static_cast<RE::NiCamera*>(root->children[0].get());
				if (isFirst)
				{
					farPlane = niCamera->viewFrustum.fFar;
					nearPlane = niCamera->viewFrustum.fNear;
					left = niCamera->viewFrustum.fLeft;
					right = niCamera->viewFrustum.fRight;
					top = niCamera->viewFrustum.fTop;
					bottom = niCamera->viewFrustum.fBottom;
					ortho = niCamera->viewFrustum.bOrtho;
					isFirst = false;
				}
				else
				{
					niCamera->viewFrustum.fFar = farPlane;
					niCamera->viewFrustum.fNear = nearPlane;
					niCamera->viewFrustum.fLeft = left;
					niCamera->viewFrustum.fRight = right;
					niCamera->viewFrustum.fTop = top;
					niCamera->viewFrustum.fBottom = bottom;
					niCamera->viewFrustum.bOrtho = ortho;
				}
			}
		}

		if (!isRunning)
		{
			return;
		}

		if (!firstFrame && (std::chrono::high_resolution_clock::now() - startWait) < timeToSkip)
		{
			return;
		}

		static const REL::Relocation<void*> Renderer(RELOCATION_ID(524907, 411393));
		static const REL::Relocation<void(void*, int, const char*, uint32_t)> TakeScreenshot(
			RELOCATION_ID(75522, 77316));
		static const REL::Relocation<RE::TESObjectCELL*(RE::TESWorldSpace*, int16_t, int16_t)>
			LoadCell(RELOCATION_ID(20026, 20460));
		static const REL::Relocation<bool*> IsPlayerCollisionDisabled(RELOCATION_ID(514184, 400334));
		static const REL::Relocation<void()> TogglePlayerCollision(RELOCATION_ID(13224, 13375));

		logger::info("{} {}", currentX, currentY);

		const auto worldSpace = RE::TES::GetSingleton()->worldSpace;
		RE::TESObjectCELL* cell = nullptr;
		auto it = worldSpace->cellMap.find(RE::CellID(currentY, currentX));
		if (it == worldSpace->cellMap.cend())
		{
			cell = LoadCell(worldSpace, currentX, currentY);
		}
		else
		{
			cell = it->second;
		}
		if (cell != nullptr)
		{
			if (!firstFrame)
			{
				TakeScreenshot(Renderer.get(), 0, nextName.c_str(), 3);
			}
			else
			{
				firstFrame = false;
			}

			if (needFinish)
			{
				needFinish = false;
				Finish();
			}

			nextName = std::format("overhead_{}_{}.png", currentX, currentY);

			auto ui = RE::UI::GetSingleton();
			auto player = RE::PlayerCharacter::GetSingleton();
			auto controlMap = RE::ControlMap::GetSingleton();
			auto main = RE::Main::GetSingleton();

			controlMap->ignoreKeyboardMouse = true;
			ui->ShowMenus(false);
			playerCamera->ForceThirdPerson();
			SetVisibility(RE::VISIBILITY::kActor, false);
			playerCamera->allowAutoVanityMode = false;
			if (!*IsPlayerCollisionDisabled)
			{
				TogglePlayerCollision();
			}
			player->CenterOnCell(cell);
			SOverheadBuilder::DisableObstacles(); 
			startWait = std::chrono::high_resolution_clock::now();
			const auto exteriorData = cell->GetCoordinates();
			player->SetPosition(
				{ exteriorData->worldX + 2048.f, exteriorData->worldY + 2048.f, 50000.f }, false);
			player->SetRotationX(90);
			player->SetRotationZ(0);
			ResetTimeTo(12.f);
			SOverheadBuilder::SetupWeather();
		}

		currentY += 2 * halfGrid + 1;
		if (currentY > maxY)
		{
			currentY = minY + halfGrid;
			currentX += 2 * halfGrid + 1;
			if (currentX > maxX)
			{
				needFinish = true;
			}
		}
	}

	bool OverheadBuilder::IsRunning() const 
	{ 
		return isRunning;
	}

	void OverheadBuilder::Finish()
	{
		isRunning = false;
	}
}
