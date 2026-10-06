#pragma once

#include <cstdint>

// IL2CPP opaque forward declarations
typedef struct Il2CppDomain Il2CppDomain;
typedef struct Il2CppAssembly Il2CppAssembly;
typedef struct Il2CppImage Il2CppImage;
typedef struct Il2CppClass Il2CppClass;
typedef struct Il2CppObject Il2CppObject;
typedef struct Il2CppString Il2CppString;
typedef struct Il2CppArray Il2CppArray;
typedef struct Il2CppException Il2CppException;
typedef struct Il2CppReflectionType Il2CppReflectionType;
typedef struct Il2CppReflectionMethod Il2CppReflectionMethod;
typedef struct FieldInfo FieldInfo;
typedef struct MethodInfo MethodInfo;
typedef struct EventInfo EventInfo;
typedef struct PropertyInfo PropertyInfo;
typedef struct Il2CppType Il2CppType;
typedef struct Il2CppThread Il2CppThread;
typedef struct Il2CppProfiler Il2CppProfiler;
typedef struct Il2CppCustomAttrInfo Il2CppCustomAttrInfo;
typedef struct Il2CppMemorySnapshot Il2CppMemorySnapshot;
typedef struct Il2CppMethodDebugInfo Il2CppMethodDebugInfo;
typedef struct Il2CppDebuggerTransport Il2CppDebuggerTransport;
typedef struct Il2CppStackFrameInfo Il2CppStackFrameInfo;
typedef void* Il2CppMethodPointer;
typedef uint16_t Il2CppChar;

typedef uintptr_t il2cpp_array_size_t;

// Concrete structures whose layout we actually read
enum Il2CppTypeEnum : uint8_t {
    IL2CPP_TYPE_END = 0x00,
    IL2CPP_TYPE_VOID = 0x01,
    IL2CPP_TYPE_BOOLEAN = 0x02,
    IL2CPP_TYPE_CHAR = 0x03,
    IL2CPP_TYPE_I1 = 0x04,
    IL2CPP_TYPE_U1 = 0x05,
    IL2CPP_TYPE_I2 = 0x06,
    IL2CPP_TYPE_U2 = 0x07,
    IL2CPP_TYPE_I4 = 0x08,
    IL2CPP_TYPE_U4 = 0x09,
    IL2CPP_TYPE_I8 = 0x0a,
    IL2CPP_TYPE_U8 = 0x0b,
    IL2CPP_TYPE_R4 = 0x0c,
    IL2CPP_TYPE_R8 = 0x0d,
    IL2CPP_TYPE_STRING = 0x0e,
    IL2CPP_TYPE_PTR = 0x0f,
    IL2CPP_TYPE_BYREF = 0x10,
    IL2CPP_TYPE_VALUETYPE = 0x11,
    IL2CPP_TYPE_CLASS = 0x12,
    IL2CPP_TYPE_VAR = 0x13,
    IL2CPP_TYPE_ARRAY = 0x14,
    IL2CPP_TYPE_GENERICINST = 0x15,
    IL2CPP_TYPE_TYPEDBYREF = 0x16,
    IL2CPP_TYPE_I = 0x18,
    IL2CPP_TYPE_U = 0x19,
    IL2CPP_TYPE_FNPTR = 0x1b,
    IL2CPP_TYPE_OBJECT = 0x1c,
    IL2CPP_TYPE_SZARRAY = 0x1d,
    IL2CPP_TYPE_MVAR = 0x1e,
};

struct Il2CppType {
    union {
        void* dummy;
        void* klassIndex;
        const Il2CppType* type;
        void* array;
        void* genericParameterIndex;
        void* generic_class;
    } data;
    unsigned int attrs : 16;
    Il2CppTypeEnum type : 8;
    unsigned int num_mods : 6;
    unsigned int byref : 1;
    unsigned int pinned : 1;
};

struct Il2CppObject {
    Il2CppClass* klass;
    void* monitor;
};

struct Il2CppString {
    Il2CppObject object;
    int32_t length;
    uint16_t chars[1];
};

struct Il2CppArray {
    Il2CppObject obj;
    void* bounds;
    il2cpp_array_size_t max_length;
    void* vector[1];
};

struct Il2CppReflectionType {
    Il2CppObject object;
    const Il2CppType* type;
};

struct Il2CppReflectionMethod {
    Il2CppObject object;
    const MethodInfo* method;
    Il2CppString* name;
    Il2CppReflectionType* reftype;
};
