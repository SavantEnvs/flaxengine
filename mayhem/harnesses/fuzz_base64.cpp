// Encryption helpers: Base64 round trips and the engine's byte scrambler.
//
// Scope cut, not a library precondition: Encryption::Base64Decode is NOT fuzzed on adversarial input.
// It has no error path, and on a string with a byte >= 0x80 or a length % 4 of 1 or 2 it reads out of
// bounds (known-findings/). Raw-byte decoding crashes on almost every input; runs 2 and 3 of this
// target (same raw-decoder harness as run 1) started with 41 stored regression crashers and ended
// "Run Failed", and in run 3 the regression_testing phase failed with sanity event 393863 ("libFuzzer
// target failed to fuzz for 5 iterations"). So fuzz bytes are built into well-formed padded base64.
#include "Engine/Utilities/Encryption.h"
#include "Engine/Core/Collections/Array.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Total mapping from fuzz bytes to a padded RFC 4648 string (data[0] bits 1-2 choose 0, 1 or 2 pads).
static void BuildBase64(uint8_t mode, const uint8_t* s, size_t n, Array<char>& out)
{
    size_t len = n / 4 * 4;
    if (len < 4)
        len = 4;
    out.Resize((int32)len);
    for (size_t i = 0; i < len; i++)
        out.Get()[i] = i < n ? kAlphabet[s[i] & 63] : 'A';
    const unsigned pads = ((mode >> 1) & 3) > 2 ? 2 : ((mode >> 1) & 3);
    for (unsigned p = 0; p < pads; p++)
        out.Get()[len - 1 - p] = '=';
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
        return 0;
    const uint8_t* body = data + 1;
    const size_t n = size - 1;
    if (data[0] & 1)
    {
        Array<char> enc;
        BuildBase64(data[0], body, n, enc);
        Array<byte> decoded;
        Encryption::Base64Decode(enc.Get(), enc.Count(), decoded);
        Array<char> again;
        Encryption::Base64Encode(decoded.Get(), decoded.Count(), again);
        Array<byte> decoded2;
        Encryption::Base64Decode(again.Get(), again.Count(), decoded2);
        if (decoded.Count() != decoded2.Count() || memcmp(decoded.Get(), decoded2.Get(), decoded.Count()) != 0)
            __builtin_trap();
    }
    else if (n > 0)  // an empty Array has a null Get(); memcpy/memcmp on it is UB in the harness itself
    {
        Array<char> encoded;
        Encryption::Base64Encode(body, (int32)n, encoded);
        Array<byte> decoded;
        Encryption::Base64Decode(encoded.Get(), encoded.Count(), decoded);
        if ((size_t)decoded.Count() != n || memcmp(decoded.Get(), body, n) != 0)
            __builtin_trap();

        Array<byte> scrambled;
        scrambled.Resize((int32)n);
        memcpy(scrambled.Get(), body, n);
        Encryption::EncryptBytes(scrambled.Get(), n);
        Encryption::DecryptBytes(scrambled.Get(), n);
        if (memcmp(scrambled.Get(), body, n) != 0)
            __builtin_trap();
    }
    return 0;
}
