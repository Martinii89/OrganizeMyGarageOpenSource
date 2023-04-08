#pragma once
#include "GarageModel.h"

class InventoryModel;

class OmgView
{
public:
	explicit OmgView(std::shared_ptr<GarageModel> vm, std::shared_ptr<InventoryModel> inventory, std::shared_ptr<GameWrapper> gw);

	void Render();

private:
	std::shared_ptr<GarageModel> m_vm;
	std::shared_ptr<InventoryModel> m_inventory;
	std::shared_ptr<GameWrapper> m_gw;
	size_t m_selectedIndex = 0;
	float m_buttonHight = 0;

	void RefreshVm() const;


	void RenderPresetListItem(size_t index);
	void RenderLoadoutEditor(const std::vector<OnlineProdData>& loadout, size_t presetIndex, int teamIndex);
	void RenderPresetDetails(size_t presetIndex);

	void OnGameThread(const std::function<void(GameWrapper*)>& theLambda) const;
	void DragAndDropPreset(size_t& index) const;
};
