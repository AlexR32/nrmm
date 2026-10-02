#include "pch.h"

#include "il2cpp.h"
#include "bindings.h"

#include <iostream>
#include <string_view>
#include <array>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <unordered_map>
#include <type_traits>
#include <utility>

using namespace Il2CppBindings;

// Runtime state. Everything the wrapper caches between calls lives in private
// static members, so including il2cpp.h does not pull in the binding table or
// the implementation helpers

HMODULE Il2Cpp::dll = nullptr;
Il2CppDomain* Il2Cpp::pDomain = nullptr;
std::atomic_bool Il2Cpp::initialized{ false };

std::unordered_map<std::string, Il2CppImage*> Il2Cpp::imageCache{};
std::unordered_map<std::string, Il2CppClass*> Il2Cpp::classCache{};
std::mutex Il2Cpp::initMutex{};
std::mutex Il2Cpp::imageMutex{};
std::mutex Il2Cpp::classMutex{};

// Lifecycle

bool Il2Cpp::Initialize() {
    if (initialized.load(std::memory_order_acquire)) return true;

    std::lock_guard<std::mutex> lock(initMutex);
    if (initialized.load(std::memory_order_relaxed)) return true;

    if (!TryLoad()) {
        std::cerr << "[Il2Cpp] Module not found\n";
        return false;
    }

    if (!BindExports()) {
        dll = nullptr;
        return false;
    }

    Il2CppDomain* domain = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
    if (!domain) {
        std::cerr << "[Il2Cpp] Failed to obtain the IL2CPP domain\n";
        return false;
    }

    pDomain = domain;

    initialized.store(true, std::memory_order_release);
    return true;
}

bool Il2Cpp::Loaded() {
    return initialized.load(std::memory_order_acquire) && dll != nullptr;
}

void Il2Cpp::ThreadAttach() {
    if (!Initialize()) return;
    if (pDomain) il2cpp_thread_attach(pDomain);
}

Il2CppDomain* Il2Cpp::Domain() {
    return pDomain;
}

HMODULE Il2Cpp::TryLoad() noexcept {
    static constexpr std::array<std::string_view, 3> candidates = {
        "GameAssembly.dll",
        "GameAssembly",
        "libil2cpp.dll"
    };

    for (const std::string_view& name : candidates) {
        HMODULE module = GetModuleHandleA(name.data());
        if (module) {
            dll = module;
            return module;
        }
    }
    return nullptr;
}

bool Il2Cpp::BindExports() {
    ExportBinder binder{ dll };
    return Il2CppBindings::BindAll(binder);
}

// Helpers

std::string Il2Cpp::Normalize(const std::string& value) {
    std::string out = value;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    const std::string suffix = ".dll";
    if (out.size() > suffix.size() && out.compare(out.size() - suffix.size(), suffix.size(), suffix) == 0) {
        out.erase(out.size() - suffix.size());
    }
    return out;
}

std::string Il2Cpp::Utf16ToUtf8(const uint16_t* data, int length) {
    if (!data || length <= 0) return "";

    int size = WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<LPCWCH>(data), length, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";

    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<LPCWCH>(data), length, out.data(), size, nullptr, nullptr);
    return out;
}

// Images

Il2CppImage* Il2Cpp::GetImage(const std::string& assemblyName) {
    if (!Initialize()) return nullptr;

    std::string key = Normalize(assemblyName);
    {
        std::lock_guard<std::mutex> lock(imageMutex);
        std::unordered_map<std::string, Il2CppImage*>::iterator it = imageCache.find(key);
        if (it != imageCache.end()) return it->second;
    }

    if (!il2cpp_domain_get_assemblies || !il2cpp_assembly_get_image) return nullptr;

    size_t count = 0;
    const Il2CppAssembly** assemblies = il2cpp_domain_get_assemblies(pDomain, &count);
    Il2CppImage* found = nullptr;
    if (assemblies) {
        for (size_t i = 0; i < count; ++i) {
            Il2CppImage* image = il2cpp_assembly_get_image(assemblies[i]);
            if (!image) continue;

            const char* name = il2cpp_image_get_name ? il2cpp_image_get_name(image) : nullptr;
            if (name && Normalize(name) == key) {
                found = image;
                break;
            }
        }
    }

    std::lock_guard<std::mutex> lock(imageMutex);
    imageCache[key] = found;
    return found;
}

// Classes

