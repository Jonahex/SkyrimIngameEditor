#pragma once

namespace SIE
{
	class Renderer
	{
	public:
		static Renderer& Instance()
		{
			static Renderer instance;
			return instance;
		}

		void SetEnabled(bool inIsEnabled);
		bool IsEnabled() const;

	private:
		bool isEnabled = false;

		uintptr_t BSSkyShaderProperty__GetRenderPasses = 0;
		uintptr_t BSSkyShader__SetupTechnique = 0;
		uintptr_t BSSkyShader__RestoreTechnique = 0;
		uintptr_t BSSkyShader__SetupGeometry = 0;
		uintptr_t BSSkyShader__RestoreGeometry = 0;

		uintptr_t BSLightingShaderProperty__GetRenderPasses = 0;
		uintptr_t BSLightingShaderProperty__GetDepthAndShadowmapRenderPasses = 0;
		uintptr_t BSLightingShaderProperty__GetLocalMapRenderPasses = 0;
		uintptr_t BSLightingShaderProperty__GetPrecipitationOcclusionMapRenderPasses = 0;
	};
}
