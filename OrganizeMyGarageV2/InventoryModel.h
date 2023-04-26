#pragma once
#include "ItemStructs.h"
#include "OmgView.h"

enum class ItemSlots {
	Body = 0,
	Decal = 1,
	Wheels = 2,
	Boost = 3,
	Antenna = 4,
	Topper = 5,
	Paint = 6,
	PaintAlt = 12,
	EngineSound = 13,
	Trail = 14,
	GoalExplosion = 15
};
	

class InventoryModel
{
public:
	explicit InventoryModel(std::shared_ptr<GameWrapper> gw);
	void InitSlotIcons();

	OnlineProdData GetProdData(OnlineProductWrapper& onlineProd);
	OnlineProdData GetProdData(const ProductInstanceID& instanceId);
	void InitProducts();
	const std::set<int>& GetForcedSlotForBody(int bodyId);
	std::shared_ptr<ImageWrapper> GetSlotIcon(int slotIndex);
	const std::vector<OnlineProdData>& GetSlotProducts(int slotIndex);

private:
	std::shared_ptr<GameWrapper> m_gw;
	std::map<int, std::vector<OnlineProdData>> m_products; //key is slot
	std::map<int, std::set<int>> m_forcedProductSlotCache;
	std::map<int, std::shared_ptr<ImageWrapper>> m_slotIcons;

	SpecialEditionDatabaseWrapper specialEditionDb = {0};
	PaintDatabaseWrapper paintDatabase= {0};

};
