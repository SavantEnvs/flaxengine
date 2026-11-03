// Link-time environment for the Mayhem harnesses: the engine's real Core/Math/Types/Serialization/
// Visject/Utilities sources are compiled as-is; this file only supplies the services those sources
// reach for that live in the rest of the engine (platform layer, scripting/content registries, logging).
#include "Engine/Core/Types/BaseTypes.h"
#include "Engine/Core/Types/Guid.h"
#include "Engine/Core/Types/String.h"
#include "Engine/Core/Types/StringView.h"
#include "Engine/Core/Math/Vector2.h"
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Core/Math/Vector4.h"
#include "Engine/Core/Math/Quaternion.h"
#include "Engine/Core/Math/Matrix.h"
#include "Engine/Core/Math/Color.h"
#include "Engine/Core/Math/Transform.h"
#include "Engine/Core/Math/Ray.h"
#include "Engine/Core/Math/Rectangle.h"
#include "Engine/Core/Math/BoundingBox.h"
#include "Engine/Core/Math/BoundingSphere.h"
#include "Engine/Core/Cache.h"
#include "Engine/Core/Object.h"
#include "Engine/Scripting/ScriptingType.h"
#include "Engine/Scripting/ScriptingObject.h"
#include "Engine/Scripting/Scripting.h"
#include "Engine/Content/Asset.h"
#include "Engine/Content/Content.h"
#include "Engine/Level/Prefabs/Prefab.h"
#include "Engine/Platform/Platform.h"
#include "Engine/Platform/Base/FileSystemBase.h"
#include "Engine/Platform/Unix/UnixFile.h"
#include "Engine/Scripting/BinaryModule.h"
#include "Engine/Scripting/ScriptingObject.h"
#include "Engine/Scripting/ManagedCLR/MCore.h"
#include "Engine/Visject/GraphParameter.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// ---- platform layer -------------------------------------------------------------------------
// Same layout as UnixPlatform::Allocate (offset stored behind the aligned pointer), backed by
// malloc so ASan sees every engine allocation.
typedef uint32 offset_t;

void* UnixPlatform::Allocate(uint64 size, uint64 alignment)
{
    if (!alignment || !size)
        return nullptr;
    const uint32 pad = sizeof(offset_t) + (uint32)(alignment - 1);
    void* p = malloc(size + pad);
    if (!p)
        abort();
    void* ptr = (void*)((((uintptr_t)p + sizeof(offset_t)) + (alignment - 1)) & ~(uintptr_t)(alignment - 1));
    *((offset_t*)ptr - 1) = (offset_t)((uintptr_t)ptr - (uintptr_t)p);
    return ptr;
}

void UnixPlatform::Free(void* ptr)
{
    if (ptr)
    {
        const offset_t offset = *((offset_t*)ptr - 1);
        free((void*)((uintptr_t)ptr - offset));
    }
}

static void FillTime(const struct tm& t, int32& year, int32& month, int32& dayOfWeek, int32& day, int32& hour, int32& minute, int32& second, int32& millisecond)
{
    year = t.tm_year + 1900;
    month = t.tm_mon + 1;
    dayOfWeek = t.tm_wday;
    day = t.tm_mday;
    hour = t.tm_hour;
    minute = t.tm_min;
    second = t.tm_sec;
    millisecond = 0;
}

void LinuxPlatform::GetSystemTime(int32& year, int32& month, int32& dayOfWeek, int32& day, int32& hour, int32& minute, int32& second, int32& millisecond)
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    FillTime(t, year, month, dayOfWeek, day, hour, minute, second, millisecond);
}

void LinuxPlatform::GetUTCTime(int32& year, int32& month, int32& dayOfWeek, int32& day, int32& hour, int32& minute, int32& second, int32& millisecond)
{
    time_t now = time(nullptr);
    struct tm t;
    gmtime_r(&now, &t);
    FillTime(t, year, month, dayOfWeek, day, hour, minute, second, millisecond);
}

bool LinuxPlatform::IsDebuggerPresent()
{
    return false;
}

void PlatformBase::Assert(const char* message, const char* file, int line)
{
    fprintf(stderr, "ASSERT FAILED: %s (%s:%d)\n", message, file, line);
    abort();
}

void PlatformBase::CheckFailed(const char* message, const char* file, int line)
{
    fprintf(stderr, "CHECK FAILED: %s (%s:%d)\n", message, file, line);
    abort();
}

void PlatformBase::Crash(int32 line, const char* file)
{
    fprintf(stderr, "CRASH (%s:%d)\n", file, line);
    abort();
}

void PlatformBase::MissingCode(int32 line, const char* file, const char* info)
{
    fprintf(stderr, "MISSING CODE: %s (%s:%d)\n", info, file, line);
    abort();
}

void PlatformBase::CreateGuid(Guid& result)
{
    static uint32 counter = 0;
    result = Guid(0x1234abcd, 0, 0, ++counter);
}

void FileSystemBase::NormalizePath(String& path)
{
    path.Replace('\\', '/');
    if (path.Length() > 2 && StringUtils::IsAlpha(path[0]) && path[1] == ':')
    {
        path[2] = '\\';
    }
}

// ---- Object / scripting / content registries ------------------------------------------------
Object::~Object()
{
}

