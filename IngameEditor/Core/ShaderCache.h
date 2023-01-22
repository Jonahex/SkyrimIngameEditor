#pragma once

#include <RE/B/BSShader.h>

namespace SIE
{
	enum class ShaderClass
	{
		Vertex,
		Pixel,
		Compute,
		Total,
	};

	class ShaderCache
	{
	public:
		static ShaderCache& Instance()
		{
			static ShaderCache instance;
			return instance;
		}

		bool IsEnabled() const;
		void SetEnabled(bool value);
		bool IsEnabledForClass(ShaderClass shaderClass) const;
		void SetEnabledForClass(ShaderClass shaderClass, bool value);

		void Clear();

		RE::BSGraphics::VertexShader* GetVertexShader(RE::BSShader::Type type, uint32_t descriptor);
		RE::BSGraphics::PixelShader* GetPixelShader(RE::BSShader::Type type, uint32_t descriptor);

	private:
		~ShaderCache();

		std::array<std::unordered_map<uint32_t, std::unique_ptr<RE::BSGraphics::VertexShader>>,
			static_cast<size_t>(RE::BSShader::Type::Total)>
			vertexShaders;
		std::array<std::unordered_map<uint32_t, std::unique_ptr<RE::BSGraphics::PixelShader>>,
			static_cast<size_t>(RE::BSShader::Type::Total)>
			pixelShaders;

		bool isEnabled = true;
		uint32_t disabledClasses = 0;
	};
}
