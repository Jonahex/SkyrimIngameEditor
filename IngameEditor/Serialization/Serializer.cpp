#include "Serialization/Serializer.h"

#include "Serialization/SerializationUtils.h"
#include "Serialization/Json.h"
#include "Utils/Engine.h"

#include <RE/B/BGSShaderParticleGeometryData.h>
#include <RE/B/BGSSoundDescriptorForm.h>
#include <RE/B/BGSVolumetricLighting.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESImageSpace.h>
#include <RE/T/TESObjectSTAT.h>
#include <RE/T/TESWeather.h>

#include <nlohmann/json.hpp>

namespace SIE
{
	namespace SSerializer
	{
		void AddToPath(const std::string& path)
		{
			constexpr size_t envBufferSize = 8192;
			char buffer[envBufferSize];
			std::size_t bufferSize = envBufferSize;
			if (!getenv_s(&bufferSize, buffer, envBufferSize, "PATH"))
			{
				std::string envPath = buffer;
				logger::info("Current PATH is {}", envPath);
				const auto pathToAddW = std::filesystem::current_path().append(path).native();
				const auto pathToAdd = std::string(pathToAddW.cbegin(), pathToAddW.cend());
				logger::info("Path to be added to PATH is {}", pathToAdd);
				envPath += ";" + pathToAdd;
				if (!_putenv_s("PATH", envPath.c_str()))
				{
					logger::info("New PATH value is {}", envPath);
				}
				else
				{
					logger::error("Failed to add {} to PATH!", pathToAdd);
				}
			}
			else
			{
				logger::error("Failed to get PATH value!");
			}
		}

		std::unordered_set<std::string> CollectReferences(const RE::TESWeather& weather)
		{
			std::unordered_set<std::string> result;
			for (const auto imageSpace : weather.imageSpaces)
			{
				if (imageSpace != nullptr)
				{
					result.insert(imageSpace->GetFile()->fileName);
				}
			}
			if (weather.precipitationData != nullptr)
			{
				result.insert(weather.precipitationData->GetFile()->fileName);
			}
			for (const auto& item : weather.volumetricLighting)
			{
				if (item != nullptr)
				{
					result.insert(item->GetFile()->fileName);
				}
			}
			for (const auto& item : weather.skyStatics)
			{
				result.insert(item->GetFile()->fileName);
			}
			for (const auto& item : weather.sounds)
			{
				result.insert(RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(item->soundFormID)
								  ->GetFile()
								  ->fileName);
			}
			return result;
		}

		std::unordered_set<std::string> CollectReferences(const RE::TESForm& form)
		{
			switch (form.formType.get())
			{
			case RE::FormType::Weather:
				return CollectReferences(static_cast<const RE::TESWeather&>(form));
			}
		    return {};
		}
	}

    Serializer& Serializer::Instance()
	{
	    static Serializer instance;
		return instance;
	}

	Serializer::Serializer()
	{
        SSerializer::AddToPath("Data/SKSE/plugins/EspGenerator");

		if ((Dll = LoadLibraryA("EspGeneratorWrapper.dll")))
		{
			if (const auto address = GetProcAddress(Dll, "Export"))
			{
				ExportImpl = reinterpret_cast<decltype(ExportImpl)>(address);
				logger::error("Successfully loaded Export method from EspGeneratorWrapper.dll");
			}
			else
			{
				logger::error("Export method wasn't found in EspGeneratorWrapper.dll!");
			}
		}
		else
		{
			logger::error("Failed to load EspGeneratorWrapper.dll!");
		}
	}

    Serializer::~Serializer()
    {
	    FreeLibrary(Dll);
	}

    void Serializer::EnqueueForm(const RE::TESForm& form)
    {
	    forms.insert(&form);
	}

    void Serializer::Export(const std::string& path) const
	{
		nlohmann::json j;

		constexpr uint32_t indexMask = 0xFF000000;

		const auto dataHandler = RE::TESDataHandler::GetSingleton();

		for (auto form : forms)
		{
			const auto master =
				dataHandler->LookupLoadedModByIndex((form->GetFormID() & indexMask) >> 24);

			j.push_back({ { "Master", master->fileName }, { "Override", form->GetFile()->fileName },
				{ "FormKey", ToFormKey(form) }, { "Form", *form }, { "References", SSerializer::CollectReferences(*form) } });
		}

		const auto pathToAddW = std::filesystem::current_path().append(path).native();
		const auto pathToAdd = std::string(pathToAddW.cbegin(), pathToAddW.cend());

		logger::info("Calling EspGenerator for {} to {}", j.dump(4), pathToAdd);
		const auto resultCode = ExportImpl(pathToAdd.c_str(), j.dump().c_str());
		if (resultCode == 0)
		{
			logger::info("Successfully exported");
		}
		else
		{
			logger::info("Export failed!");
		}
	}
}