Il2CppClass* Il2Cpp::FindClass(const std::string& requested, Il2CppImage* image) {
    if (!Initialize()) return nullptr;

    std::string name = requested;
    if (name.rfind("Il2Cpp.", 0) == 0) name = name.substr(7);

    if (!image) image = GetImage("Assembly-CSharp");

    std::string key = std::to_string(reinterpret_cast<uintptr_t>(image)) + "|" + name;
    {
        std::lock_guard<std::mutex> lock(classMutex);
        std::unordered_map<std::string, Il2CppClass*>::iterator it = classCache.find(key);
        if (it != classCache.end()) return it->second;
    }

    Il2CppClass* klass = FindClassInImage(image, name);

    std::lock_guard<std::mutex> lock(classMutex);
    classCache[key] = klass;
    return klass;
}

Il2CppClass* Il2Cpp::GetCachedClass(const std::string& name, Il2CppImage* image) {
    return FindClass(name, image);
}

std::string Il2Cpp::GetClassFullName(Il2CppClass* klass) {
    if (!klass) return "";

    std::vector<std::string> parts;
    Il2CppClass* current = klass;
    while (current) {
        const char* n = il2cpp_class_get_name(current);
        parts.insert(parts.begin(), n ? n : "");

        current = il2cpp_class_get_declaring_type ? il2cpp_class_get_declaring_type(current) : nullptr;
        if (current == klass) break;
    }

    const char* ns = il2cpp_class_get_namespace(klass);
    std::string full = (ns && *ns) ? std::string(ns) + "." : std::string();
    for (size_t i = 0; i < parts.size(); ++i) {
        full += parts[i];
        if (i + 1 < parts.size()) full += ".";
    }
    return full;
}

Il2CppClass* Il2Cpp::FindClassInImage(Il2CppImage* image, const std::string& name) {
    if (!image) return nullptr;

    // Fast path: namespace qualified name
    size_t dot = name.find_last_of('.');
    if (il2cpp_class_from_name) {
        if (dot != std::string::npos) {
            std::string ns = name.substr(0, dot);
            std::string cls = name.substr(dot + 1);
            if (Il2CppClass* klass = il2cpp_class_from_name(image, ns.c_str(), cls.c_str())) {
                return klass;
            }
        } else if (Il2CppClass* klass = il2cpp_class_from_name(image, "", name.c_str())) {
            return klass;
        }
    }

    // Slow path: full name scan, required for nested types
    if (!il2cpp_image_get_class || !il2cpp_image_get_class_count) return nullptr;

    size_t count = il2cpp_image_get_class_count(image);
    for (size_t i = 0; i < count; ++i) {
        Il2CppClass* klass = const_cast<Il2CppClass*>(il2cpp_image_get_class(image, i));
        if (klass && GetClassFullName(klass) == name) return klass;
    }
    return nullptr;
}

// Methods

const MethodInfo* Il2Cpp::GetMethod(Il2CppClass* klass, const char* methodName, int paramCount) {
    if (!klass || !il2cpp_class_get_method_from_name) return nullptr;

    // Walk the hierarchy so inherited methods resolve too
    for (Il2CppClass* k = klass; k; k = il2cpp_class_get_parent ? il2cpp_class_get_parent(k) : nullptr) {
        const MethodInfo* method = il2cpp_class_get_method_from_name(k, methodName, paramCount);
        if (method) return method;
        if (!il2cpp_class_get_parent) break;
    }
    return nullptr;
}

const MethodInfo* Il2Cpp::GetMethod(const char* className, const char* methodName, int paramCount, Il2CppImage* image) {
    return GetMethod(FindClass(className, image), methodName, paramCount);
}

