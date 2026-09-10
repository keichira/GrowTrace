#pragma once

#include "../Math/Rect.h"
#include "../Memory/MemoryBuffer.h"
#include "../Precompiled.h"
#include "TileInfo.h"

class WorldInfo;

class WorldTileManager
{
public:
    WorldTileManager(WorldInfo* pParent);
    ~WorldTileManager();

public:
    bool Serialize(MemoryBuffer& memBuffer, bool write, bool database, WorldInfo* pWorld, float gameVersion = 0.0f);
    uint32 GetMemEstimate(bool database, WorldInfo* pWorld, float gameVersion);

    Vector2Int& GetSize() { return m_size; }
    void SetSize(const Vector2Int& size) { m_size = size; }

    TileInfo* GetTile(const Vector2Int& pos);
    TileInfo* GetTile(int32 x, int32 y);
    TileInfo* GetTile(int32 index);
    TileInfo* GetTileByWorldPos(float x, float y);
    TileInfo* GetTileByWorldPos(const Vector2Float& pos);

    bool IsTileLockedWithLock(TileInfo* pTile);
    bool IsTileLockedWithLockButPublic(TileInfo* pTile);
    bool IsPlayerOwnerOfTheTile(TileInfo* pTile, int32 userID);

    TileInfo* GetTileInfoFlaggedWith(eItemFlag flag, uint32 skipItemID = 0);
    TileInfo* GetTileInfoByItemID(int32 itemID, int32 startIndex = 0);

    int32 GetTileIndex(TileInfo* pTile);
    TileInfo* GetTileByTypeFromRect(const RectFloat& rect, int32 itemType);

private:
    Vector2Int m_size;
    std::vector<TileInfo> m_tiles;
    std::vector<TempTileData> m_tempTiles;

    WorldInfo* m_pWorld;
};