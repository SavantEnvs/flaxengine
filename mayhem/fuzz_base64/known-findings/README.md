`Encryption::Base64Decode` is NOT fuzzed on adversarial input by the `fuzz-base64` target. The target only
covers round trips (Base64 encode -> decode -> re-encode, and EncryptBytes/DecryptBytes) on well-formed
padded base64 built from the fuzz bytes. A green run with 0 defects says nothing about the decoder's
robustness on arbitrary strings.

Two known out-of-bounds bugs in `Encryption::Base64Decode` (Source/Engine/Utilities/Encryption.cpp) are
therefore unreachable by the harness. They have NOT been reported upstream. The decoder has no validation and
no error return, and the engine calls it on scene data (Serialization.cpp, Foliage.cpp, StaticModel.cpp).
Each `input` is a string that, passed verbatim to `Base64Decode`, crashes it (ASan):

- `negative-char-index/input`: a byte >= 0x80 indexes `B64index` with a negative `char` (OOB read).
- `length-mod4-overread/input`: length % 4 of 1 or 2 reads `encoded[last + 1 | last + 2]` past the buffer.

The `input` files carry the old harness's leading mode byte (odd => decode the rest verbatim); strip it
to get the string.

They are kept out of `testsuite/`, which Mayhem replays on every run. A person decided to keep the target as
round-trip-only (runs 2 and 3 of the raw-decoder harness ended "Run Failed" with 41 stored crashers; run 3's
regression_testing phase failed with sanity event 393863, "libFuzzer target failed to fuzz for 5 iterations").
