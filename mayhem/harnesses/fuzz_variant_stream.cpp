// Binary Variant / VariantType deserializer (ReadStream::Read), used by graphs, assets and scripting.
#include "Engine/Core/Types/Variant.h"
#include "Engine/Serialization/MemoryReadStream.h"
#include <stdint.h>
#include <stddef.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
        return 0;
    MemoryReadStream stream(data + 1, (uint32)(size - 1));
    if (data[0] & 1)
    {
        VariantType type;
        stream.Read(type);
    }
    else
    {
        Variant value;
        stream.Read(value);
        Variant copy(value);
        (void)copy.Type;
    }
    return 0;
}
