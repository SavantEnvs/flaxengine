// JSON document handling: rapidjson parse with the engine's flags, JsonTools::MergeObjects / ChangeIds / GetGuid.
#include "Engine/Serialization/Json.h"
#include "Engine/Serialization/JsonTools.h"
#include "Engine/Core/Collections/Dictionary.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void CollectGuids(const rapidjson_flax::Value& v, Dictionary<Guid, Guid>& mapping, int depth)
{
    if (depth > 64)
        return;
    if (v.IsString())
    {
        Guid id = JsonTools::GetGuid(v);
        if (id.IsValid())
            mapping[id] = Guid(id.D, id.A, id.B, id.C);
    }
    else if (v.IsObject())
    {
        for (auto i = v.MemberBegin(); i != v.MemberEnd(); ++i)
            CollectGuids(i->value, mapping, depth + 1);
    }
    else if (v.IsArray())
    {
        for (rapidjson::SizeType i = 0; i < v.Size(); i++)
            CollectGuids(v[i], mapping, depth + 1);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    char* text = (char*)malloc(size + 64);
    memcpy(text, data, size);
    // rapidjson's SSE4.2 string scan reads up to 16 bytes past the terminating NUL; pad so that benign over-read stays in-bounds.
    memset(text + size, 0, 64);

    rapidjson_flax::Document doc;
    doc.Parse(text, size);
    if (!doc.HasParseError() && doc.IsObject())
    {
        Dictionary<Guid, Guid> mapping;
        CollectGuids(doc, mapping, 0);
        JsonTools::ChangeIds(doc, mapping);

        // Split the text at its first NUL-free midpoint and merge the second object into the first.
        rapidjson_flax::Document other;
        other.Parse(text + size / 2);
        if (!other.HasParseError() && other.IsObject())
            JsonTools::MergeDocuments(doc, other);
    }
    free(text);
    return 0;
}
