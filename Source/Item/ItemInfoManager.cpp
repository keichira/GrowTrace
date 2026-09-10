#include "ItemInfoManager.h"
#include "../IO/File.h"
#include "../IO/Log.h"
#include "../Math/Math.h"
#include "../Proton/ProtonUtils.h"
#include "../Utils/StringUtils.h"

ItemInfoManager* GetItemInfoManager()
{
    return ItemInfoManager::GetInstance();
}

ItemInfoManager::ItemInfoManager() : m_version(0), m_itemCount(0), m_hash(0) {}

ItemInfoManager::~ItemInfoManager() {}

bool ItemInfoManager::LoadRaw(const uint8* pData, uint32 size)
{
    if (!pData || size == 0)
        return nullptr;

    MemoryBuffer memBuffer(pData, size);
    memBuffer.Read(m_version);

    if (m_version > MAX_SUPPORTED_ITEM_DATA_VERSION)
        return false;

    memBuffer.Read(m_itemCount);

    m_items.resize(m_itemCount);
    for (auto& item : m_items)
    {
        item.Serialize(memBuffer, false, false, m_version);
    }
}

ItemInfo* ItemInfoManager::GetItemByID(int32 itemID)
{
    if (itemID < 0 || itemID >= m_itemCount)
        return nullptr;

    return &m_items[itemID];
}

ItemInfo* ItemInfoManager::GetItemByName(const string& name)
{
    if (name.empty())
        return nullptr;

    string searchName = ToLower(name);
    for (auto& item : m_items)
    {
        if (ToLower(item.name) == searchName)
            return &item;
    }
    return nullptr;
}