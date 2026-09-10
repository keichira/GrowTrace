#include "WorldTileManager.h"
#include "../Item/ItemInfoManager.h"
#include "../Math/Math.h"
#include "../Math/Random.h"
#include "../Utils/StringUtils.h"
#include "WorldInfo.h"

static std::vector<TempTileData> tileMapEncodeBuffer;

WorldTileManager::WorldTileManager(WorldInfo* pParent) : m_pWorld(pParent) {}

WorldTileManager::~WorldTileManager() {}

bool WorldTileManager::Serialize(MemoryBuffer& memBuffer, bool write, bool database, WorldInfo* pWorld,
                                 float gameVersion)
{
    memBuffer.ReadWrite(m_size, write);

    uint32 totalTiles = m_tiles.size();
    memBuffer.ReadWrite(totalTiles, write);

    if (totalTiles != (m_size.x * m_size.y) || totalTiles > (255 * 255))
        return false;

    if (!write)
    {
        m_tiles.resize(totalTiles);
        m_tempTiles.resize(totalTiles);
    }

    if (!database && gameVersion >= 4.31f)
    {
        uint32 val1 = 0;
        uint8 val2 = 0;
        memBuffer.ReadWrite(val1, write);
        memBuffer.ReadWrite(val2, write);
    }

    if (write)
    {
        if (database)
        {
            memBuffer.WriteRaw(m_tempTiles.data(), sizeof(TempTileData) * m_tempTiles.size());

            uint32 extraCount = 0;
            uint32 checkpoint = memBuffer.GetOffset();
            memBuffer.Write(extraCount);

            for (uint16 i = 0; i < m_tiles.size(); ++i)
            {
                TileInfo* pTile = &m_tiles[i];

                if (!pTile->HasFlag(TILE_FLAG_HAS_EXTRA_DATA) && !pTile->HasFlag(TILE_FLAG_HAS_PARENT))
                {
                    continue;
                }

                extraCount++;
                memBuffer.Write(i);
                if (!pTile->Serialize(memBuffer, true, true, pWorld->GetWorldVersion()))
                    return false;
            }

            uint32 end = memBuffer.GetOffset();

            memBuffer.Seek(checkpoint);
            memBuffer.Write(extraCount);

            memBuffer.Seek(end);
        }
        else
        {
            const TempTileData* pSourceData = m_tempTiles.data();
            uint32 batchStartIdx = 0;

            for (uint32 i = 0; i < m_tiles.size(); ++i)
            {
                TileInfo* pTile = &m_tiles[i];

                if (!pTile->HasFlag(TILE_FLAG_HAS_EXTRA_DATA) && !pTile->HasFlag(TILE_FLAG_HAS_PARENT))
                    continue;

                uint32 batchCount = i - batchStartIdx;
                if (batchCount > 0)
                {
                    memBuffer.WriteRaw(pSourceData + batchStartIdx, sizeof(TempTileData) * batchCount);
                }

                pTile->Serialize(memBuffer, true, false, pWorld->GetWorldVersion());
                batchStartIdx = i + 1;
            }

            uint32 remaining = m_tempTiles.size() - batchStartIdx;
            if (remaining > 0)
            {
                memBuffer.WriteRaw(pSourceData + batchStartIdx, sizeof(TempTileData) * remaining);
            }
        }
    }
    else
    {
        if (!database)
        {
            for (uint32 i = 0; i < m_tiles.size(); ++i)
            { // unused but yeah...
                TileInfo* pTile = &m_tiles[i];

                pTile->BindTileData(&m_tempTiles[i]);

                if (!pTile->Serialize(memBuffer, false, false, m_pWorld->GetWorldVersion()))
                    return false;

                pTile->SetMapIndex(i);
                pTile->SetPos(i % m_size.x, i / m_size.x);
            }
        }
    }

    return true;
}

uint32 WorldTileManager::GetMemEstimate(bool database, WorldInfo* pWorld, float gameVersion)
{
    MemoryBuffer memSize;

    Serialize(memSize, true, database, pWorld, gameVersion);
    return memSize.GetOffset();
}

TileInfo* WorldTileManager::GetTile(const Vector2Int& pos)
{
    return GetTile(pos.x, pos.y);
}

TileInfo* WorldTileManager::GetTile(int32 x, int32 y)
{
    if (x < 0 || y < 0 || x >= m_size.x || y >= m_size.y)
    {
        return nullptr;
    }

    return &m_tiles[y * m_size.x + x];
}

TileInfo* WorldTileManager::GetTile(int32 index)
{
    if (index < 0 || index > (m_size.x * m_size.y))
    {
        return nullptr;
    }

    if (index >= m_tiles.size())
        return nullptr;

    return &m_tiles[index];
}

TileInfo* WorldTileManager::GetTileByWorldPos(float x, float y)
{
    if (x <= 0 || y <= 0)
        return nullptr;

    return GetTile((int32)(x / 32.0f), (int32)(y / 32.0f));
}

TileInfo* WorldTileManager::GetTileByWorldPos(const Vector2Float& pos)
{
    return GetTileByWorldPos(pos.x, pos.y);
}

TileInfo* WorldTileManager::GetTileInfoFlaggedWith(eItemFlag flag, uint32 skipItemID)
{
    ItemInfoManager* pItemMgr = GetItemInfoManager();

    for (auto& tile : m_tiles)
    {
        uint16 fgItemID = tile.GetFG();
        if (fgItemID == ITEM_ID_BLANK || fgItemID == skipItemID)
            continue;

        ItemInfo* pItem = pItemMgr->GetItemByID(fgItemID);
        if (!pItem)
            continue;

        if (pItem->HasFlag(flag))
            return &tile;
    }

    return nullptr;
}

TileInfo* WorldTileManager::GetTileInfoByItemID(int32 itemID, int32 startIndex)
{
    if (startIndex >= m_tiles.size())
        return nullptr;

    for (int32 i = startIndex; i < (m_size.x * m_size.y); ++i)
    {
        if (m_tiles[i].GetFG() == itemID)
            return &m_tiles[i];
    }

    return nullptr;
}

int32 WorldTileManager::GetTileIndex(TileInfo* pTile)
{
    if (!pTile)
        return -1;

    Vector2Int vTilePos = pTile->GetPos();
    if (vTilePos.x < 0 || vTilePos.y < 0 || vTilePos.x >= m_size.x || vTilePos.y >= m_size.y)
    {
        return -1;
    }

    return vTilePos.y * m_size.x + vTilePos.x;
}

TileInfo* WorldTileManager::GetTileByTypeFromRect(const RectFloat& rect, int32 itemType)
{
    int32 xStart = Max(0, Min(rect.left / 32, rect.right / 32));
    int32 xEnd = Min(m_size.x, Max(rect.left / 32, rect.right / 32));
    int32 yStart = Max(0, Min(rect.top / 32, rect.bottom / 32));
    int32 yEnd = Min(m_size.y, Max(rect.top / 32, rect.bottom / 32));

    // todo add type to TileInfo instead of this
    ItemInfoManager* pItemMgr = GetItemInfoManager();

    for (int y = yStart; y < yEnd; ++y)
    {
        for (int x = xStart; x < xEnd; ++x)
        {
            TileInfo* pTile = &m_tiles[y * m_size.x + x];

            ItemInfo* pItem = pItemMgr->GetItemByID(pTile->GetDisplayedItem());
            if (!pItem)
                continue;

            if (pItem->type == itemType)
                return pTile;
        }
    }

    return nullptr;
}