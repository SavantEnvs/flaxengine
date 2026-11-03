#!/usr/bin/env bash
#
# mayhem/build.sh — build the fuzz harnesses (sanitized), the selftest and the upstream Catch2 suite (normal flags).
#
# The real Flax Engine build needs Flax.Build + .NET 10 and the full platform/graphics/scripting
# stack. The fuzzed code here is the engine's own platform-independent runtime: core types/math,
# Variant, serialization streams, JSON helpers, text utilities and the Visject graph loader. These
# are compiled straight from Source/Engine, and mayhem/stubs/engine_stubs.cpp replaces only the
# link-time platform layer and the scripting/content registries (which are generated or need the
# whole engine).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

SAN_OBJ="$SRC/obj/mayhem-san"
PLAIN_OBJ="$SRC/obj/mayhem-plain"
rm -rf "$SAN_OBJ" "$PLAIN_OBJ"
mkdir -p "$SAN_OBJ" "$PLAIN_OBJ"

DEFS="-DPLATFORM_LINUX -DPLATFORM_UNIX -D_GNU_SOURCE -D_LINUX64 -DBUILD_DEVELOPMENT \
 -DPLATFORM_SIMD_SSE2=1 -DPLATFORM_SIMD_SSE3=1 -DPLATFORM_SIMD_SSE4_1=1 -DPLATFORM_SIMD_SSE4_2=1 \
 -DCOMPILE_WITHOUT_CSHARP -DLOG_ENABLE=0 -DFLAXENGINE_VERSION_BUILD=6700 -DFLAXENGINE_API= -DCORE_API= -msse4.2"
INCS="-I$SRC/mayhem/stubs -I$SRC/Source -I$SRC/Source/ThirdParty -I$SRC/Source/Engine \
 -I$SRC/Source/ThirdParty/fmt/include"
CXXSTD="-std=c++14 -w"

ENGINE_SRCS=(
  Core/Cache.cpp
  Core/Collections/Sorting.cpp
  Core/Math/BoundingBox.cpp Core/Math/BoundingFrustum.cpp Core/Math/BoundingSphere.cpp
  Core/Math/CollisionsHelper.cpp Core/Math/Color.cpp Core/Math/Color32.cpp Core/Math/Half.cpp
  Core/Math/Math.cpp Core/Math/Matrix.cpp Core/Math/Matrix3x3.cpp Core/Math/OrientedBoundingBox.cpp
  Core/Math/Packed.cpp Core/Math/Plane.cpp Core/Math/Quaternion.cpp Core/Math/Ray.cpp
  Core/Math/Rectangle.cpp Core/Math/Transform.cpp Core/Math/Vector2.cpp Core/Math/Vector3.cpp
  Core/Math/Vector4.cpp Core/Math/Viewport.cpp
  Core/Memory/Allocation.cpp
  Core/Types/DateTime.cpp Core/Types/Guid.cpp Core/Types/String.cpp Core/Types/StringView.cpp
  Core/Types/TimeSpan.cpp Core/Types/Variant.cpp Core/Types/Version.cpp
  Debug/Exception.cpp
  Platform/Base/StringUtilsBase.cpp Platform/Unix/UnixStringUtils.cpp
  Serialization/JsonTools.cpp Serialization/JsonWriter.cpp Serialization/MemoryReadStream.cpp
  Serialization/MemoryWriteStream.cpp Serialization/Serialization.cpp Serialization/Stream.cpp
  Utilities/Crc.cpp Utilities/Encryption.cpp Utilities/Noise.cpp Utilities/TextProcessing.cpp
  Visject/GraphParameter.cpp Visject/VisjectMeta.cpp
)
HARNESSES=(fuzz_visject_graph fuzz_variant_stream fuzz_json_tools fuzz_text_processing fuzz_base64)

compile_all() {
    local objdir="$1"; shift
    { for f in "${ENGINE_SRCS[@]}"; do echo "Source/Engine/$f"; done
      echo Source/ThirdParty/fmt/format.cpp; echo mayhem/stubs/engine_stubs.cpp; } |
    xargs -P "$MAYHEM_JOBS" -I{} bash -c '
        f="$1"; objdir="$2"; shift 2
        o="$objdir/$(echo "$f" | tr / _).o"
        "$@" -c "$f" -o "$o"' _ {} "$objdir" "$CXX" $CXXSTD $DEFS $INCS "$@"
}

# 1) Sanitized engine subset + harnesses
compile_all "$SAN_OBJ" -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link
for h in "${HARNESSES[@]}"; do
    $CXX $CXXSTD $DEFS $INCS -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link \
        -c "mayhem/harnesses/$h.cpp" -o "$SAN_OBJ/harness_$h.o"
done
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$SAN_OBJ/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -x c -c "$STANDALONE_FUZZ_MAIN" -o "$SAN_OBJ/standalone_main.o"

ENGINE_OBJS=("$SAN_OBJ"/Source_*.o "$SAN_OBJ/mayhem_stubs_engine_stubs.cpp.o")
for h in "${HARNESSES[@]}"; do
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE \
        "$SAN_OBJ/harness_$h.o" "${ENGINE_OBJS[@]}" "$SAN_OBJ/lsan_off.o" -o "$SRC/$h"
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS \
        "$SAN_OBJ/harness_$h.o" "${ENGINE_OBJS[@]}" "$SAN_OBJ/lsan_off.o" \
        "$SAN_OBJ/standalone_main.o" -o "$SRC/$h-standalone"
done

# 2) Selftest: the same engine code with normal flags, no sanitizers
compile_all "$PLAIN_OBJ" -O2 $COVERAGE_FLAGS
$CXX $CXXSTD $DEFS $INCS -O2 $COVERAGE_FLAGS -c mayhem/selftest.cpp -o "$PLAIN_OBJ/selftest.o"
$CXX $COVERAGE_FLAGS "$PLAIN_OBJ/selftest.o" "$PLAIN_OBJ"/Source_*.o \
    "$PLAIN_OBJ/mayhem_stubs_engine_stubs.cpp.o" -o "$SRC/flax_selftest"

# 3) Upstream Catch2 suite (Source/Engine/Tests), same normal-flags engine objects. Upstream's TestMain.cpp
#    needs the full Engine, so mayhem/catch_main.cpp supplies the Catch2 main instead.
UPSTREAM_TESTS=(TestGuid TestString TestStringUtils TestCollections TestMath TestTime)
for t in "${UPSTREAM_TESTS[@]}"; do
    echo "Source/Engine/Tests/$t.cpp"
done | xargs -P "$MAYHEM_JOBS" -I{} bash -c '
    f="$1"; objdir="$2"; shift 2
    "$@" -c "$f" -o "$objdir/upstream_$(basename "$f" .cpp).o"' _ {} "$PLAIN_OBJ" $CXX $CXXSTD $DEFS $INCS -O2 $COVERAGE_FLAGS
$CXX $CXXSTD $DEFS $INCS -O2 $COVERAGE_FLAGS -c mayhem/catch_main.cpp -o "$PLAIN_OBJ/catch_main.o"
$CXX $COVERAGE_FLAGS "$PLAIN_OBJ"/upstream_*.o "$PLAIN_OBJ/catch_main.o" "$PLAIN_OBJ"/Source_*.o \
    "$PLAIN_OBJ/mayhem_stubs_engine_stubs.cpp.o" -o "$SRC/flax_upstream_tests"
