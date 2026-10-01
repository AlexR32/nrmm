#pragma once

#include <pch.h>

#include <windows.h>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>

#include "types.h"

// Thin wrapper over the IL2CPP runtime exported by GameAssembly.dll. Only the
// declarations the rest of the mod uses live here; the exported bindings
// (bindings.h) and every implementation detail stay in il2cpp.cpp

class Il2Cpp {
public:
    static bool Initialize();
    static bool Loaded();

    // Attach the calling thread to the IL2CPP domain. Required before any
    // managed call is made from a thread Unity did not create itself
    static void ThreadAttach();
    static Il2CppDomain* Domain();

    // Images
    static Il2CppImage* GetImage(const std::string& assemblyName);

    // Classes
    // Accepts a plain name ("CarParent"), a namespace qualified name
    // ("System.Reflection.MethodInfo") or a nested/Il2CppInspector style name
    // ("car_carOrigin.ChassisType", "Il2Cpp.CarParent")
    static Il2CppClass* FindClass(const std::string& requested, Il2CppImage* image = nullptr);
    static Il2CppClass* GetCachedClass(const std::string& name, Il2CppImage* image = nullptr);
    static std::string GetClassFullName(Il2CppClass* klass);

    // Methods
    static const MethodInfo* GetMethod(Il2CppClass* klass, const char* methodName, int paramCount = -1);
    static const MethodInfo* GetMethod(const char* className, const char* methodName, int paramCount, Il2CppImage* image = nullptr);

    // Disambiguates overloads that only differ in one parameter type, e.g.
    // MonoBehaviour::StartCoroutine(IEnumerator) vs StartCoroutine(string)
    static const MethodInfo* GetMethodByParamClass(Il2CppClass* klass, const char* methodName, int paramCount, int paramIndex, const char* paramClassName);

    // Same as above but only matches generic definitions, which callers then
    // inflate with InflateGenericMethod
    static const MethodInfo* GetGenericMethodByParamClass(Il2CppClass* klass, const char* methodName, int paramCount, int paramIndex, const char* paramClassName);

    // Native entry point of a compiled method (MethodInfo::methodPointer is the
    // first field in every IL2CPP version)
    static void* GetCompiledMethod(const MethodInfo* method);

    // Invocation
    static Il2CppObject* Invoke(const MethodInfo* method, void* obj, void** params, Il2CppException** outExc = nullptr);
    static std::string GetExceptionText(Il2CppException* exception);

    // Fields
    static FieldInfo* FindField(Il2CppClass* klass, const char* fieldName);
    static Il2CppClass* ObjectClass(Il2CppObject* object);

    static bool GetInstanceFieldRaw(FieldInfo* field, Il2CppObject* instance, void* out);
    static bool SetInstanceFieldRaw(FieldInfo* field, Il2CppObject* instance, const void* value);

    static Il2CppObject* GetInstanceFieldObject(Il2CppObject* instance, const char* fieldName);

    template <typename T>
    static bool GetInstanceFieldValue(Il2CppObject* instance, const char* fieldName, T& outValue) {
        FieldInfo* field = FindField(ObjectClass(instance), fieldName);
        return field && GetInstanceFieldRaw(field, instance, &outValue);
    }

    template <typename T>
    static bool SetInstanceFieldValue(Il2CppObject* instance, const char* fieldName, const T& value) {
        FieldInfo* field = FindField(ObjectClass(instance), fieldName);
        return field && SetInstanceFieldRaw(field, instance, &value);
    }

    // Reference-typed fields are stored differently: the field write stores the
    // object pointer straight into the field; passing &value like
    // SetInstanceFieldValue would store the address of the local variable
    static bool SetInstanceFieldObject(Il2CppObject* instance, const char* fieldName, void* value);

    static Il2CppObject* GetStaticFieldObject(Il2CppClass* klass, const char* fieldName);

    template <typename T>
    static bool GetStaticFieldValue(Il2CppClass* klass, const char* fieldName, T& outValue) {
        FieldInfo* field = FindField(klass, fieldName);
        return field && GetFieldRaw(field, &outValue);
    }

    static bool GetFieldRaw(FieldInfo* field, void* out);
    static uint32_t GetFieldFlags(FieldInfo* field);

    // Enum literals are static fields of the enum class; returns their raw
    // bytes so any underlying integral type works
    struct EnumMember {
        std::string name;
        std::vector<uint8_t> raw;
    };

    static std::vector<EnumMember> GetEnumMembers(Il2CppClass* enumClass);
    static const EnumMember* FindEnumMember(const std::vector<EnumMember>& members, const char* name);

