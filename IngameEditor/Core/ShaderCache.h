#pragma once

#include <RE/B/BSShader.h>

#include <condition_variable>
#include <unordered_set>

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
		ShaderCompilationTask(ShaderClass shaderClass, const RE::BSShader& shader,
			uint32_t descriptor);
		void Perform() const;

		size_t GetId() const;

		bool operator==(const ShaderCompilationTask& other) const;

	protected:
		ShaderClass shaderClass;
		const RE::BSShader& shader;
		uint32_t descriptor;
	};
}

template <>
struct std::hash<SIE::ShaderCompilationTask>
{
	std::size_t operator()(const SIE::ShaderCompilationTask& task) const noexcept
	{
		return task.GetId();
	}
};

namespace SIE
{
	class CompilationSet
	{
	public:
		ShaderCompilationTask WaitTake();
		void Add(const ShaderCompilationTask& task);
		void Complete(const ShaderCompilationTask& task);
		void Clear();

	private:
		std::unordered_set<ShaderCompilationTask> availableTasks;
		std::unordered_set<ShaderCompilationTask> tasksInProgress;
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
		void ProcessCompilationSet();

		~ShaderCache();

		std::array<std::unordered_map<uint32_t, std::unique_ptr<RE::BSGraphics::VertexShader>>,
			static_cast<size_t>(RE::BSShader::Type::Total)>
			vertexShaders;
		std::array<std::unordered_map<uint32_t, std::unique_ptr<RE::BSGraphics::PixelShader>>,
			static_cast<size_t>(RE::BSShader::Type::Total)>
			pixelShaders;

		bool isEnabled = false;
		uint32_t disabledClasses = 0;

		bool isAsync = true;
		CompilationSet compilationSet; 
		std::vector<std::jthread> compilationThreads;
		std::mutex vertexShadersMutex;
		std::mutex pixelShadersMutex;
	};
}
