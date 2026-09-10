#include "TileInfo.h"
#include "../IO/Log.h"
#include "../Item/ItemInfoManager.h"
#include "WorldInfo.h"
#include "WorldTileManager.h"

TileInfo::TileInfo() : m_pExtraData(nullptr), m_tileData(nullptr), m_damage(0), m_type(0) {}

TileInfo::~TileInfo()
{
    SAFE_DELETE(m_pExtraData)
}

bool TileInfo::Serialize(MemoryBuffer& memBuffer, bool write, bool database, uint16 worldVersion)
{
    // little hacking around
    if (m_tileData->fg == 3760 && write && database)
    {
        int16 newFG = ITEM_ID_BEDROCK;
        memBuffer.ReadWrite(newFG, write);
        memBuffer.ReadWrite(m_tileData->bg, write);
        memBuffer.ReadWrite(m_tileData->parent, write);

        newFG = 0;
        memBuffer.ReadWrite(newFG, write);
        return true;
    }

    if (!database)
    {
        memBuffer.ReadWrite(m_tileData->fg, write);
        memBuffer.ReadWrite(m_tileData->bg, write);
        memBuffer.ReadWrite(m_tileData->parent, write);
        memBuffer.ReadWrite(m_tileData->flags, write);
    }

    if (HasFlag(TILE_FLAG_HAS_PARENT))
    {
        memBuffer.ReadWrite(m_tileData->parent, write); // ye its like that
    }

    if (HasFlag(TILE_FLAG_HAS_EXTRA_DATA))
    {
        if (write)
        {
            if (!m_pExtraData)
            {
                LOGGER_LOG_ERROR("Tile flagged with extra data but extra data is NULL? fg:%d", m_tileData->fg);
                return false;
            }

            m_pExtraData->Serialize(memBuffer, true, database, this, worldVersion);
        }
        else
        {
            ItemInfo* pItem = GetItemInfoManager()->GetItemByID(m_tileData->fg);
            if (!pItem)
                return false;

            uint8 tileExtraType = GetTileExtraType(pItem->type);
            if (tileExtraType != TILE_EXTRA_TYPE_NONE)
            {
                m_pExtraData = CreateTileExtra(tileExtraType);

                if (m_pExtraData)
                {
                    m_pExtraData->Serialize(memBuffer, false, database, this, worldVersion);
                }
            }
            else
            {
                // little hack again
                if (m_tileData->fg == 3760)
                {
                    memBuffer.Seek(memBuffer.GetOffset() + 21);
                    return true;
                }

                LOGGER_LOG_ERROR("Failed tile serialization unknown tile extra for item: %d, type: %d, flags: %d",
                                 m_tileData->fg, pItem->type, m_tileData->flags);
                return false;
            }
        }
    }

    if (database && !write && m_tileData->fg != ITEM_ID_BLANK)
    {
        ItemInfo* pItem = GetItemInfoManager()->GetItemByID(m_tileData->fg);
        if (!pItem)
            return false;

        m_type = pItem->type;
    }

    return true;
}

uint32 TileInfo::GetMemEstimate(bool database, uint16 worldVersion)
{
    MemoryBuffer memSize;
    Serialize(memSize, true, database, worldVersion);

    return memSize.GetOffset();
}

bool TileInfo::IsCollidable()
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(GetDisplayedItem());
    if (!pItem)
        return true;

    if (pItem->collisionType == COLLISION_IF_OFF)
        return !HasFlag(TILE_FLAG_IS_ON);

    if (pItem->collisionType == COLLISION_IF_ON)
        return HasFlag(TILE_FLAG_IS_ON);

    return !(pItem->collisionType == COLLISION_NONE || pItem->collisionType == COLLISION_ONE_WAY);
}

void TileInfo::BindTileData(TempTileData* pTileData)
{
    m_tileData = pTileData;
}

float TileInfo::GetGrowthPercent()
{
    if (!m_pExtraData)
        return 0.0f;

    return m_pExtraData->GetGrowthPercent(this);
}

void TileInfo::FinalizeGrowth(uint32 ageMS)
{
    if (!m_pExtraData)
        return;

    m_pExtraData->FinalizeGrowth(ageMS);
}

void TileInfo::ModGrowth(int32 deltaAgeSec, int32 ageSec)
{
    if (!m_pExtraData)
        return;

    m_pExtraData->ModGrowth(deltaAgeSec, ageSec);
}

uint16 TileInfo::GetDisplayedItem()
{
    return m_tileData->fg != ITEM_ID_BLANK ? m_tileData->fg : m_tileData->bg;
}

bool TileInfo::IsTileExtraType(eTileExtraType type)
{
    if (!m_pExtraData)
        return false;

    return (m_pExtraData->type == type);
}
