#pragma once

#include "../Math/Vector2.h"
#include "../Memory/MemoryBuffer.h"
#include "../Precompiled.h"
#include "WorldObjectManager.h"
#include "WorldTileManager.h"

bool IsValidWorldName(const string& worldName, bool allowColon = false);

class WorldInfo
{
public:
    WorldInfo();
    ~WorldInfo();

public:
    void Kill();

    bool Serialize(MemoryBuffer& memBuffer, bool write, bool database, float gameVersion = 0.0f);
    uint32 GetMemEstimate(bool database, float gameVersion = 0.0f);

    void SetName(const string& worldName) { m_name = worldName; }
    const string& GetWorlName() const { return m_name; }

    void SetCurrentWeather(uint32 newWeather) { m_currentWeather = newWeather; }
    uint32 GetCurrentWeather() const { return m_currentWeather; }

    uint32 GetDefaultWeather() const { return m_defaultWeather; }

    uint16 GetWorldVersion() const { return m_version; }

    WorldTileManager* GetTileManager() { return m_pTileMgr; };
    WorldObjectManager* GetObjectManager() { return m_pObjMgr; }

private:
    uint16 m_version;
    uint32 m_flags;
    string m_name;

    WorldTileManager* m_pTileMgr;
    WorldObjectManager* m_pObjMgr;

    uint16 m_defaultWeather;
    uint16 m_currentWeather;
};