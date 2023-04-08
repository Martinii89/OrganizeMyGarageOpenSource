#pragma once
#include "ItemStructs.h"
#include "OmgView.h"

class InventoryModel
{
public:
	explicit InventoryModel(std::shared_ptr<GameWrapper> gw);
	void InitSlotIcons();

	void InitSlots();
	OnlineProdData GetProdData(OnlineProductWrapper& onlineProd);
	OnlineProdData GetProdData(const ProductInstanceID& instanceId);
	void InitProducts();
	const std::set<int>& GetForcedSlotForBody(int bodyId);
	std::shared_ptr<ImageWrapper> GetSlotIcon(int slotIndex);
	const std::vector<OnlineProdData>& GetSlotProducts(int slot);

private:
	std::shared_ptr<GameWrapper> m_gw;
	std::map<int, std::vector<OnlineProdData>> m_products; //key is slot
	std::map<int, std::set<int>> m_forcedProductSlotCache;
	std::map<int, std::shared_ptr<ImageWrapper>> m_slotIcons;

	SpecialEditionDatabaseWrapper specialEditionDb = {0};
	PaintDatabaseWrapper paintDatabase= {0};

};