const MethodInfo* Il2Cpp::GetMethodByParamClass(Il2CppClass* klass, const char* methodName, int paramCount, int paramIndex, const char* paramClassName) {
    if (!klass || !il2cpp_class_get_methods || !il2cpp_method_get_name) return nullptr;

    for (Il2CppClass* k = klass; k; k = il2cpp_class_get_parent ? il2cpp_class_get_parent(k) : nullptr) {
        void* iter = nullptr;
        while (const MethodInfo* method = il2cpp_class_get_methods(k, &iter)) {
            const char* name = il2cpp_method_get_name(method);
            if (!name || strcmp(name, methodName) != 0) continue;

            // Skip generic definitions: callers want a concrete method.
            if (il2cpp_method_is_generic && il2cpp_method_is_generic(method)) continue;

            if (paramCount >= 0 && il2cpp_method_get_param_count && static_cast<int>(il2cpp_method_get_param_count(method)) != paramCount) continue;

            if (!il2cpp_method_get_param || !il2cpp_class_from_il2cpp_type) continue;
            const Il2CppType* paramType = il2cpp_method_get_param(method, static_cast<uint32_t>(paramIndex));
            if (!paramType) continue;

            Il2CppClass* paramClass = il2cpp_class_from_il2cpp_type(paramType);
            const char* clsName = paramClass ? il2cpp_class_get_name(paramClass) : nullptr;
            if (clsName && strcmp(clsName, paramClassName) == 0) return method;
        }

        if (!il2cpp_class_get_parent) break;
    }
    return nullptr;
}

const MethodInfo* Il2Cpp::GetGenericMethodByParamClass(Il2CppClass* klass, const char* methodName, int paramCount, int paramIndex, const char* paramClassName) {
    if (!klass || !il2cpp_class_get_methods || !il2cpp_method_get_name) return nullptr;

    for (Il2CppClass* k = klass; k; k = il2cpp_class_get_parent ? il2cpp_class_get_parent(k) : nullptr) {
        void* iter = nullptr;
        while (const MethodInfo* method = il2cpp_class_get_methods(k, &iter)) {
            const char* name = il2cpp_method_get_name(method);
            if (!name || strcmp(name, methodName) != 0) continue;

            // Only generic definitions; the caller inflates them
            if (!il2cpp_method_is_generic || !il2cpp_method_is_generic(method)) continue;

            if (paramCount >= 0 && il2cpp_method_get_param_count && static_cast<int>(il2cpp_method_get_param_count(method)) != paramCount) continue;

            if (!il2cpp_method_get_param || !il2cpp_class_from_il2cpp_type) continue;
            const Il2CppType* paramType = il2cpp_method_get_param(method, static_cast<uint32_t>(paramIndex));
            if (!paramType) continue;

            Il2CppClass* paramClass = il2cpp_class_from_il2cpp_type(paramType);
            const char* clsName = paramClass ? il2cpp_class_get_name(paramClass) : nullptr;
            if (clsName && strcmp(clsName, paramClassName) == 0) return method;
        }

        if (!il2cpp_class_get_parent) break;
    }
    return nullptr;
}

void* Il2Cpp::GetCompiledMethod(const MethodInfo* method) {
    if (!method) return nullptr;
    return *reinterpret_cast<void* const*>(method);
}

// Invocation

Il2CppObject* Il2Cpp::Invoke(const MethodInfo* method, void* obj, void** params, Il2CppException** outExc) {
    if (!method || !il2cpp_runtime_invoke) return nullptr;

    Il2CppException* exc = nullptr;
    Il2CppObject* result = il2cpp_runtime_invoke(method, obj, params, &exc);
    if (outExc) *outExc = exc;

    if (exc) {
        std::string message = GetExceptionText(exc);
        std::cerr << "[Il2Cpp] Managed exception thrown during invoke: " << (message.empty() ? "<no message>" : message) << "\n";
    }
    return result;
}

std::string Il2Cpp::GetExceptionText(Il2CppException* exception) {
    if (!exception) return "";

    Il2CppObject* message = GetInstanceFieldObject(reinterpret_cast<Il2CppObject*>(exception), "_message");
    if (!message) return "";

    return StringToUtf8(reinterpret_cast<Il2CppString*>(message));
}

// Fields

FieldInfo* Il2Cpp::FindField(Il2CppClass* klass, const char* fieldName) {
    if (!klass || !il2cpp_class_get_field_from_name) return nullptr;

    for (Il2CppClass* k = klass; k; k = il2cpp_class_get_parent ? il2cpp_class_get_parent(k) : nullptr) {
        FieldInfo* field = il2cpp_class_get_field_from_name(k, fieldName);
        if (field) return field;
        if (!il2cpp_class_get_parent) break;
    }
    return nullptr;
}

Il2CppClass* Il2Cpp::ObjectClass(Il2CppObject* object) {
    if (!object || !il2cpp_object_get_class) return nullptr;
    return il2cpp_object_get_class(object);
}

bool Il2Cpp::GetInstanceFieldRaw(FieldInfo* field, Il2CppObject* instance, void* out) {
    if (!field || !instance || !il2cpp_field_get_value) return false;
    il2cpp_field_get_value(instance, field, out);
    return true;
}

