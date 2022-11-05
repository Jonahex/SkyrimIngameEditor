#include "pch.h"

#include "EspGeneratorWrapper.h"

extern "C" __declspec(dllexport) int Export(const char* outputPath, const char* jsonString)
{
	return EspGenerator::EspGenerator::Export(gcnew String(outputPath), gcnew String(jsonString));
}
