#include "pch.h"
#include "OmgView.h"

#include "imgui/imgui_internal.h"
#include "ImguiUtils.h"
#include "InventoryModel.h"

size_t findCaseInsensitive(std::string data, std::string toSearch, size_t pos = 0)
{
	// Convert complete given String to lower case
	std::ranges::transform(data, data.begin(), ::tolower);
	// Convert complete given Sub String to lower case
	std::ranges::transform(toSearch, toSearch.begin(), ::tolower);
	// Find sub string in given string
	return data.find(toSearch, pos);
}


OmgView::OmgView(std::shared_ptr<GarageModel> vm, std::shared_ptr<InventoryModel> inventory, std::shared_ptr<GameWrapper> gw):
	m_vm(std::move(vm)),
	m_inventory(std::move(inventory)),
	m_gw(std::move(gw))
{
}

void OmgView::Render()
{
	ImGui::Columns(2);

	bool randomGoalExplosion = m_vm->GetRandomGoalExplosionEnabled();
	if (ImGui::Checkbox("Random goal explosion each game", &randomGoalExplosion)) {
		m_vm->SetRandomGoalExplosionEnabled(randomGoalExplosion);
	}
	ImGui::SameLine();
	HelpMarker("WARNING: It will modify active preset.");

	bool favoritesEnabled = m_vm->GetFavoritesEnabled();
	if (ImGui::Checkbox("Cycle through favorite presets", &favoritesEnabled)) {
		m_vm->SetFavoritesEnabled(favoritesEnabled);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Toggle cycling through favorite presets");
	}
	
	bool favoritesShuffle = m_vm->GetFavoritesShuffle();
	if (ImGui::Checkbox("Shuffle favorite presets", &favoritesShuffle)) {
		m_vm->SetFavoritesShuffle(favoritesShuffle);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Shuffle presets instead of going through them sequentially.");
	}

	bool favoritesNotify = m_vm->GetFavoritesNotify();
	if (ImGui::Checkbox("Notify on preset change", &favoritesNotify)) {
		m_vm->SetFavoritesNotify(favoritesNotify);
	}
	ImGui::SameLine();
	HelpMarker("Creates a toast notification on preset change.");

	ImGui::Text("Your Presets");
	ImGui::SameLine();
	HelpMarker("Drag to reorder. Double-click to activate");

	auto g = ImGui::GetCurrentContext();
	auto& style = g->Style;
	auto framePadding = style.FramePadding;
	float lineHeight = ImGui::GetTextLineHeight();
	m_buttonHight = lineHeight + framePadding.y * 2 + 5;

	ImGui::BeginChild("the preset table", ImVec2(-1, -m_buttonHight), true);

	for (size_t i = 0; i < m_vm->GetPresets().size(); i++)
	{
		RenderPresetListItem(i);
	}

	ImGui::EndChild();

	if (ImGui::Button("Add"))
	{
		OnGameThread([this](...)
		{
			static_cast<void>(m_vm->AddPreset());
			RefreshVm();
		});
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
	{
		OnGameThread([this](...)
		{
			m_inventory->InitProducts();
			m_vm->RefreshPresets();
			m_vm->RefreshEquippedIndex();
		});
	}

	ImGui::NextColumn();

	ImGui::BeginChild("the presets details");

	RenderPresetDetails(m_selectedIndex);

	ImGui::EndChild();
}

void OmgView::RefreshVm() const
{
	m_vm->RefreshPresets();
	m_vm->RefreshEquippedIndex();
}

void OmgView::OnGameThread(const std::function<void(GameWrapper*)>& theLambda) const
{
	m_gw->Execute(theLambda);
}

void OmgView::RenderPresetListItem(size_t index)
{
	auto scopeId = ImGui::ScopeId{index};
	const auto& presets = m_vm->GetPresets();
	auto& name = presets[index].name;
	const auto indexIsEquipped = m_vm->equippedPresetIndex == index;
	m_selectedIndex = std::clamp(m_selectedIndex, static_cast<size_t>(0), presets.size() - 1);

	auto equippedStyle = ImGui::ScopeStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0, 1), indexIsEquipped);

	const auto label = indexIsEquipped ? std::format("{} (equipped)", name) : name;
	if (ImGui::Selectable(label.c_str(), m_selectedIndex == index, ImGuiSelectableFlags_AllowDoubleClick))
	{
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			OnGameThread([this, name](...)
			{
				m_vm->EquipPreset(name);
			});
		}
		m_selectedIndex = index;
	}
	DragAndDropPreset(index);
}