bool Il2Cpp::SetInstanceFieldRaw(FieldInfo* field, Il2CppObject* instance, const void* value) {
    if (!field || !instance || !il2cpp_field_set_value) return false;
    il2cpp_field_set_value(instance, field, const_cast<void*>(value));
    return true;
}

Il2CppObject* Il2Cpp::GetInstanceFieldObject(Il2CppObject* instance, const char* fieldName) {
    Il2CppObject* value = nullptr;
    return GetInstanceFieldValue(instance, fieldName, value) ? value : nullptr;
}

bool Il2Cpp::SetInstanceFieldObject(Il2CppObject* instance, const char* fieldName, void* value) {
    if (!instance) return false;

    FieldInfo* field = FindField(ObjectClass(instance), fieldName);
    if (!field || !il2cpp_field_set_value) return false;

    il2cpp_field_set_value(instance, field, value);
    return true;
}

Il2CppObject* Il2Cpp::GetStaticFieldObject(Il2CppClass* klass, const char* fieldName) {
    FieldInfo* field = FindField(klass, fieldName);
    if (!field) return nullptr;

    Il2CppObject* value = nullptr;
    return GetFieldRaw(field, &value) ? value : nullptr;
}

bool Il2Cpp::GetFieldRaw(FieldInfo* field, void* out) {
    if (!field || !il2cpp_field_static_get_value) return false;
    il2cpp_field_static_get_value(field, out);
    return true;
}

uint32_t Il2Cpp::GetFieldFlags(FieldInfo* field) {
    return (field && il2cpp_field_get_flags) ? il2cpp_field_get_flags(field) : 0;
}

std::vector<Il2Cpp::EnumMember> Il2Cpp::GetEnumMembers(Il2CppClass* enumClass) {
    std::vector<EnumMember> members;
    if (!enumClass || !il2cpp_class_get_fields || !il2cpp_field_get_name) return members;

    uint32_t align = 0;
    int32_t size = il2cpp_class_value_size ? il2cpp_class_value_size(enumClass, &align) : 4;
    if (size <= 0 || size > 8) size = 4;

    void* iter = nullptr;
    while (FieldInfo* field = il2cpp_class_get_fields(enumClass, &iter)) {
        // Only enum literals are static; value__ is the instance field
        if (il2cpp_field_get_flags && (GetFieldFlags(field) & 0x10) == 0) continue;

        const char* name = il2cpp_field_get_name(field);
        if (!name || strcmp(name, "value__") == 0) continue;

        EnumMember member;
        member.name = name;
        member.raw.assign(static_cast<size_t>(size), 0);
        if (!GetFieldRaw(field, member.raw.data())) continue;

        members.push_back(std::move(member));
    }
    return members;
}

const Il2Cpp::EnumMember* Il2Cpp::FindEnumMember(const std::vector<EnumMember>& members, const char* name) {
    for (const Il2Cpp::EnumMember& member : members) {
        if (member.name == name) return &member;
    }
    return nullptr;
}

// Types / reflection

Il2CppObject* Il2Cpp::GetTypeObject(Il2CppClass* klass) {
    if (!klass || !il2cpp_class_get_type || !il2cpp_type_get_object) return nullptr;
    return il2cpp_type_get_object(il2cpp_class_get_type(klass));
}

Il2CppClass* Il2Cpp::GetClassFromTypeObject(Il2CppObject* typeObject) {
    if (!typeObject) return nullptr;
    Il2CppReflectionType* reflection = reinterpret_cast<Il2CppReflectionType*>(typeObject);
    if (!reflection->type || !il2cpp_class_from_il2cpp_type) return nullptr;
    return il2cpp_class_from_il2cpp_type(reflection->type);
}

Il2CppClass* Il2Cpp::GetSystemTypeClass() {
    static Il2CppClass* cached = nullptr;
    if (cached) return cached;

    Il2CppImage* corlib = GetImage("mscorlib");
    if (!corlib) corlib = GetImage("System.Private.CoreLib");
    cached = corlib ? FindClassInImage(corlib, "System.Type") : nullptr;
    return cached;
}

