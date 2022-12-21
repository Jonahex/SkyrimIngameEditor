#include "Gui/NiTransformEditor.h"

#include "Gui/Utils.h"

#include "3rdparty/ImGuizmo/ImGuizmo.h"

#include <RE/N/NiCamera.h>
#include <RE/N/NiTransform.h>
#include <RE/P/PlayerCamera.h>

namespace SIE
{
	namespace SNiTransformEditor
	{
		static void FillImGuizmoViewMatrix(const RE::NiCamera& camera, float view[4][4])
		{
			for (size_t rowIndex = 0; rowIndex < 3; ++rowIndex)
			{
				view[3][rowIndex] = 0.f;
				view[rowIndex][3] = 0.f;
				for (size_t columnIndex = 0; columnIndex < 3; ++columnIndex)
				{
					view[rowIndex][columnIndex] = camera.world.rotate.entry[rowIndex][columnIndex];
					view[3][rowIndex] -= camera.world.rotate.entry[columnIndex][rowIndex] *
					                     camera.world.translate[columnIndex];
				}
			}
			view[3][3] = 1.f;
		}

		static void FillImGuizmoProjectionMatrix(const RE::NiCamera& camera, float projection[4][4])
		{
			for (size_t rowIndex = 0; rowIndex < 4; ++rowIndex)
			{
				for (size_t columnIndex = 0; columnIndex < 4; ++columnIndex)
				{
					projection[rowIndex][columnIndex] = 0.f;
				}
			}
			projection[0][0] = -(camera.viewFrustum.fLeft + camera.viewFrustum.fRight) /
			                   (camera.viewFrustum.fRight - camera.viewFrustum.fLeft);
			projection[2][0] = 2.f / (camera.viewFrustum.fRight - camera.viewFrustum.fLeft);
			projection[0][1] = -(camera.viewFrustum.fTop + camera.viewFrustum.fBottom) /
			                   (camera.viewFrustum.fTop - camera.viewFrustum.fBottom);
			projection[1][1] = 2.f / (camera.viewFrustum.fTop - camera.viewFrustum.fBottom);
			projection[0][2] =
				camera.viewFrustum.fFar / (camera.viewFrustum.fFar - camera.viewFrustum.fNear);
			projection[3][2] = -camera.viewFrustum.fFar * camera.viewFrustum.fNear /
			                   (camera.viewFrustum.fFar - camera.viewFrustum.fNear);
			projection[0][3] = 1.f;
		}

		static void Transpose(float src[4][4], float dst[4][4])
		{
			for (size_t rowIndex = 0; rowIndex < 4; ++rowIndex)
			{
				for (size_t columnIndex = 0; columnIndex < 4; ++columnIndex)
				{
					dst[rowIndex][columnIndex] = src[columnIndex][rowIndex];
				}
			}
		}

		static void IdentityMatrix(float matrix[4][4])
		{
			for (size_t rowIndex = 0; rowIndex < 4; ++rowIndex)
			{
				for (size_t columnIndex = 0; columnIndex < 4; ++columnIndex)
				{
					matrix[rowIndex][columnIndex] = (rowIndex == columnIndex) ? 1.f : 0.f;
				}
			}
		}

		static void FillImGuizmoModelMatrix(const RE::NiTransform& transform, float model[4][4])
		{
			for (size_t rowIndex = 0; rowIndex < 3; ++rowIndex)
			{
				model[3][rowIndex] = transform.translate[rowIndex];
				model[rowIndex][3] = 0.f;
				for (size_t columnIndex = 0; columnIndex < 3; ++columnIndex)
				{
					model[rowIndex][columnIndex] = transform.rotate.entry[columnIndex][rowIndex] * transform.scale;
				}
			}
			model[3][3] = 1.f;

			/*constexpr float rad2Deg = 180.f / 3.141592f;
			RE::NiPoint3 euler, scale;
			transform.rotate.ToEulerAnglesXYZ(euler);
			euler.x *= rad2Deg;
			euler.y *= rad2Deg;
			euler.z *= rad2Deg;
			scale.x = scale.y = scale.z = transform.scale;
			ImGuizmo::RecomposeMatrixFromComponents(&transform.translate.x, &euler.x, &scale.x,
				reinterpret_cast<float*>(model));*/
		}

		static void FillTransformFromImGuizmoModelMatrix(RE::NiTransform& transform, float model[4][4])
		{
			for (size_t index = 0; index < 3; ++index)
			{
				transform.translate[index] = model[3][index];
			}
			transform.scale = std::sqrt(
				model[0][0] * model[0][0] + model[0][1] * model[0][1] + model[0][2] * model[0][2]);
			for (size_t rowIndex = 0; rowIndex < 3; ++rowIndex)
			{
				for (size_t columnIndex = 0; columnIndex < 3; ++columnIndex)
				{
					transform.rotate.entry[columnIndex][rowIndex] = model[rowIndex][columnIndex] / transform.scale;
				}
			}

			/*constexpr float deg2Rad = 3.141592f / 180.f;
			RE::NiPoint3 euler, scale;
			ImGuizmo::DecomposeMatrixToComponents(reinterpret_cast<float*>(model),
				&transform.translate.x, &euler.x, &scale.x);
			euler.x *= deg2Rad;
			euler.y *= deg2Rad;
			euler.z *= deg2Rad;
			transform.rotate.SetEulerAnglesXYZ(euler);
			transform.scale = scale.x;*/
		}

