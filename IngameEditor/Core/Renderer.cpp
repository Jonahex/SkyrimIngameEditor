#include "Core/Renderer.h"

#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSSkyShaderProperty.h>
#include <RE/B/BSSkyShader.h>

namespace SIE
{
	void Renderer::SetEnabled(bool inIsEnabled)
	{
		if (inIsEnabled == isEnabled)
		{
			return;
		}

		if (inIsEnabled)
		{
			BSSkyShaderProperty__GetRenderPasses =
				stl::write_vfunc(RE::VTABLE_BSSkyShaderProperty[0], 0x2A,
					&RE::BSSkyShaderProperty::GetRenderPassesImpl);
			BSSkyShader__SetupTechnique = stl::write_vfunc(RE::VTABLE_BSSkyShader[0], 0x2,
				&RE::BSSkyShader::SetupTechniqueImpl);
			BSSkyShader__RestoreTechnique = stl::write_vfunc(RE::VTABLE_BSSkyShader[0], 0x3,
				&RE::BSSkyShader::RestoreTechniqueImpl);
			BSSkyShader__SetupGeometry = stl::write_vfunc(RE::VTABLE_BSSkyShader[0], 0x6,
				&RE::BSSkyShader::SetupGeometryImpl);
			BSSkyShader__RestoreGeometry = stl::write_vfunc(RE::VTABLE_BSSkyShader[0], 0x7,
				&RE::BSSkyShader::RestoreGeometryImpl);

			BSLightingShaderProperty__GetRenderPasses =
				stl::write_vfunc(RE::VTABLE_BSLightingShaderProperty[0], 0x2A,
					&RE::BSLightingShaderProperty::GetRenderPassesImpl);
			BSLightingShaderProperty__GetDepthAndShadowmapRenderPasses =
				stl::write_vfunc(RE::VTABLE_BSLightingShaderProperty[0], 0x2B,
					&RE::BSLightingShaderProperty::GetDepthAndShadowmapRenderPassesImpl);
			BSLightingShaderProperty__GetLocalMapRenderPasses =
				stl::write_vfunc(RE::VTABLE_BSLightingShaderProperty[0], 0x2C,
					&RE::BSLightingShaderProperty::GetLocalMapRenderPassesImpl);
			BSLightingShaderProperty__GetPrecipitationOcclusionMapRenderPasses =
				stl::write_vfunc(RE::VTABLE_BSLightingShaderProperty[0], 0x2D,
					&RE::BSLightingShaderProperty::GetPrecipitationOcclusionMapRenderPassesImpl);
		}
		else
		{
			stl::write_vfunc(RE::VTABLE_BSSkyShaderProperty[0], 0x2A,
				BSSkyShaderProperty__GetRenderPasses);
			stl::write_vfunc(RE::VTABLE_BSSkyShader[0], 0x2, BSSkyShader__SetupTechnique);
			stl::write_vfunc(RE::VTABLE_BSSkyShader[0], 0x3, BSSkyShader__RestoreTechnique);
			stl::write_vfunc(RE::VTABLE_BSSkyShader[0], 0x6, BSSkyShader__SetupGeometry);
			stl::write_vfunc(RE::VTABLE_BSSkyShader[0], 0x7, BSSkyShader__RestoreGeometry);

			stl::write_vfunc(RE::VTABLE_BSLightingShaderProperty[0], 0x2A,
				BSLightingShaderProperty__GetRenderPasses);
			stl::write_vfunc(RE::VTABLE_BSLightingShaderProperty[0], 0x2B,
				BSLightingShaderProperty__GetDepthAndShadowmapRenderPasses);
			stl::write_vfunc(RE::VTABLE_BSLightingShaderProperty[0], 0x2C,
				BSLightingShaderProperty__GetLocalMapRenderPasses);
			stl::write_vfunc(RE::VTABLE_BSLightingShaderProperty[0], 0x2D,
				BSLightingShaderProperty__GetPrecipitationOcclusionMapRenderPasses);
		}

		isEnabled = inIsEnabled;
	}

	bool Renderer::IsEnabled() const
	{
		return isEnabled;
	}
}
