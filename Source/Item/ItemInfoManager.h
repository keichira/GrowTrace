#pragma once

#include "../Math/Math.h"
#include "../Precompiled.h"
#include "ItemInfo.h"

#define MAX_SUPPORTED_ITEM_DATA_VERSION 26

class ItemInfoManager
{
public:
    ItemInfoManager();
    ~ItemInfoManager();

    static ItemInfoManager* GetInstance()
    {
        static ItemInfoManager instance;
        return &instance;
    }

    bool LoadRaw(const uint8* pData, uint32 size);

    ItemInfo* GetItemByID(int32 itemID);
    ItemInfo* GetItemByName(const string& name);
    uint32 GetItemCount() const { return m_itemCount; }
    uint16 GetVersion() const { return m_version; }

    uint32 GetHash() const { return m_hash; }
    void SetHash(uint32 hash) { m_hash = hash; }

private:
    uint16 m_version;
    uint32 m_itemCount;

    uint32 m_hash;

    std::vector<ItemInfo> m_items;
};

ItemInfoManager* GetItemInfoManager();