    // Types / reflection
    static Il2CppObject* GetTypeObject(Il2CppClass* klass);
    static Il2CppClass* GetClassFromTypeObject(Il2CppObject* typeObject);
    static Il2CppClass* GetSystemTypeClass();

    // Inflates a generic method definition using managed reflection: reflectionMethod.MakeGenericMethod(new[] { typeArgs })
    static const MethodInfo* InflateGenericMethod(const MethodInfo* genericDefinition, Il2CppClass* declaringClass, const std::vector<Il2CppClass*>& typeArgs);
    static Il2CppObject* InvokeGeneric(Il2CppClass* klass, const char* methodName, int paramCount, const std::vector<Il2CppClass*>& typeArgs, void* obj, void** params);

    // Unity helpers
    // UnityEngine.Object.FindObjectOfType<T>() with a reflection fallback
    static Il2CppObject* FindObjectOfType(Il2CppClass* klass);
    static bool CurrentThreadIsMainThread();
    static int GetCurrentThreadId();

    // UnityEngine.Object.FindObjectsOfType<T>() returning every active instance
    // of the requested type as a managed array
    static Il2CppArray* FindObjectsOfType(Il2CppClass* klass);

    // UnityEngine.Resources.FindObjectsOfTypeAll(Type). Unlike FindObjectsOfType
    // this also returns inactive objects and objects with HideFlags.DontSave, so
    // it is how objects in a HideAndDontSave scene are reached
    static Il2CppArray* FindObjectsOfTypeAll(Il2CppClass* klass);

    // gameObject.GetComponent<T>()
    static Il2CppObject* GetComponent(Il2CppObject* gameObject, Il2CppClass* componentClass);

    // object.gameObject. Works for both Component and GameObject instances,
    // since the accessor is resolved from the object's own class hierarchy
    static Il2CppObject* GetGameObject(Il2CppObject* object);

    // component.transform
    static Il2CppObject* GetTransform(Il2CppObject* component);

    // gameObject.transform. GameObject derives from Object, not Component, so it needs its own accessor
    static Il2CppObject* GetGameObjectTransform(Il2CppObject* gameObject);

    // False when a UnityEngine.Object's native side is gone. Unity keeps the
    // managed wrapper alive after Destroy, so a non-null pointer can still
    // throw when touched; m_CachedPtr (0x10 on UnityEngine.Object) is null then
    static bool IsUnityObjectAlive(Il2CppObject* object);

    // gameObject.activeSelf of the passed component or GameObject. False when
    // the object, its GameObject or the property is missing
    static bool IsActiveSelf(Il2CppObject* object);

    // behaviour.StartCoroutine(IEnumerator)
    static Il2CppObject* StartCoroutine(Il2CppObject* behaviour, Il2CppObject* routine);

    // Arrays / strings
    static constexpr size_t kArrayDataOffset = 0x20; // klass + monitor + bounds + max_length

    static Il2CppArray* NewArray(Il2CppClass* elementClass, size_t count);
    static size_t ArrayLength(Il2CppArray* array);
    static void ArraySetRef(Il2CppArray* array, size_t index, Il2CppObject* value);
    static Il2CppObject* ArrayGetRef(Il2CppArray* array, size_t index);
    static void ArraySetRaw(Il2CppArray* array, size_t index, const void* value, size_t size);

    static Il2CppString* NewString(const char* text);
    static Il2CppObject* NewObject(Il2CppClass* klass);
    static std::string StringToUtf8(Il2CppString* value);

    // Boxes a value type (e.g. an enum literal) into a managed object so it can
    // be passed where an object-typed parameter is expected
    static Il2CppObject* BoxValue(Il2CppClass* klass, const void* value);

    // Reads the int32 a boxed value type (e.g. a boxed property result) points
    // at, without exposing the raw unbox export to callers
    static int32_t UnboxInt32(Il2CppObject* boxed, int32_t fallback = 0);

    // Reads the bool (single byte) a boxed value type points at. Reading it as
    // an int32 would also pick up the padding after the byte
    static bool UnboxBool(Il2CppObject* boxed, bool fallback = false);

private:
    static HMODULE dll;
    static Il2CppDomain* pDomain;
    static std::atomic<bool> initialized;

    static std::unordered_map<std::string, Il2CppImage*> imageCache;
    static std::unordered_map<std::string, Il2CppClass*> classCache;
    static std::mutex initMutex;
    static std::mutex imageMutex;
    static std::mutex classMutex;

    static HMODULE TryLoad() noexcept;
    static bool BindExports();
    static Il2CppClass* FindClassInImage(Il2CppImage* image, const std::string& name);
    static std::string Normalize(const std::string& value);
    static std::string Utf16ToUtf8(const uint16_t* data, int length);
};
