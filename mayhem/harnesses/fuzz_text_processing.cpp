// Text parsing helpers: TextProcessing tokenizer, StringUtils numeric parsers, Guid::Parse, Version::Parse.
#include "Engine/Utilities/TextProcessing.h"
#include "Engine/Core/Types/Guid.h"
#include "Engine/Core/Types/Version.h"
#include "Engine/Core/Types/String.h"
#include "Engine/Core/Types/StringView.h"
#include "Engine/Platform/StringUtils.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
        return 0;
    // The engine parses NUL-terminated buffers (TextProcessing/ParseHex peek one char past the length).
    char* terminated = (char*)malloc(size + 1);
    memcpy(terminated, data, size);
    terminated[size] = 0;
    const char* text = terminated;
    const int32 length = (int32)size;
    switch (data[0] % 5)
    {
    case 0:
    {
        TextProcessing reader(text + 1, length - 1);
        if (data[0] & 0x20)
            reader.Setup_HLSL();
        TextProcessing::Token token;
        int32 guard = 0;
        while (reader.CanRead() && guard++ < 100000)
        {
            if (data[0] & 0x10)
                reader.ReadLine(&token);
            else
                reader.ReadToken(&token);
            reader.EatWhiteSpaces();
        }
        break;
    }
    case 1:
    {
        int32 i32; int64 i64; uint32 u32; uint64 u64; float f; uint32 hex;
        StringUtils::Parse(text + 1, (uint32)(length - 1), &i32);
        StringUtils::Parse(text + 1, length - 1, &i64);
        StringUtils::Parse(text + 1, (uint32)(length - 1), &u32);
        StringUtils::Parse(text + 1, length - 1, &u64);
        StringUtils::ParseHex(text + 1, length - 1, &hex);
        StringUtils::Parse(text + 1, &f);
        break;
    }
    case 2:
    {
        Guid guid;
        if (!Guid::Parse(StringAnsiView(text + 1, length - 1), guid))
        {
            Char buf[40];  // FormatType::D/B/P are unimplemented upstream (MissingCode)
            guid.ToString(buf, Guid::FormatType::N);
        }
        break;
    }
    case 3:
    {
        Version v;
        Version::Parse(String(text + 1, length - 1), &v);
        break;
    }
    default:
    {
        int32 outLen = 0;
        Char* wide = StringUtils::ConvertUTF82UTF16(text + 1, length - 1, outLen);
        if (wide)
        {
            int32 back = 0;
            char* utf8 = StringUtils::ConvertUTF162UTF8(wide, outLen, back);
            if (utf8)
                Allocator::Free(utf8);
            Allocator::Free(wide);
        }
        break;
    }
    }
    free(terminated);
    return 0;
}