		static bool GizmoEditor(float model[4][4]) 
		{
			static bool showGizmo = true;
			static bool isLocal = false;
			static bool translate = true;
			static bool rotate = true;
			static bool scale = true;

			ImGui::Checkbox("Show Gizmo", &showGizmo);
			if (!showGizmo)
			{
				return false;
			}

			ImGui::Checkbox("Local", &isLocal);
			ImGui::Checkbox("Translate", &translate);
			ImGui::SameLine();
			ImGui::Checkbox("Rotate", &rotate);
			ImGui::SameLine();
			ImGui::Checkbox("Scale", &scale);

			int operation = 0;
			if (translate)
			{
				operation = operation | ImGuizmo::TRANSLATE;
			}
			if (rotate)
			{
				operation = operation | ImGuizmo::ROTATE;
			}
			if (scale)
			{
				operation = operation | ImGuizmo::SCALEU;
			}

			if (operation == 0)
			{
				return false;
			}

			const auto playerCamera = RE::PlayerCamera::GetSingleton();
			auto niCamera = static_cast<RE::NiCamera*>(playerCamera->cameraRoot->children[0].get());

			float view[4][4], projection[4][4];
			//SNiTransformEditor::FillImGuizmoViewMatrix(*niCamera, view);
			//SNiTransformEditor::FillImGuizmoProjectionMatrix(*niCamera, projection);
			SNiTransformEditor::Transpose(niCamera->worldToCam, view);
			SNiTransformEditor::IdentityMatrix(projection);

			ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
			const ImGuiIO& io = ImGui::GetIO();
			ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

			bool wasEdited = false;
			if (ImGuizmo::Manipulate(reinterpret_cast<float*>(view),
					reinterpret_cast<float*>(projection), static_cast<ImGuizmo::OPERATION>(operation),
					isLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
					reinterpret_cast<float*>(model)))
			{
				wasEdited = true;
			}
			return wasEdited;
		}
	}

	bool NiPoint3Editor(const char* label, RE::NiPoint3& vector)
	{
		return ImGui::DragFloat3(label, &vector.x, 5.f);
	}

	bool NiTransformEditor(const char* label, RE::NiTransform& transform,
		const RE::NiTransform& parentTransform)
	{
		bool wasEdited = false;

		if (PushingCollapsingHeader(label))
		{
			RE::NiTransform worldTransform = parentTransform * transform;
			float model[4][4];
			SNiTransformEditor::FillImGuizmoModelMatrix(worldTransform, model);
			if (SNiTransformEditor::GizmoEditor(model))
			{
				SNiTransformEditor::FillTransformFromImGuizmoModelMatrix(worldTransform, model);
				transform = parentTransform.Invert() * worldTransform;
				wasEdited = true;
			}
			if (NiPoint3Editor("Translation", transform.translate))
			{
				wasEdited = true;
			}
			RE::NiPoint3 euler;
			transform.rotate.ToEulerAnglesXYZ(euler);
			if (NiPoint3Editor("Rotation", euler))
			{
				transform.rotate.SetEulerAnglesXYZ(euler);
				wasEdited = true;
			}
			if (ImGui::DragFloat("Scale", &transform.scale, 0.01f, 0.f))
			{
				wasEdited = true;
			}

			ImGui::TreePop();
		}
		return wasEdited;
	}

	bool ReferenceTransformEditor(const char* label, RE::TESObjectREFR& ref) 
	{
		bool wasEdited = false;

		if (PushingCollapsingHeader(label))
		{
			constexpr float rad2Deg = 180.f / 3.141592f;
			constexpr float deg2Rad = 3.141592f / 180.f;

			float model[4][4];
			RE::NiPoint3 location = ref.data.location;
			RE::NiPoint3 angle = ref.data.angle;
			RE::NiPoint3 angleDeg = angle * rad2Deg;
			RE::NiPoint3 scale = { ref.refScale / 100.f, ref.refScale / 100.f,
				ref.refScale / 100.f };
			ImGuizmo::RecomposeMatrixFromComponents(&location.x, &angleDeg.x, &scale.x,
				reinterpret_cast<float*>(model));
			if (SNiTransformEditor::GizmoEditor(model))
			{
				ImGuizmo::DecomposeMatrixToComponents(reinterpret_cast<float*>(model), &location.x,
					&angleDeg.x, &scale.x);
				angle = angleDeg * deg2Rad;
				wasEdited = true;
			}
			if (NiPoint3Editor("Translation", location))
			{
				wasEdited = true;
			}
			if (NiPoint3Editor("Rotation", angle))
			{
				wasEdited = true;
			}
			if (ImGui::DragFloat("Scale", &scale.x, 0.01f, 0.01f))
			{
				wasEdited = true;
			}

			if (wasEdited)
			{
				ref.formFlags |= RE::TESForm::RecordFlags::kDisabled;
				ref.MoveTo_Impl(RE::ObjectRefHandle(), ref.GetParentCell(), ref.GetWorldspace(),
					location, angle);
				ref.formFlags &= ~RE::TESForm::RecordFlags::kDisabled;
				ref.SetScale(scale.x);
			}

			ImGui::TreePop();
		}
		return wasEdited;
	}
}
