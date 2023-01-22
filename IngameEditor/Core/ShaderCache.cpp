#include "Core/ShaderCache.h"

#include <RE/V/VertexDesc.h>

#include <magic_enum.hpp>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace SIE
{
	namespace SShaderCache
	{
		constexpr const char* VertexShaderProfile = "vs_5_0";
		constexpr const char* PixelShaderProfile = "ps_5_0";
		constexpr const char* ComputeShaderProfile = "cs_5_0";

		static std::wstring GetShaderPath(const std::string_view& name) 
		{ 
			return std::format(L"Data/SKSE/plugins/SIE/Shaders/{}.hlsl", std::wstring(name.begin(), name.end()));
		}

		static const char* GetShaderProfile(ShaderClass shaderClass)
		{ 
			switch (shaderClass)
			{
			case ShaderClass::Vertex:
				return VertexShaderProfile;
			case ShaderClass::Pixel:
				return PixelShaderProfile;
			case ShaderClass::Compute:
				return ComputeShaderProfile;
			}
		}

		uint32_t GetTechnique(uint32_t descriptor)
		{
			return 0x3F & (descriptor >> 24);
		}

		enum class LightingShaderTechniques
		{
			None					= 0,
			Envmap					= 1,
			Glowmap					= 2,
			Parallax				= 3,
			Facegen					= 4,
			FacegenRGBTint			= 5,
			Hair					= 6,
			ParallaxOcc				= 7,
			MTLand					= 8,
			LODLand					= 9,
			Snow					= 10,	// unused
			MultilayerParallax		= 11,
			TreeAnim				= 12,
			LODObjects				= 13,
			MultiIndexSparkle		= 14,
			LODObjectHD				= 15,
			Eye						= 16,
			Cloud					= 17,	// unused
			LODLandNoise			= 18,
			MTLandLODBlend			= 19,
			Outline					= 20,
		};

		enum class LightingShaderFlags
		{
			VC						= 1 << 0,
			Skinned					= 1 << 1,
			ModelSpaceNormals		= 1 << 2,
			// flags 3 to 8 are unused
			Specular				= 1 << 9,
			SoftLighting			= 1 << 10,
			RimLighting				= 1 << 11,
			BackLighting			= 1 << 12,
			ShadowDir				= 1 << 13,
			DefShadow				= 1 << 14,
			ProjectedUV				= 1 << 15,
			AnisoLighting			= 1 << 16,
			AmbientSpecular			= 1 << 17,
			WorldMap				= 1 << 18,
			BaseObjectIsSnow		= 1 << 19,
			DoAlphaTest				= 1 << 20,
			Snow					= 1 << 21,
			CharacterLight			= 1 << 22,
			AdditionalAlphaMask		= 1 << 23,
		};

		enum class BloodSplatterShaderTechniques
		{
			Splatter				= 0,
			Flare					= 1,
		};

		enum class DistantTreeShaderTechniques
		{
			DistantTreeBlock		= 0,
			Depth					= 1,
		};

		enum class DistantTreeShaderFlags
		{
			AlphaTest				= 0x10000,
		};

		enum class SkyShaderTechniques
		{
			SunOcclude				= 0,
			SunGlare				= 1,
			MoonAndStarsMask		= 2,
			Stars					= 3,
			Clouds					= 4,
			CloudsLerp				= 5,
			CloudsFade				= 6,
			Texture					= 7,
			Sky						= 8,
		};

		enum class GrassShaderTechniques
		{
			RenderDepth				= 8,
		};

		enum class GrassShaderFlags
		{
			AlphaTest				= 0x10000,
		};

		enum class ParticleShaderTechniques
		{
			Particles				= 0,
			ParticlesGryColor		= 1,
			ParticlesGryAlpha		= 2,
			ParticlesGryColorAlpha	= 3,
			EnvCubeSnow				= 4,
			EnvCubeRain				= 5,
		};

		static void GetLightingShaderDefines(uint32_t descriptor,
			D3D_SHADER_MACRO* defines)
		{
			static REL::Relocation<void(uint32_t, D3D_SHADER_MACRO*)> VanillaGetLightingShaderDefines(
				RELOCATION_ID(101631, 108698));

			const auto technique =
				static_cast<LightingShaderTechniques>(GetTechnique(descriptor));

			if (technique == LightingShaderTechniques::Outline)
			{
				defines[0] = { "OUTLINE", nullptr };
				++defines;
			}

			VanillaGetLightingShaderDefines(descriptor, defines);
		}

		static void GetBloodSplaterShaderDefines(uint32_t descriptor, D3D_SHADER_MACRO* defines)
		{
			if (descriptor == static_cast<uint32_t>(BloodSplatterShaderTechniques::Splatter))
			{
				defines[0] = { "SPLATTER", nullptr };
				++defines;
			}
			else if (descriptor == static_cast<uint32_t>(BloodSplatterShaderTechniques::Flare))
			{
				defines[0] = { "FLARE", nullptr };
				++defines;
			}
			defines[0] = { nullptr, nullptr };
		}

		static void GetDistantTreeShaderDefines(uint32_t descriptor, D3D_SHADER_MACRO* defines)
		{
			const auto technique = descriptor & 1;
			if (technique == static_cast<uint32_t>(DistantTreeShaderTechniques::Depth))
			{
				defines[0] = { "RENDER_DEPTH", nullptr };
				++defines;
			}
			if (descriptor & static_cast<uint32_t>(DistantTreeShaderFlags::AlphaTest))
			{
				defines[0] = { "DO_ALPHA_TEST", nullptr };
				++defines;
			}
			defines[0] = { nullptr, nullptr };
		}

		static void GetSkyShaderDefines(uint32_t descriptor, D3D_SHADER_MACRO* defines)
		{
			const auto technique = static_cast<SkyShaderTechniques>(descriptor);
			switch (technique)
			{
			case SkyShaderTechniques::SunOcclude:
				{
					defines[0] = { "OCCLUSION", nullptr };
					++defines;
					break;
				}
			case SkyShaderTechniques::SunGlare:
				{
					defines[0] = { "TEX", nullptr };
					defines[1] = { "DITHER", nullptr };
					defines += 2;
					break;
				}
			case SkyShaderTechniques::MoonAndStarsMask:
				{
					defines[0] = { "TEX", nullptr };
					defines[1] = { "MOONMASK", nullptr };
					defines += 2;
					break;
				}
			case SkyShaderTechniques::Stars:
				{
					defines[0] = { "HORIZFADE", nullptr };
					++defines;
					break;
				}
			case SkyShaderTechniques::Clouds:
				{
					defines[0] = { "TEX", nullptr };
					defines[1] = { "CLOUDS", nullptr };
					defines += 2;
					break;
				}
			case SkyShaderTechniques::CloudsLerp:
				{
					defines[0] = { "TEX", nullptr };
					defines[1] = { "CLOUDS", nullptr };
					defines[2] = { "TEXLERP", nullptr };
					defines += 3;
					break;
				}
			case SkyShaderTechniques::CloudsFade:
				{
					defines[0] = { "TEX", nullptr };
					defines[1] = { "CLOUDS", nullptr };
					defines[2] = { "TEXFADE", nullptr };
					defines += 3;
					break;
				}
			case SkyShaderTechniques::Texture:
				{
					defines[0] = { "TEX", nullptr };
					++defines;
					break;
				}
			case SkyShaderTechniques::Sky:
				{
					defines[0] = { "DITHER", nullptr };
					++defines;
					break;
				}
			}
			
			defines[0] = { nullptr, nullptr };
		}

		static void GetGrassShaderDefines(uint32_t descriptor, D3D_SHADER_MACRO* defines)
		{
			const auto technique = descriptor & 0b1111;
			if (technique == static_cast<uint32_t>(GrassShaderTechniques::RenderDepth))
			{
				defines[0] = { "RENDER_DEPTH", nullptr };
				++defines;
			}
			if (descriptor & static_cast<uint32_t>(GrassShaderFlags::AlphaTest))
			{
				defines[0] = { "DO_ALPHA_TEST", nullptr };
				++defines;
			}
			defines[0] = { nullptr, nullptr };
		}

		static void GetParticleShaderDefines(uint32_t descriptor, D3D_SHADER_MACRO* defines)
		{
			const auto technique = static_cast<ParticleShaderTechniques>(descriptor);
			switch (technique)
			{
			case ParticleShaderTechniques::ParticlesGryColor:
				{
					defines[0] = { "GRAYSCALE_TO_COLOR", nullptr };
					++defines;
					break;
				}
			case ParticleShaderTechniques::ParticlesGryAlpha:
				{
					defines[0] = { "GRAYSCALE_TO_ALPHA", nullptr };
					++defines;
					break;
				}
			case ParticleShaderTechniques::ParticlesGryColorAlpha:
				{
					defines[0] = { "GRAYSCALE_TO_COLOR", nullptr };
					defines[1] = { "GRAYSCALE_TO_ALPHA", nullptr };
					defines += 2;
					break;
				}
			case ParticleShaderTechniques::EnvCubeSnow:
				{
					defines[0] = { "ENVCUBE", nullptr };
					defines[1] = { "SNOW", nullptr };
					defines += 2;
					break;
				}
			case ParticleShaderTechniques::EnvCubeRain:
				{
					defines[0] = { "ENVCUBE", nullptr };
					defines[1] = { "RAIN", nullptr };
					defines += 2;
					break;
				}
			}

			defines[0] = { nullptr, nullptr };
		}

		static void GetShaderDefines(RE::BSShader::Type type, uint32_t descriptor,
			D3D_SHADER_MACRO* defines)
		{
			switch (type)
			{
			case RE::BSShader::Type::Grass:
				GetGrassShaderDefines(descriptor, defines);
				break;
			case RE::BSShader::Type::Sky:
				GetSkyShaderDefines(descriptor, defines);
				break;
			case RE::BSShader::Type::BloodSplatter:
				GetBloodSplaterShaderDefines(descriptor, defines);
				break;
			case RE::BSShader::Type::Lighting:
				GetLightingShaderDefines(descriptor, defines);
				break;
			case RE::BSShader::Type::DistantTree:
				GetDistantTreeShaderDefines(descriptor, defines);
				break;
			case RE::BSShader::Type::Particle:
				GetParticleShaderDefines(descriptor, defines);
				break;
			}
		}

		static std::array<std::array<std::unordered_map<std::string, int32_t>,
							  static_cast<size_t>(ShaderClass::Total)>,
			static_cast<size_t>(RE::BSShader::Type::Total)>
			GetVariableIndices()
		{
			std::array<std::array<std::unordered_map<std::string, int32_t>,
						   static_cast<size_t>(ShaderClass::Total)>,
				static_cast<size_t>(RE::BSShader::Type::Total)>
				result;

			auto& lightingVS =
				result[static_cast<size_t>(RE::BSShader::Type::Lighting)][static_cast<size_t>(ShaderClass::Vertex)];
			lightingVS = {
				{ "HighDetailRange", 12 },
				{ "FogParam", 13 },
				{ "FogNearColor", 14 },
				{ "FogFarColor", 15 },
				{ "LeftEyeCenter", 9 },
				{ "RightEyeCenter", 10 },
				{ "TexcoordOffset", 11 },
				{ "World", 0 },
				{ "PreviousWorld", 1 },
				{ "EyePosition", 2 },
				{ "LandBlendParams", 3 },
				{ "TreeParams", 4 },
				{ "WindTimers", 5 },
				{ "TextureProj", 6 },
				{ "IndexScale", 7 },
				{ "WorldMapOverlayParameters", 8 },
				{ "Bones", 16 },
			};

			auto& lightingPS = result[static_cast<size_t>(RE::BSShader::Type::Lighting)]
									 [static_cast<size_t>(ShaderClass::Pixel)];
			lightingPS = {
				{ "VPOSOffset", 11 },
				{ "FogColor", 19 },
				{ "ColourOutputClamp", 20 },
				{ "EnvmapData", 21 },
				{ "ParallaxOccData", 22 },
				{ "TintColor", 23 },
				{ "LODTexParams", 24 },
				{ "SpecularColor", 25 },
				{ "SparkleParams", 26 },
				{ "MultiLayerParallaxData", 27 },
				{ "LightingEffectParams", 28 },
				{ "IBLParams", 29 },
				{ "LandscapeTexture1to4IsSnow", 30 },
				{ "LandscapeTexture5to6IsSnow", 31 },
				{ "LandscapeTexture1to4IsSpecPower", 32 },
				{ "LandscapeTexture5to6IsSpecPower", 33 },
				{ "SnowRimLightParameters", 34 },
				{ "CharacterLightParams", 35 },
				{ "NumLightNumShadowLight", 0 },
				{ "PointLightPosition", 1 },
				{ "PointLightColor", 2 },
				{ "DirLightDirection", 3 },
				{ "DirLightColor", 4 },
				{ "DirectionalAmbient", 5 },
				{ "AmbientSpecularTintAndFresnelPower", 6 },
				{ "MaterialData", 7 },
				{ "EmitColor", 8 },
				{ "AlphaTestRef", 9 },
				{ "ShadowLightMaskSelect", 10 },
				{ "ProjectedUVParams", 12 },
				{ "ProjectedUVParams2", 13 },
				{ "ProjectedUVParams3", 14 },
				{ "SplitDistance", 15 },
				{ "SSRParams", 16 },
				{ "WorldMapOverlayParametersPS", 17 },
				{ "AmbientColor", 18 },
			};

			auto& bloodSplatterVS = result[static_cast<size_t>(RE::BSShader::Type::BloodSplatter)]
									 [static_cast<size_t>(ShaderClass::Vertex)];
			bloodSplatterVS = {
				{ "WorldViewProj", 0 },
				{ "LightLoc", 1 },
				{ "Ctrl", 2 },
			};

			auto& bloodSplatterPS = result[static_cast<size_t>(RE::BSShader::Type::BloodSplatter)]
										  [static_cast<size_t>(ShaderClass::Pixel)];
			bloodSplatterPS = {
				{ "Alpha", 0 },
			};

			auto& distantTreeVS = result[static_cast<size_t>(RE::BSShader::Type::DistantTree)]
										  [static_cast<size_t>(ShaderClass::Vertex)];
			distantTreeVS = {
				{ "WorldViewProj", 1 },
				{ "World", 2 },
				{ "PreviousWorld", 3 },
				{ "FogParam", 4 },
			};

			auto& distantTreePS = result[static_cast<size_t>(RE::BSShader::Type::DistantTree)]
										  [static_cast<size_t>(ShaderClass::Pixel)];
			distantTreePS = {
				{ "DiffuseColor", 0 },
				{ "AmbientColor", 1 },
			};

			auto& skyVS = result[static_cast<size_t>(RE::BSShader::Type::Sky)]
										[static_cast<size_t>(ShaderClass::Vertex)];
			skyVS = {
				{ "WorldViewProj", 0 },
				{ "World", 1 },
				{ "PreviousWorld", 2 },
				{ "BlendColor", 3 },
				{ "EyePosition", 4 },
				{ "TexCoordOff", 5 },
				{ "VParams", 6 },
			};

			auto& skyPS = result[static_cast<size_t>(RE::BSShader::Type::Sky)]
										[static_cast<size_t>(ShaderClass::Pixel)];
			skyPS = {
				{ "PParams", 0 },
			};

			auto& grassVS = result[static_cast<size_t>(RE::BSShader::Type::Grass)]
										[static_cast<size_t>(ShaderClass::Vertex)];
			grassVS = {
				{ "WorldViewProj", 0 },
				{ "WorldView", 1 },
				{ "World", 2 },
				{ "PreviousWorld", 3 },
				{ "FogNearColor", 4 },
				{ "WindVector", 5 },
				{ "WindTimer", 6 },
				{ "DirLightDirection", 7 },
				{ "PreviousWindTimer", 8 },
				{ "DirLightColor", 9 },
				{ "AlphaParam1", 10 },
				{ "AmbientColor", 11 },
				{ "AlphaParam2", 12 },
				{ "ScaleMask", 13 },
				{ "ShadowClampValue", 14 },
			};

			auto& particleVS = result[static_cast<size_t>(RE::BSShader::Type::Particle)]
								[static_cast<size_t>(ShaderClass::Vertex)];
			particleVS = {
				{ "WorldViewProj", 0 },
				{ "PrevWorldViewProj", 1 },
				{ "PrecipitationOcclusionWorldViewProj", 2 },
				{ "fVars0", 3 },
				{ "fVars1", 4 },
				{ "fVars2", 5 },
				{ "fVars3", 6 },
				{ "fVars4", 7 },
				{ "Color1", 8 },
				{ "Color2", 9 },
				{ "Color3", 10 },
				{ "Velocity", 11 },
				{ "Acceleration", 12 },
				{ "ScaleAdjust", 13 },
				{ "Wind", 14 },
			};

			auto& particlePS = result[static_cast<size_t>(RE::BSShader::Type::Particle)]
								[static_cast<size_t>(ShaderClass::Pixel)];
			particlePS = {
				{ "ColorScale", 0 },
				{ "TextureSize", 1 },
			};

			return result;
		}

		static int32_t GetVariableIndex(ShaderClass shaderClass, RE::BSShader::Type shaderType, const char* name)
		{
			static auto variableNames = GetVariableIndices();

			const auto& names =
				variableNames[static_cast<size_t>(shaderType)][static_cast<size_t>(shaderClass)];
			auto it = names.find(name);
			if (it == names.cend())
			{
				return -1;
			}

			return it->second;
		}

		static std::string MergeDefinesString(const std::array<D3D_SHADER_MACRO, 64>& defines)
		{ 
			std::string result;
			for (const auto& def : defines)
			{
				if (def.Name != nullptr)
				{
					result += def.Name;
					result += ' ';
				}
				else
				{
					break;
				}
			}
			return result;
		}

		static void AddAttribute(uint64_t& desc, RE::BSGraphics::Vertex::Attribute attribute) 
		{ 
			desc |= ((1ull << (44 + attribute)) | (1ull << (54 + attribute)) |
					 (0b1111ull << (4 * attribute + 4)));
		}

		template<size_t MaxOffsetsSize>
		static void ReflectConstantBuffers(ID3D11ShaderReflection& reflector,
			std::array<size_t, 3>& bufferSizes,
			std::array<int8_t, MaxOffsetsSize>& constantOffsets,
			uint64_t& vertexDesc,
			ShaderClass shaderClass, RE::BSShader::Type shaderType, uint32_t descriptor)
		{
			D3D11_SHADER_DESC desc;
			if (FAILED(reflector.GetDesc(&desc)))
			{
				logger::error("Failed to get shader descriptor for {} shader {}::{}",
					magic_enum::enum_name(shaderClass), magic_enum::enum_name(shaderType),
					descriptor);
				return;
			}

			if (desc.ConstantBuffers <= 0)
			{
				return;
			}

			if (shaderClass == ShaderClass::Vertex)
			{
				vertexDesc = 0b1111;
				bool hasTexcoord2 = false;
				bool hasTexcoord3 = false;
				for (uint32_t inputIndex = 0; inputIndex < desc.InputParameters; ++inputIndex)
				{
					D3D11_SIGNATURE_PARAMETER_DESC inputDesc;
					if (FAILED(reflector.GetInputParameterDesc(inputIndex, &inputDesc)))
					{
						logger::error(
							"Failed to get input parameter {} descriptor for {} shader {}::{}",
							inputIndex, magic_enum::enum_name(shaderClass),
							magic_enum::enum_name(shaderType),
							descriptor);
					}
					else
					{
						std::string_view semanticName = inputDesc.SemanticName;
						if (semanticName == "POSITION" && inputDesc.SemanticIndex == 0)
						{
							AddAttribute(vertexDesc, RE::BSGraphics::Vertex::VA_POSITION);
						}
						else if (semanticName == "TEXCOORD" &&
								 inputDesc.SemanticIndex == 0)
						{
							AddAttribute(vertexDesc, RE::BSGraphics::Vertex::VA_TEXCOORD0);
						}
						else if (semanticName == "TEXCOORD" && inputDesc.SemanticIndex == 1)
						{
							AddAttribute(vertexDesc, RE::BSGraphics::Vertex::VA_TEXCOORD1);
						}
						else if (semanticName == "NORMAL" &&
								 inputDesc.SemanticIndex == 0)
						{
							AddAttribute(vertexDesc, RE::BSGraphics::Vertex::VA_NORMAL);
						}
						else if (semanticName == "BINORMAL" && inputDesc.SemanticIndex == 0)
						{
							AddAttribute(vertexDesc, RE::BSGraphics::Vertex::VA_BINORMAL);
						}
						else if (semanticName == "COLOR" &&
								 inputDesc.SemanticIndex == 0)
						{
							AddAttribute(vertexDesc, RE::BSGraphics::Vertex::VA_COLOR);
						}
						else if (semanticName == "BLENDWEIGHT" && inputDesc.SemanticIndex == 0)
						{
							AddAttribute(vertexDesc, RE::BSGraphics::Vertex::VA_SKINNING);
						}
						else if (semanticName == "TEXCOORD" && inputDesc.SemanticIndex >= 4 &&
								 inputDesc.SemanticIndex <= 7)
						{
							AddAttribute(vertexDesc, RE::BSGraphics::Vertex::VA_INSTANCEDATA);
						}
						else if (semanticName == "TEXCOORD" &&
								 inputDesc.SemanticIndex == 2)
						{
							hasTexcoord2 = true;
						}
						else if (semanticName == "TEXCOORD" && inputDesc.SemanticIndex == 3)
						{
							hasTexcoord3 = true;
						}
					}
				}
				if (hasTexcoord2)
				{
					if (hasTexcoord3)
					{
						AddAttribute(vertexDesc, RE::BSGraphics::Vertex::VA_LANDDATA);
					}
					else
					{
						AddAttribute(vertexDesc, RE::BSGraphics::Vertex::VA_EYEDATA);
					}
				}
			}

			auto mapBufferConsts =
				[&](const char* bufferName, size_t& bufferSize)
			{
				auto bufferReflector = reflector.GetConstantBufferByName(bufferName);
				if (bufferReflector == nullptr)
				{
					logger::warn("Buffer {} not found for {} shader {}::{}",
						bufferName, magic_enum::enum_name(shaderClass),
						magic_enum::enum_name(shaderType),
						descriptor);
					return;
				}

				D3D11_SHADER_BUFFER_DESC bufferDesc;
				if (FAILED(bufferReflector->GetDesc(&bufferDesc)))
				{
					logger::warn("Failed to get buffer {} descriptor for {} shader {}::{}",
						bufferName, magic_enum::enum_name(shaderClass),
						magic_enum::enum_name(shaderType),
						descriptor);
					return;
				}

				for (uint32_t i = 0; i < bufferDesc.Variables; i++)
				{
					ID3D11ShaderReflectionVariable* var = bufferReflector->GetVariableByIndex(i);

					D3D11_SHADER_VARIABLE_DESC varDesc;
					if (FAILED(var->GetDesc(&varDesc)))
					{
						logger::error("Failed to get variable descriptor for {} shader {}::{}",
							magic_enum::enum_name(shaderClass), magic_enum::enum_name(shaderType),
							descriptor);
						continue;
					}

					const auto variableIndex =
						GetVariableIndex(shaderClass, shaderType, varDesc.Name);
					if (variableIndex != -1)
					{
						constantOffsets[variableIndex] = varDesc.StartOffset / 4;
					}
					else
					{
						logger::error("Unknown variable name {} in {} shader {}::{}",
							varDesc.Name, magic_enum::enum_name(shaderClass),
							magic_enum::enum_name(shaderType),
							descriptor);
					}
				}

				bufferSize = ((bufferDesc.Size + 15) & ~15) / 16;
			};

			mapBufferConsts("PerTechnique", bufferSizes[0]);
			mapBufferConsts("PerMaterial", bufferSizes[1]);
			mapBufferConsts("PerGeometry", bufferSizes[2]);
		}

		static ID3DBlob* CompileShader(ShaderClass shaderClass, const RE::BSShader& shader,
			uint32_t descriptor)
		{
			const auto type = shader.shaderType.get();
			const std::wstring path = GetShaderPath(shader.fxpFilename);

			std::array<D3D_SHADER_MACRO, 64> defines;
			if (shaderClass == ShaderClass::Vertex)
			{
				defines[0] = { "VSHADER", nullptr };
			}
			else if (shaderClass == ShaderClass::Pixel)
			{
				defines[0] = { "PSHADER", nullptr };
			}
			defines[1] = { nullptr, nullptr };
			GetShaderDefines(type, descriptor, &defines[1]);

			//logger::info("{}, {}", descriptor, MergeDefinesString(defines));

			ID3DBlob* shaderBlob = nullptr;
			ID3DBlob* errorBlob = nullptr;
			const uint32_t flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
			const HRESULT compileResult = D3DCompileFromFile(path.c_str(), defines.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE, "main",
					GetShaderProfile(shaderClass), flags, 0, &shaderBlob, &errorBlob);

			if (FAILED(compileResult))
			{
				if (errorBlob != nullptr)
				{
					logger::error("Failed to compile {} shader {}::{}: {}",
						magic_enum::enum_name(shaderClass), magic_enum::enum_name(type), descriptor,
						static_cast<char*>(errorBlob->GetBufferPointer()));
					errorBlob->Release();
				}
				if (shaderBlob != nullptr)
				{
					shaderBlob->Release();
				}

				return nullptr;
			}
			logger::info("Compiled {} shader {}::{}", magic_enum::enum_name(shaderClass),
				magic_enum::enum_name(type), descriptor);

			return shaderBlob;
		}

		std::unique_ptr<RE::BSGraphics::VertexShader> CreateVertexShader(ID3DBlob& shaderData,
			RE::BSShader::Type type, uint32_t descriptor) 
		{
			static const auto device = REL::Relocation<ID3D11Device**>(RE::Offset::D3D11Device);
			static const auto perTechniqueBuffersArray =
				REL::Relocation<ID3D11Buffer**>(RELOCATION_ID(524755, 411371));
			static const auto perMaterialBuffersArray =
				REL::Relocation<ID3D11Buffer**>(RELOCATION_ID(524757, 411373));
			static const auto perGeometryBuffersArray =
				REL::Relocation<ID3D11Buffer**>(RELOCATION_ID(524759, 411375));
			static const auto bufferData = REL::Relocation<void*>(RELOCATION_ID(524965, 411446));

			auto rawPtr =
				new uint8_t[sizeof(RE::BSGraphics::VertexShader) + shaderData.GetBufferSize()];
			auto shaderPtr = new (rawPtr) RE::BSGraphics::VertexShader;
			memcpy(rawPtr + sizeof(RE::BSGraphics::VertexShader), shaderData.GetBufferPointer(),
				shaderData.GetBufferSize());
			auto newShader = std::unique_ptr<RE::BSGraphics::VertexShader>(shaderPtr);
			newShader->byteCodeSize = shaderData.GetBufferSize();
			newShader->id = descriptor;
			newShader->shaderDesc = 0;

			Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflector;
			const auto reflectionResult = D3DReflect(shaderData.GetBufferPointer(), shaderData.GetBufferSize(),
				IID_PPV_ARGS(&reflector));
			if (FAILED(reflectionResult))
			{
				logger::error("Failed to reflect vertex shader {}::{}", magic_enum::enum_name(type),
					descriptor);
			}
			else
			{
				std::array<size_t, 3> bufferSizes = { 0, 0, 0 };
				std::fill(newShader->constantTable.begin(), newShader->constantTable.end(), 0);
				ReflectConstantBuffers(*reflector.Get(), bufferSizes, newShader->constantTable, newShader->shaderDesc,
					ShaderClass::Vertex, type, descriptor);
				if (bufferSizes[0] != 0)
				{
					newShader->constantBuffers[0].buffer =
						perTechniqueBuffersArray.get()[bufferSizes[0]];
				}
				else
				{
					newShader->constantBuffers[0].buffer = nullptr;
					newShader->constantBuffers[0].data = bufferData.get();
				}
				if (bufferSizes[1] != 0)
				{
					newShader->constantBuffers[1].buffer =
						perMaterialBuffersArray.get()[bufferSizes[1]];
				}
				else
				{
					newShader->constantBuffers[1].buffer = nullptr;
					newShader->constantBuffers[1].data = bufferData.get();
				}
				if (bufferSizes[2] != 0)
				{
					newShader->constantBuffers[2].buffer =
						perGeometryBuffersArray.get()[bufferSizes[2]];
				}
				else
				{
					newShader->constantBuffers[2].buffer = nullptr;
					newShader->constantBuffers[2].data = bufferData.get();
				}
			}

			const auto result = (*device)->CreateVertexShader(shaderData.GetBufferPointer(), newShader->byteCodeSize, nullptr, &newShader->shader);
			if (FAILED(result))
			{
				logger::error("Failed to create vertex shader {}::{}", magic_enum::enum_name(type),
					descriptor);
				if (newShader->shader != nullptr)
				{
					newShader->shader->Release();
				}
				return nullptr;
			}  
			return newShader;
		}

		std::unique_ptr<RE::BSGraphics::PixelShader> CreatePixelShader(ID3DBlob& shaderData,
			RE::BSShader::Type type, uint32_t descriptor)
		{
			static const auto device = REL::Relocation<ID3D11Device**>(RE::Offset::D3D11Device);
			static const auto perTechniqueBuffersArray =
				REL::Relocation<ID3D11Buffer**>(RELOCATION_ID(524761, 411377));
			static const auto perMaterialBuffersArray =
				REL::Relocation<ID3D11Buffer**>(RELOCATION_ID(524763, 411379));
			static const auto perGeometryBuffersArray =
				REL::Relocation<ID3D11Buffer**>(RELOCATION_ID(524765, 411381));
			static const auto bufferData = REL::Relocation<void*>(RELOCATION_ID(524967, 411448));

			auto newShader = std::make_unique<RE::BSGraphics::PixelShader>();
			newShader->id = descriptor;
			
			Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflector;
			const auto reflectionResult = D3DReflect(shaderData.GetBufferPointer(),
				shaderData.GetBufferSize(), IID_PPV_ARGS(&reflector));
			if (FAILED(reflectionResult))
			{
				logger::error("Failed to reflect vertex shader {}::{}", magic_enum::enum_name(type),
					descriptor);
			}
			else
			{
				std::array<size_t, 3> bufferSizes = { 0, 0, 0 };
				std::fill(newShader->constantTable.begin(), newShader->constantTable.end(), 0);
				uint64_t dummy;
				ReflectConstantBuffers(*reflector.Get(), bufferSizes, newShader->constantTable,
					dummy,
					ShaderClass::Pixel, type, descriptor);
				if (bufferSizes[0] != 0)
				{
					newShader->constantBuffers[0].buffer =
						perTechniqueBuffersArray.get()[bufferSizes[0]];
				}
				else
				{
					newShader->constantBuffers[0].buffer = nullptr;
					newShader->constantBuffers[0].data = bufferData.get();
				}
				if (bufferSizes[1] != 0)
				{
					newShader->constantBuffers[1].buffer =
						perMaterialBuffersArray.get()[bufferSizes[1]];
				}
				else
				{
					newShader->constantBuffers[1].buffer = nullptr;
					newShader->constantBuffers[1].data = bufferData.get();
				}
				if (bufferSizes[2] != 0)
				{
					newShader->constantBuffers[2].buffer =
						perGeometryBuffersArray.get()[bufferSizes[2]];
				}
				else
				{
					newShader->constantBuffers[2].buffer = nullptr;
					newShader->constantBuffers[2].data = bufferData.get();
				}
			}

			const auto result = (*device)->CreatePixelShader(shaderData.GetBufferPointer(),
				shaderData.GetBufferSize(),
				nullptr, &newShader->shader);
			if (FAILED(result))
			{
				logger::error("Failed to create pixel shader {}::{}", magic_enum::enum_name(type),
					descriptor);
				if (newShader->shader != nullptr)
				{
					newShader->shader->Release();
				}
				return nullptr;
			}  
			return newShader;
		}

		static bool IsSupportedShader(const RE::BSShader& shader)
		{
			return shader.shaderType == RE::BSShader::Type::Lighting ||
			       shader.shaderType == RE::BSShader::Type::BloodSplatter ||
			       shader.shaderType == RE::BSShader::Type::DistantTree ||
			       shader.shaderType == RE::BSShader::Type::Sky ||
			       shader.shaderType == RE::BSShader::Type::Grass ||
			       shader.shaderType == RE::BSShader::Type::Particle;
		}
	}

	RE::BSGraphics::VertexShader* ShaderCache::GetVertexShader(const RE::BSShader& shader,
		uint32_t descriptor)
	{
		if (!SShaderCache::IsSupportedShader(shader))
		{
			return nullptr;
		}

		auto& typeCache = vertexShaders[static_cast<size_t>(shader.shaderType.underlying())];
		auto it = typeCache.find(descriptor);
		if (it == typeCache.end())
		{
			const auto shaderBlob =
				SShaderCache::CompileShader(ShaderClass::Vertex, shader, descriptor);
			if (shaderBlob == nullptr)
			{
				return nullptr;
			}
			if (auto newShader = SShaderCache::CreateVertexShader(*shaderBlob,
					shader.shaderType.get(), descriptor))
			{
				it = typeCache.insert_or_assign(descriptor, std::move(newShader)).first;
			}
			else
			{
				return nullptr;
			}
		}
		return it->second.get();
	}

	RE::BSGraphics::PixelShader* ShaderCache::GetPixelShader(const RE::BSShader& shader,
		uint32_t descriptor)
	{
		if (!SShaderCache::IsSupportedShader(shader))
		{
			return nullptr;
		}

		auto& typeCache = pixelShaders[static_cast<size_t>(shader.shaderType.underlying())];
		auto it = typeCache.find(descriptor);
		if (it == typeCache.end())
		{
			const auto shaderBlob =
				SShaderCache::CompileShader(ShaderClass::Pixel, shader, descriptor);
			if (shaderBlob == nullptr)
			{
				return nullptr;
			}
			if (auto newShader = SShaderCache::CreatePixelShader(*shaderBlob,
					shader.shaderType.get(), descriptor))
			{
				it = typeCache.insert_or_assign(descriptor, std::move(newShader)).first;
			}
			else
			{
				return nullptr;
			}
		}
		return it->second.get();
	}

	ShaderCache::~ShaderCache() 
	{ 
		Clear();
	}

	bool ShaderCache::IsEnabledForClass(ShaderClass shaderClass) const 
	{
		return isEnabled && !(disabledClasses & (1 << static_cast<uint32_t>(shaderClass)));
	}

	void ShaderCache::SetEnabledForClass(ShaderClass shaderClass, bool value)
	{
		if (value)
		{
			disabledClasses &= ~(1 << static_cast<uint32_t>(shaderClass));
		}
		else
		{
			disabledClasses |= (1 << static_cast<uint32_t>(shaderClass));
		}
	}

	void ShaderCache::Clear()
	{
		for (auto& shaders : vertexShaders)
		{
			for (auto& [id, shader] : shaders)
			{
				shader->shader->Release();
			}
			shaders.clear();
		}
		for (auto& shaders : pixelShaders)
		{
			for (auto& [id, shader] : shaders)
			{
				shader->shader->Release();
			}
			shaders.clear();
		}
	}

	bool ShaderCache::IsEnabled() const
	{ 
		return isEnabled;
	}

	void ShaderCache::SetEnabled(bool value)
	{ 
		isEnabled = value;
	}
}
