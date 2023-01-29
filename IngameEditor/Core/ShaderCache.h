#pragma once

#include <RE/B/BSShader.h>

#include <condition_variable>

namespace SIE
{
	enum class ShaderClass
	{
		Vertex,
		Pixel,
		Compute,
		Total,
	};

	class ShaderCompilationTask
	{
	public:
		ShaderCompilationTask(ShaderClass shaderClass, const RE::BSShader& shader, uint32_t descriptor);
		void Perform();

	protected:
		ShaderClass shaderClass;
		const RE::BSShader& shader;
		uint32_t descriptor;
	};

	class CompilationQueue
	{
	public:
		std::optional<ShaderCompilationTask> Pop();
		void Push(const ShaderCompilationTask& task);
		void Clear();

	private:
		std::queue<ShaderCompilationTask> queue;
		std::condition_variable conditionVariable;
		std::mutex mutex;
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
		bool IsAsync() const;
		void SetAsync(bool value);

		void Clear();

		RE::BSGraphics::VertexShader* GetVertexShader(const RE::BSShader& shader, uint32_t descriptor);
		RE::BSGraphics::PixelShader* GetPixelShader(const RE::BSShader& shader,
			uint32_t descriptor);

		RE::BSGraphics::VertexShader* MakeAndAddVertexShader(const RE::BSShader& shader,
			uint32_t descriptor);
		RE::BSGraphics::PixelShader* MakeAndAddPixelShader(const RE::BSShader& shader,
			uint32_t descriptor);

	private:
		ShaderCache();
		void ProcessCompilationQueue();

		~ShaderCache();

		std::array<std::unordered_map<uint32_t, std::unique_ptr<RE::BSGraphics::VertexShader>>,
			static_cast<size_t>(RE::BSShader::Type::Total)>
			vertexShaders;
		std::array<std::unordered_map<uint32_t, std::unique_ptr<RE::BSGraphics::PixelShader>>,
			static_cast<size_t>(RE::BSShader::Type::Total)>
			pixelShaders;

		bool isEnabled = true;
		uint32_t disabledClasses = 0;

		bool isAsync = true;
		CompilationQueue compilationQueue; 
		std::jthread compilationThread;
	};
}