void Object::DeleteObject(float timeToLive, bool useGameTime)
{
}

ScriptingTypeInitializer::ScriptingTypeInitializer(BinaryModule* module, const StringAnsiView& fullname, int32 size, ScriptingType::InitRuntimeHandler initRuntime, ScriptingType::SpawnHandler spawn, ScriptingTypeInitializer* baseType, ScriptingType::SetupScriptVTableHandler setupScriptVTable, ScriptingType::SetupScriptObjectVTableHandler setupScriptObjectVTable, const ScriptingType::InterfaceImplementation* interfaces)
    : ScriptingTypeHandle(module, -1)
{
}

const ScriptingType& ScriptingTypeHandle::GetType() const
{
    // Registry-less build: only used as an opaque lookup key by the stubbed Scripting::Find* calls.
    alignas(16) static char storage[sizeof(ScriptingType)];
    return *(const ScriptingType*)storage;
}

ScriptingObject* ScriptingType::GetDefaultInstance() const
{
    return nullptr;
}

ScriptingTypeHandle Scripting::FindScriptingType(const StringAnsiView& fullname)
{
    return ScriptingTypeHandle();
}

ScriptingObject* FindObject(const Guid& id, MClass* type)
{
    return nullptr;
}

Asset* Content::LoadAsync(const Guid& id, const ScriptingTypeHandle& type)
{
    return nullptr;
}

void Asset::AddReference()
{
}

void Asset::RemoveReference()
{
}

bool Asset::WaitForLoaded(double timeoutInMilliseconds) const
{
    return true;
}

Actor* Prefab::GetDefaultInstance()
{
    return nullptr;
}

#define STUB_TYPE(T) ScriptingTypeInitializer T::TypeInitializer(nullptr, StringAnsiView(#T), (int32)sizeof(T));
#define STUB_TEMPLATE_TYPE(T) template<> ScriptingTypeInitializer T::TypeInitializer(nullptr, StringAnsiView(#T), (int32)sizeof(T));
STUB_TEMPLATE_TYPE(Float2)
STUB_TEMPLATE_TYPE(Float3)
STUB_TEMPLATE_TYPE(Float4)
STUB_TEMPLATE_TYPE(Double3)
STUB_TEMPLATE_TYPE(Double4)
STUB_TEMPLATE_TYPE(Int4)
STUB_TYPE(Asset)
STUB_TYPE(GraphParameter)
STUB_TYPE(ScriptingObject)
STUB_TYPE(Prefab)
STUB_TYPE(Quaternion)
STUB_TYPE(Matrix)
STUB_TYPE(Color)
STUB_TYPE(Transform)
STUB_TYPE(Ray)
STUB_TYPE(Rectangle)
STUB_TYPE(BoundingBox)
STUB_TYPE(BoundingSphere)

Asset* LoadAsset(const Guid& id, const ScriptingTypeHandle& type)
{
    return nullptr;
}

BinaryModule::BinaryModulesList& BinaryModule::GetModules()
{
    static BinaryModulesList modules;
    return modules;
}

MGCHandle MCore::GCHandle::New(MObject* obj, bool pinned)
{
    return 0;
}

MObject* MCore::GCHandle::GetTarget(const MGCHandle& handle)
{
    return nullptr;
}

void MCore::GCHandle::Free(const MGCHandle& handle)
{
}

void ScriptingType::DefaultInitRuntime()
{
}

ScriptingObject* ScriptingType::DefaultSpawn(const ScriptingObjectSpawnParams& params)
{
    return nullptr;
}

UnixFile* UnixFile::Open(const StringView& path, FileMode mode, FileAccess access, FileShare share)
{
    return nullptr;
}

// Classes whose key functions live in engine modules that are not linked: only their typeinfo
// is referenced (as a base of other typeinfos), never used for dynamic_cast on the fuzzed paths.
#define STUB_TYPEINFO(mangled) \
    __asm__(".data\n.balign 8\n.globl _ZTI" mangled "\n.type _ZTI" mangled ",@object\n" \
            "_ZTI" mangled ":\n.quad _ZTVN10__cxxabiv117__class_type_infoE+16\n.quad _ZTS" mangled "\n" \
            ".section .rodata\n.globl _ZTS" mangled "\n_ZTS" mangled ":\n.asciz \"" mangled "\"\n.data\n")
STUB_TYPEINFO("11SceneObject");
STUB_TYPEINFO("6Prefab");
STUB_TYPEINFO("12BinaryModule");
STUB_TYPEINFO("5Asset");
STUB_TYPEINFO("8UnixFile");

ScriptingObject::ScriptingObject(const SpawnParams& params)
    : _gcHandle(0)
    , _type(params.Type)
    , _id(params.ID)
{
}

ScriptingObject::~ScriptingObject()
{
}

void ScriptingObject::ChangeID(const Guid& newId)
{
    _id = newId;
}

void ScriptingObject::SetManagedInstance(MObject* instance)
{
}

void ScriptingObject::OnManagedInstanceDeleted()
{
}

void ScriptingObject::OnScriptingDispose()
{
}

bool ScriptingObject::CreateManaged()
{
    return false;
}

void ScriptingObject::DestroyManaged()
{
}

void ScriptingObject::OnDeleteObject()
{
}

String ScriptingObject::ToString() const
{
    return String::Empty;
}