const MethodInfo* Il2Cpp::InflateGenericMethod(const MethodInfo* genericDefinition, Il2CppClass* declaringClass, const std::vector<Il2CppClass*>& typeArgs) {
    if (!genericDefinition || !il2cpp_method_get_object || !il2cpp_array_new || !il2cpp_runtime_invoke) {
        return nullptr;
    }

    Il2CppImage* corlib = GetImage("mscorlib");
    if (!corlib) corlib = GetImage("System.Private.CoreLib");
    if (!corlib) return nullptr;

    Il2CppClass* typeClass = GetSystemTypeClass();
    Il2CppClass* methodInfoClass = FindClassInImage(corlib, "System.Reflection.MethodInfo");
    if (!typeClass || !methodInfoClass) return nullptr;

    Il2CppObject* reflectionMethod = reinterpret_cast<Il2CppObject*>(il2cpp_method_get_object(genericDefinition, declaringClass));
    if (!reflectionMethod) return nullptr;

    Il2CppArray* types = il2cpp_array_new(typeClass, static_cast<uintptr_t>(typeArgs.size()));
    if (!types) return nullptr;

    for (size_t i = 0; i < typeArgs.size(); ++i) {
        Il2CppObject* typeObject = GetTypeObject(typeArgs[i]);
        if (!typeObject) return nullptr;
        ArraySetRef(types, i, typeObject);
    }

    const MethodInfo* makeGeneric = GetMethod(methodInfoClass, "MakeGenericMethod", 1);
    if (!makeGeneric) return nullptr;

    void* args[1] = {types};
    Il2CppObject* inflated = Invoke(makeGeneric, reflectionMethod, args);
    if (!inflated) return nullptr;

    return reinterpret_cast<Il2CppReflectionMethod*>(inflated)->method;
}

Il2CppObject* Il2Cpp::InvokeGeneric(Il2CppClass* klass, const char* methodName, int paramCount, const std::vector<Il2CppClass*>& typeArgs, void* obj, void** params) {
    const MethodInfo* definition = GetMethod(klass, methodName, paramCount);
    if (!definition) return nullptr;

    const MethodInfo* inflated = InflateGenericMethod(definition, klass, typeArgs);
    if (!inflated) return nullptr;

    return Invoke(inflated, obj, params);
}

// Unity helpers

Il2CppObject* Il2Cpp::FindObjectOfType(Il2CppClass* klass) {
    if (!klass) return nullptr;

    Il2CppImage* coreModule = GetImage("UnityEngine.CoreModule");
    Il2CppClass* objectClass = FindClassInImage(coreModule, "UnityEngine.Object");
    if (!objectClass) return nullptr;

    // Preferred: the non-generic overload FindObjectOfType(System.Type)
    const MethodInfo* byType = GetMethod(objectClass, "FindObjectOfType", 1);
    if (byType) {
        Il2CppObject* typeObject = GetTypeObject(klass);
        if (!typeObject) return nullptr;

        void* args[1] = {typeObject};
        return Invoke(byType, nullptr, args);
    }

    // Fallback: FindObjectOfType<T>()
    const MethodInfo* generic = GetMethod(objectClass, "FindObjectOfType", 0);
    if (!generic) generic = GetMethod(objectClass, "FindAnyObjectByType", 0);
    if (!generic) return nullptr;

    const MethodInfo* inflated = InflateGenericMethod(generic, objectClass, {klass});
    if (!inflated) return nullptr;

    return Invoke(inflated, nullptr, nullptr);
}

bool Il2Cpp::CurrentThreadIsMainThread() {
    Il2CppImage* coreModule = GetImage("UnityEngine.CoreModule");
    Il2CppClass* objectClass = FindClassInImage(coreModule, "UnityEngine.Object");
    if (!objectClass) return false;

    const MethodInfo* byType = GetMethod(objectClass, "CurrentThreadIsMainThread", 0);
    if (!byType) return false;

    return Invoke(byType, nullptr, nullptr);
}

int Il2Cpp::GetCurrentThreadId() {
    Il2CppImage* coreModule = GetImage("UnityEngine.CoreModule");
    Il2CppClass* objectClass = FindClassInImage(coreModule, "UnityEngine.Object");
    if (!objectClass) return 0;

    const MethodInfo* byType = GetMethod(objectClass, "GetCurrentThreadId", 0);
    if (!byType) return 0;

    Il2CppObject* result = Invoke(byType, nullptr, nullptr);
    return UnboxInt32(result);
}

