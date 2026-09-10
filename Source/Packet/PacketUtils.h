#pragma once

#include "../Precompiled.h"
#include "../Utils/StringUtils.h"

struct TextPacketField
{
    uint32 hash = 0;
    const char* key;
    uint16 keySize = 0;
    const char* value;
    uint16 valueSize = 0;

    std::string_view GetKeyStringView() const { return std::string_view(key, keySize); }

    std::string_view GetStringView() const { return std::string_view(value, valueSize); }

    string GetString() { return string(value, valueSize); }

    eToIntResult GetUInt(uint32& out) { return ToUInt(value, valueSize, out); }

    eToIntResult GetInt(int32& out) { return ToInt(value, valueSize, out); }

    eToIntResult GetBool(bool& out)
    {
        int32 tempVal = 0;

        eToIntResult res = ToInt(value, valueSize, tempVal);
        if (res != TO_INT_SUCCESS)
            return res;

        out = (tempVal != 0);
        return TO_INT_SUCCESS;
    }
};

template <uint8 N> struct ParsedTextPacket
{
    TextPacketField fields[N];
    uint8 count;

    TextPacketField* Find(uint32 hash)
    {
        for (uint8 i = 0; i < count; ++i)
        {
            if (fields[i].hash == hash)
            {
                return &fields[i];
            }
        }

        return nullptr;
    }
};

template <uint8 N> inline void ParseTextPacket(const char* data, uint32 size, ParsedTextPacket<N>& out)
{
    out.count = 0;
    if (!data || size == 0)
        return;

    const char* p = data;
    const char* end = data + size;

    while (p < end && out.count < N)
    {
        while (p < end && (*p == '\n' || *p == '\r'))
            ++p;

        if (p >= end)
            break;

        const char* nextNL = (const char*)(std::memchr(p, '\n', end - p));
        const char* lineEnd = nextNL ? nextNL : end;
        const char* lineStart = p;
        p = nextNL ? nextNL + 1 : end;

        // umm, lets keep it whatever...
        if (lineEnd > lineStart && *(lineEnd - 1) == '\r')
            --lineEnd;

        if (lineStart < lineEnd && *lineStart == '|')
            ++lineStart;

        if (lineEnd > lineStart && *(lineEnd - 1) == '|')
            --lineEnd;

        // force check 1 |
        const char* pipe = (const char*)(std::memchr(lineStart, '|', lineEnd - lineStart));
        if (!pipe)
            continue;

        const char* keyStart = lineStart;
        const char* keyEnd = pipe;
        const char* valStart = pipe + 1;
        const char* valEnd = lineEnd;

        // ||item|4, item||5
        const char* secondPipe = (const char*)(std::memchr(pipe + 1, '|', lineEnd - (pipe + 1)));
        if (secondPipe)
        {
            const char* secondKeyStart = secondPipe;
            while (secondKeyStart > valStart)
            {
                char c = *(secondKeyStart - 1);
                if (IsAlpha(c))
                {
                    --secondKeyStart;
                }
                else
                {
                    break;
                }
            }

            if (secondKeyStart > valStart && secondKeyStart < secondPipe)
            {
                valEnd = secondKeyStart;

                if (keyStart < keyEnd && valStart <= valEnd && out.count < N)
                {
                    uint32 hash = HashString(keyStart, (uint32)(keyEnd - keyStart));
                    out.fields[out.count++] = {hash, keyStart, (uint16)(keyEnd - keyStart), valStart,
                                               (uint16)(valEnd - valStart)};
                }

                keyStart = secondKeyStart;
                keyEnd = secondPipe;
                valStart = secondPipe + 1;
                valEnd = lineEnd;
            }
            else
            {
                continue;
            }
        }

        if (valEnd > valStart && *(valEnd - 1) == '\0')
            --valEnd;

        if (keyStart >= keyEnd || valStart > valEnd)
            continue;

        uint32 hash = HashString(keyStart, (uint32)(keyEnd - keyStart));

        out.fields[out.count++] = {hash, keyStart, (uint16)(keyEnd - keyStart), valStart, (uint16)(valEnd - valStart)};
    }
}

template <uint8 N> class TextPacketEditor
{
public:
    void SetField(uint32 keyHash, std::string_view newValue) { m_overrides[keyHash] = newValue; }
    void AddNewField(std::string_view key, std::string_view value) { m_newFields.push_back({key, value}); }

    bool HasModifications() const { return !m_overrides.empty() || !m_newFields.empty(); }

    string Get(const ParsedTextPacket<N>& parsed) const
    {
        string payload;
        payload.reserve(256);

        for (uint8 i = 0; i < parsed.count; ++i)
        {
            const auto& field = parsed.fields[i];

            payload.append(field.GetKeyStringView());
            payload.push_back('|');

            auto it = m_overrides.find(field.hash);
            if (it != m_overrides.end())
            {
                payload.append(it->second);
            }
            else
            {
                payload.append(field.GetStringView());
            }

            payload.push_back('\n');
        }

        for (auto& [key, val] : m_newFields)
        {
            payload.append(key);
            payload.push_back('|');
            payload.append(val);
            payload.push_back('\n');
        }

        return payload;
    }

private:
    std::unordered_map<uint32, std::string_view> m_overrides;
    std::vector<std::pair<std::string_view, std::string_view>> m_newFields;
};