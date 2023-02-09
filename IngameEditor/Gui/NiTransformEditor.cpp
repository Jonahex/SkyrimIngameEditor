#include "Gui/NiTransformEditor.h"

#include "Gui/Utils.h"

#include "3rdparty/ImGuizmo/ImGuizmo.h"

#include <RE/N/NiCamera.h>
#include <RE/N/NiTransform.h>
#include <RE/P/PlayerCamera.h>

#include <numbers>

namespace SIE
{
	namespace SNiTransformEditor
	{
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
		}

		static bool GizmoEditor(float model[4][4], bool allowAxisScale) 
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
				operation = operation | (allowAxisScale ? ImGuizmo::SCALEU : ImGuizmo::SCALE_X);
			}

			if (operation == 0)
			{
				return false;
			}

			const auto playerCamera = RE::PlayerCamera::GetSingleton();
			auto niCamera = static_cast<RE::NiCamera*>(playerCamera->cameraRoot->children[0].get());

			ImGuizmo::matrix_t cameraWorld;
			SNiTransformEditor::FillImGuizmoModelMatrix(niCamera->world, cameraWorld.m);
			ImGuizmo::matrix_t inverseCameraWorld;
			inverseCameraWorld.Inverse(cameraWorld);
			ImGuizmo::matrix_t axisSwitch;
			axisSwitch.v.right.Set(0.f, 0.f, -1.f, 0.f);
			axisSwitch.v.up.Set(0.f, 1.f, 0.f, 0.f);
			axisSwitch.v.dir.Set(1.f, 0.f, 0.f, 0.f);
			axisSwitch.v.position.Set(0.f, 0.f, 0.f, 1.f);
			ImGuizmo::matrix_t view = inverseCameraWorld * axisSwitch;
			ImGuizmo::matrix_t inverseView;
			inverseView.Inverse(view);
			ImGuizmo::matrix_t worldToCam =
				*reinterpret_cast<ImGuizmo::matrix_t*>(niCamera->worldToCam);
			worldToCam.Transpose();
			ImGuizmo::matrix_t projection = inverseView * worldToCam;
			
			ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
			const ImGuiIO& io = ImGui::GetIO();
			ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

			bool wasEdited = false;
			if (ImGuizmo::Manipulate(view.m16,
					projection.m16, static_cast<ImGuizmo::OPERATION>(operation),
					isLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
					reinterpret_cast<float*>(model)))
			{
				wasEdited = true;
			}
			return wasEdited;
		}
	}

	bool NiPoint3Editor(const char* label, RE::NiPoint3& vector, float speed)
	{
		return ImGui::DragFloat3(label, &vector.x, speed);
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
			if (SNiTransformEditor::GizmoEditor(model, true))
			{
				SNiTransformEditor::FillTransformFromImGuizmoModelMatrix(worldTransform, model);
				transform = parentTransform.Invert() * worldTransform;
				wasEdited = true;
			}
			if (NiPoint3Editor("Translation", transform.translate, 5.f))
			{
				wasEdited = true;
			}
			RE::NiPoint3 euler;
			transform.rotate.ToEulerAnglesXYZ(euler);
			if (NiPoint3Editor("Rotation", euler, 0.1f))
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
			constexpr float rad2Deg = 180.f / std::numbers::pi;
			constexpr float deg2Rad = std::numbers::pi / 180.f;

			float model[4][4];
			RE::NiPoint3 location = ref.data.location;
			RE::NiPoint3 angle = ref.data.angle;
			RE::NiPoint3 angleDeg = angle * rad2Deg;
			RE::NiPoint3 scale = { ref.refScale / 100.f, ref.refScale / 100.f,
				ref.refScale / 100.f };
			ImGuizmo::RecomposeMatrixFromComponents(&location.x, &angleDeg.x, &scale.x,
				reinterpret_cast<float*>(model));
			if (SNiTransformEditor::GizmoEditor(model, false))
			{
				ImGuizmo::DecomposeMatrixToComponents(reinterpret_cast<float*>(model), &location.x,
					&angleDeg.x, &scale.x);
				angle = angleDeg * deg2Rad;
				wasEdited = true;
			}
			if (NiPoint3Editor("Translation", location, 5.f))
			{
				wasEdited = true;
			}
			if (NiPoint3Editor("Rotation", angle, 0.1f))
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