Il2CppArray* Il2Cpp::FindObjectsOfType(Il2CppClass* klass) {
    if (!klass) return nullptr;

    Il2CppImage* coreModule = GetImage("UnityEngine.CoreModule");
    Il2CppClass* objectClass = FindClassInImage(coreModule, "UnityEngine.Object");
    if (!objectClass) return nullptr;

    // Preferred: the non-generic overload FindObjectsOfType(System.Type)
    const MethodInfo* byType = GetMethod(objectClass, "FindObjectsOfType", 1);
    if (byType) {
        Il2CppObject* typeObject = GetTypeObject(klass);
        if (!typeObject) return nullptr;

        void* args[1] = {typeObject};
        return reinterpret_cast<Il2CppArray*>(Invoke(byType, nullptr, args));
    }

    // Fallback: FindObjectsOfType<T>()
    const MethodInfo* generic = GetMethod(objectClass, "FindObjectsOfType", 0);
    if (!generic) return nullptr;

    const MethodInfo* inflated = InflateGenericMethod(generic, objectClass, {klass});
    if (!inflated) return nullptr;

    return reinterpret_cast<Il2CppArray*>(Invoke(inflated, nullptr, nullptr));
}

Il2CppArray* Il2Cpp::FindObjectsOfTypeAll(Il2CppClass* klass) {
    if (!klass) return nullptr;

    Il2CppImage* coreModule = GetImage("UnityEngine.CoreModule");
    Il2CppClass* resourcesClass = FindClassInImage(coreModule, "UnityEngine.Resources");
    if (!resourcesClass) return nullptr;

    const MethodInfo* byType = GetMethod(resourcesClass, "FindObjectsOfTypeAll", 1);
    if (!byType) return nullptr;

    Il2CppObject* typeObject = GetTypeObject(klass);
    if (!typeObject) return nullptr;

    void* args[1] = {typeObject};
    return reinterpret_cast<Il2CppArray*>(Invoke(byType, nullptr, args));
}

Il2CppObject* Il2Cpp::GetComponent(Il2CppObject* gameObject, Il2CppClass* componentClass) {
    if (!gameObject || !componentClass) return nullptr;

    Il2CppImage* coreModule = GetImage("UnityEngine.CoreModule");
    Il2CppClass* gameObjectClass = FindClassInImage(coreModule, "UnityEngine.GameObject");
    if (!gameObjectClass) return nullptr;

    Il2CppObject* typeObject = GetTypeObject(componentClass);
    if (!typeObject) return nullptr;

    // Prefer GetComponent(System.Type); it avoids inflating the generic
    const MethodInfo* byType = GetMethodByParamClass(gameObjectClass, "GetComponent", 1, 0, "Type");
    if (byType) {
        void* args[1] = {typeObject};
        return Invoke(byType, gameObject, args);
    }

    // Fallback: GetComponent<T>()
    return InvokeGeneric(gameObjectClass, "GetComponent", 0, {componentClass}, gameObject, nullptr);
}

Il2CppObject* Il2Cpp::GetGameObject(Il2CppObject* object) {
    if (!object) return nullptr;

    Il2CppClass* klass = ObjectClass(object);
    const MethodInfo* getter = GetMethod(klass, "get_gameObject", 0);
    return getter ? Invoke(getter, object, nullptr) : nullptr;
}

bool Il2Cpp::IsUnityObjectAlive(Il2CppObject* object) {
    if (!object) return false;

    // Unity keeps the managed wrapper after the native object is destroyed; the
    // native pointer field is null then and any accessor on it would throw. A
    // non-Unity object has no such field, so treat it as alive
    void* cachedPtr = nullptr;
    if (GetInstanceFieldValue(object, "m_CachedPtr", cachedPtr)) {
        return cachedPtr != nullptr;
    }
    return true;
}

bool Il2Cpp::IsActiveSelf(Il2CppObject* object) {
    if (!IsUnityObjectAlive(object)) return false;

    Il2CppObject* gameObject = GetGameObject(object);
    if (!IsUnityObjectAlive(gameObject)) return false;

    const MethodInfo* getter = GetMethod(ObjectClass(gameObject), "get_activeSelf", 0);
    if (!getter) return false;

    return UnboxBool(Invoke(getter, gameObject, nullptr));
}