void OmgView::RenderLoadoutEditor(const std::vector<OnlineProdData>& loadout, size_t presetIndex, int teamIndex)
{
	static std::set slotsToDraw = {0, 1, 2, 3, 4, 5, 7, 12, 13, 14, 15};
	int bodyId = loadout[0].prodId;
	auto lockedSlots = m_inventory->GetForcedSlotForBody(bodyId);
	for (auto& equippedItem : loadout)
	{
		if (!slotsToDraw.contains(equippedItem.slot))
		{
			continue;
		}
		if (auto iconRes = m_inventory->GetSlotIcon(equippedItem.slot))
		{
			ImGui::Image(iconRes->GetImGuiTex(), {32, 32});
			ImGui::SameLine();
		}
		auto slotLockedScope = ImGui::Disable(lockedSlots.contains(equippedItem.slot));

		char inputBuffer[64] = "";
		auto teamScope = ImGui::ScopeId(teamIndex);
		if (ImGui::BeginSearchableCombo(std::format("##LoadoutSlotSelector{}", equippedItem.slot).c_str(), equippedItem.name.c_str(), inputBuffer, 64, "Type to filter"))
		{
			const std::string filterStr(inputBuffer);
			const auto slotProducts = m_inventory->GetSlotProducts(equippedItem.slot);
			for (auto& inventoryProduct : slotProducts)
			{
				const bool selected = equippedItem.instanceId == inventoryProduct.instanceId;
				if (findCaseInsensitive(inventoryProduct.name, filterStr, 0) == std::string::npos) continue;
				if (inventoryProduct.slot != 0 && !inventoryProduct.IsBodyCompatible(loadout[0].prodId)) continue;

				auto lbl = std::format("{}: ##{}{}{}", inventoryProduct.name, inventoryProduct.prodId, inventoryProduct.slot,
				                       inventoryProduct.instanceId.lower_bits);
				if (ImGui::Selectable(lbl.c_str(), selected))
				{
					OnGameThread([this, inventoryProduct, presetIndex, teamIndex](auto gw)
					{
						m_vm->EquipItem(presetIndex, inventoryProduct, teamIndex);
						RefreshVm();
					});
				}

				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndSearchableCombo();
		}
	}
}

void OmgView::RenderPresetDetails(size_t presetIndex)
{
	static int selectedIndexMem = -1;
	static std::string name;
	const PresetData& preset = m_vm->GetPresets()[presetIndex];
	if (presetIndex != selectedIndexMem)
	{
		//Selection changed.
		selectedIndexMem = presetIndex;
		name = preset.name;
	}
	ImGui::BeginChild("LoadoutDetails", {-1, -m_buttonHight}, true);

	ImGui::InputText("Preset Name", &name);
	ImGui::SameLine();
	if (ImGui::Button("Rename"))
	{
		if (name != preset.name)
		{
			OnGameThread([this, presetIndex, name = name](auto gw)
			{
				m_vm->RenamePreset(presetIndex, name);
				RefreshVm();
			});
		}
	}
	
	bool favorite = m_vm->IsFavorite(name);
	if (ImGui::Checkbox("Favorite", &favorite)) {
		m_vm->UpdateFavorite(name, favorite);
	}

	auto draw_color = [](const std::string& lbl, const LinearColor& color)
	{
		ImGui::TextUnformatted(lbl.c_str());
		ImGui::SameLine();
		ImGui::ColorButton(lbl.c_str(), color, ImGuiColorEditFlags_NoTooltip);
	};


	if (ImGui::CollapsingHeader("Blue", ImGuiTreeNodeFlags_DefaultOpen))
	{
		draw_color(std::format("Primary Paint({})", preset.color1.primaryId), preset.color1.primaryColor);
		ImGui::SameLine();
		draw_color(std::format("Accent Paint({})", preset.color1.accentId), preset.color1.accentColor);
		RenderLoadoutEditor(preset.loadout, presetIndex, 0);
	}

	if (ImGui::CollapsingHeader("Orange", ImGuiTreeNodeFlags_DefaultOpen))
	{
		draw_color(std::format("Primary Paint({})", preset.color2.primaryId), preset.color2.primaryColor);
		ImGui::SameLine();
		draw_color(std::format("Accent Paint({})", preset.color2.accentId), preset.color2.accentColor);
		RenderLoadoutEditor(preset.loadout2, presetIndex, 1);
	}


	ImGui::EndChild();

	if (ImGui::Button("Copy"))
	{
		OnGameThread([this, name = preset.name](auto gw)
		{
			m_vm->CopyPreset(name);
			RefreshVm();
		});
	}
	ImGui::SameLine();
 
	if (ImGui::Button("Delete"))
	{
		m_selectedIndex = 0;
		OnGameThread([this, name = preset.name](auto gw)
		{
			m_vm->DeletePreset(name);
			RefreshVm();
		});
	}
	ImGui::SameLine();

	if (ImGui::Button("Equip"))
	{
		OnGameThread([this, name = preset.name](auto gw)
		{
			m_vm->EquipPreset(name);
		});
	}
}


void OmgView::DragAndDropPreset(size_t& index) const
{
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		ImGui::SetDragDropPayload("DND_PresetImg", &index, sizeof(size_t));
		//ImGui::Text("Swap %s", preset.name.c_str());
		if (ImGui::GetIO().KeyShift)
		{
			ImGui::Text("Swap");
		}
		else
		{
			ImGui::Text("Move (Hold shift to swap)");
		}
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_PresetImg"))
		{
			IM_ASSERT(payload->DataSize == sizeof(size_t));
			size_t sourceIndex = *static_cast<size_t*>(payload->Data);
			OnGameThread([this, index, sourceIndex](auto gw)
			{
				if (ImGui::GetIO().KeyShift)
				{
					m_vm->SwapPreset(index, sourceIndex);
				}
				else
				{
					m_vm->MovePreset(index, sourceIndex);
				}

				m_vm->RefreshPresets();
				m_vm->RefreshEquippedIndex();
			});
			//cvarManager->log("Swap: " + std::to_string(index) + " with " + std::to_string(payload_n));
		}
		ImGui::EndDragDropTarget();
	}
}
