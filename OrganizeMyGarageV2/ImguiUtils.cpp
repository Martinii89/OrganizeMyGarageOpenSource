#include "pch.h"
#include "ImguiUtils.h"
#include "IMGUI/imgui_internal.h"

ImGui::Disable::Disable(const bool should_disable_, float alpha): should_disable(should_disable_)
{
	if (should_disable)
	{
		PushItemFlag(ImGuiItemFlags_Disabled, true);
		PushStyleVar(ImGuiStyleVar_Alpha, alpha);
	}
}

ImGui::Disable::~Disable()
{
	if (should_disable)
	{
		PopItemFlag();
		PopStyleVar();
	}
}

ImGui::ScopeStyleColor::ScopeStyleColor(ImGuiCol idx, const ImVec4& col, bool active) : m_active(active)
{
	if (m_active)
	{
		PushStyleColor(idx, col);
	}
}

ImGui::ScopeStyleColor::~ScopeStyleColor()
{
	if (m_active)
	{
		PopStyleColor();
	}
}