Il2CppObject* Il2Cpp::GetTransform(Il2CppObject* component) {
    if (!component) return nullptr;

    Il2CppImage* coreModule = GetImage("UnityEngine.CoreModule");
    Il2CppClass* componentClass = FindClassInImage(coreModule, "UnityEngine.Component");
    if (!componentClass) return nullptr;

    const MethodInfo* getter = GetMethod(componentClass, "get_transform", 0);
    return Invoke(getter, component, nullptr);
}

Il2CppObject* Il2Cpp::GetGameObjectTransform(Il2CppObject* gameObject) {
    if (!gameObject) return nullptr;

    Il2CppImage* coreModule = GetImage("UnityEngine.CoreModule");
    Il2CppClass* gameObjectClass = FindClassInImage(coreModule, "UnityEngine.GameObject");
    if (!gameObjectClass) return nullptr;

    const MethodInfo* getter = GetMethod(gameObjectClass, "get_transform", 0);
    return Invoke(getter, gameObject, nullptr);
}

Il2CppObject* Il2Cpp::StartCoroutine(Il2CppObject* behaviour, Il2CppObject* routine) {
    if (!behaviour || !routine) return nullptr;

    Il2CppImage* coreModule = GetImage("UnityEngine.CoreModule");
    Il2CppClass* monoBehaviourClass = FindClassInImage(coreModule, "UnityEngine.MonoBehaviour");
    if (!monoBehaviourClass) return nullptr;

    static const MethodInfo* startCoroutine = nullptr;
    if (!startCoroutine) {
        startCoroutine = GetMethodByParamClass(monoBehaviourClass, "StartCoroutine", 1, 0, "IEnumerator");
    }
    if (!startCoroutine) return nullptr;

    void* args[1] = {routine};
    return Invoke(startCoroutine, behaviour, args);
}

// Arrays / strings

Il2CppArray* Il2Cpp::NewArray(Il2CppClass* elementClass, size_t count) {
    if (!elementClass || !il2cpp_array_new) return nullptr;
    return il2cpp_array_new(elementClass, static_cast<uintptr_t>(count));
}

size_t Il2Cpp::ArrayLength(Il2CppArray* array) {
    if (!array) return 0;
    if (il2cpp_array_length) return static_cast<size_t>(il2cpp_array_length(array));
    return static_cast<size_t>(array->max_length);
}

void Il2Cpp::ArraySetRef(Il2CppArray* array, size_t index, Il2CppObject* value) {
    if (!array) return;
    reinterpret_cast<Il2CppObject**>(reinterpret_cast<uint8_t*>(array) + kArrayDataOffset)[index] = value;
}

Il2CppObject* Il2Cpp::ArrayGetRef(Il2CppArray* array, size_t index) {
    if (!array) return nullptr;
    return reinterpret_cast<Il2CppObject**>(reinterpret_cast<uint8_t*>(array) + kArrayDataOffset)[index];
}

void Il2Cpp::ArraySetRaw(Il2CppArray* array, size_t index, const void* value, size_t size) {
    if (!array || !value) return;
    memcpy(reinterpret_cast<uint8_t*>(array) + kArrayDataOffset + index * size, value, size);
}

Il2CppString* Il2Cpp::NewString(const char* text) {
    return (il2cpp_string_new && text) ? il2cpp_string_new(text) : nullptr;
}

Il2CppObject* Il2Cpp::NewObject(Il2CppClass* klass) {
    return (klass && il2cpp_object_new) ? il2cpp_object_new(klass) : nullptr;
}

Il2CppObject* Il2Cpp::BoxValue(Il2CppClass* klass, const void* value) {
    return (klass && value && il2cpp_value_box) ? il2cpp_value_box(klass, const_cast<void*>(value)) : nullptr;
}

std::string Il2Cpp::StringToUtf8(Il2CppString* value) {
    if (!value || !il2cpp_string_chars || !il2cpp_string_length) return "";
    return Utf16ToUtf8(il2cpp_string_chars(value), static_cast<int>(il2cpp_string_length(value)));
}

int32_t Il2Cpp::UnboxInt32(Il2CppObject* boxed, int32_t fallback) {
    if (!boxed || !il2cpp_object_unbox) return fallback;
    return *reinterpret_cast<int32_t*>(il2cpp_object_unbox(boxed));
}

bool Il2Cpp::UnboxBool(Il2CppObject* boxed, bool fallback) {
    if (!boxed || !il2cpp_object_unbox) return fallback;
    return *reinterpret_cast<bool*>(il2cpp_object_unbox(boxed)) != 0;
}
