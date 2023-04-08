#pragma once
#include "IMGUI/imgui.h"

static void HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

namespace ImGui
{
	struct Disable
	{
		explicit Disable(const bool should_disable_, float alpha = 0.5f);

		Disable(const Disable& other) = delete;
		Disable(Disable&& other) noexcept = delete;
		Disable& operator=(const Disable& other) = delete;
		Disable& operator=(Disable&& other) noexcept = delete;
		~Disable();

		bool should_disable;
	};


	struct ScopeStyleColor
	{
		explicit ScopeStyleColor(ImGuiCol idx, const ImVec4& col, bool active = true);
		~ScopeStyleColor();

		ScopeStyleColor(const ScopeStyleColor& other) = delete;
		ScopeStyleColor(ScopeStyleColor&& other) noexcept = delete;
		ScopeStyleColor& operator=(const ScopeStyleColor& other) = delete;
		ScopeStyleColor& operator=(ScopeStyleColor&& other) noexcept = delete;

	private:
		bool m_active;
	};


	template <typename T>
	struct ScopeId
	{
		ScopeId(const ScopeId& other) = delete;
		ScopeId(ScopeId&& other) noexcept = delete;
		ScopeId& operator=(const ScopeId& other) = delete;
		ScopeId& operator=(ScopeId&& other) noexcept = delete;

		explicit ScopeId(const T& id);
		~ScopeId();
	};

	template <typename T>
	ScopeId<T>::ScopeId(const T& id)
	{
		PushID(id);
	}

	template <>
	inline ScopeId<std::string>::ScopeId(const std::string& id)
	{
		PushID(id.c_str());
	}

	template <typename T>
	ScopeId<T>::~ScopeId()
	{
		PopID();
	}

	//deduction guide to remove warning
	template <typename X>
	ScopeId(X) -> ScopeId<X>;
	//// Will cause a error 
	//ScopeId(std::string) -> ScopeId<const char*>;
}
