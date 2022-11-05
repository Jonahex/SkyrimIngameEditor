#pragma once

#include <msclr\marshal_cppstd.h>

using namespace System;

extern "C" __declspec(dllexport) int Export(const char* outputPath, const char* jsonString);
