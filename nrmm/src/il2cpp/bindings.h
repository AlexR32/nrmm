#pragma once

#include "types.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <iostream>

// Resolves one export through GetProcAddress and stores it in the destination
// pointer. Kept as a functor so BindAll can stay a plain list of bind() calls.
struct ExportBinder {
    HMODULE module;

    template <typename T>
    bool operator()(const char* name, T& out, bool required) const {
        FARPROC proc = GetProcAddress(module, name);
        if (!proc) {
            if (required) std::cerr << "[Il2Cpp] [ERR] Could not get " << name << "\n";
            return !required;
        }

        out = reinterpret_cast<T>(proc);
        return true;
    }
};

// Auxiliary callback signatures used by the exported API
typedef void (*Il2CppClassForEachFunc)(Il2CppClass* klass, void* userData);
typedef void (*Il2CppFrameWalkFunc)(const Il2CppStackFrameInfo* info, void* userData);
typedef void (*Il2CppBacktraceFunc)(char* output, int maxLength);
typedef void (*Il2CppLogCallback)(const char* message);
typedef void (*Il2CppGCHeapFunc)(void* data, void* userData);
typedef void (*Il2CppNameChunkFunc)(void* data, void* userData);
typedef void (*Il2CppRegisterObjectFunc)(Il2CppObject* object, void* userData);
typedef void (*Il2CppWorldChangedFunc)(void* userData);
typedef void (*Il2CppProfileFunc)(Il2CppProfiler* prof);
typedef void (*Il2CppProfileMethodFunc)(Il2CppProfiler* prof, const MethodInfo* method);
typedef void (*Il2CppProfileAllocFunc)(Il2CppProfiler* prof, Il2CppObject* obj, Il2CppClass* klass);
typedef void (*Il2CppProfileGCFunc)(Il2CppProfiler* prof, int32_t event, int32_t generation, size_t used_memory);
typedef void (*Il2CppProfileGCResizeFunc)(Il2CppProfiler* prof, int64_t newSize);
typedef void (*Il2CppProfileFileIOFunc)(Il2CppProfiler* prof, int32_t kind, int32_t count);
typedef void (*Il2CppProfileThreadFunc)(Il2CppProfiler* prof, unsigned long tid);

// Exported function signatures (GameAssembly.dll)

// Non IL2CPP utility exports
typedef int32_t (*t_CloseZStream)(void* stream);
typedef void* (*t_CreateZStream)(int32_t compress, int32_t level, void* callback, void* userdata);
typedef int32_t (*t_DllCanUnloadNow)();
typedef int32_t (*t_DllGetActivationFactory)(void* activatableClassId, void** factory);
typedef int32_t (*t_Flush)(void* stream, int32_t flush);
typedef int32_t (*t_ReadZStream)(void* stream, void* buffer, int32_t length);
typedef void* (*t_UnityPalGetLocalTimeZoneData)();
typedef void* (*t_UnityPalGetTimeZoneDataForID)(const char* id, int32_t* length);
typedef void* (*t_UnityPalTimeZoneInfoGetTimeZoneIDs)();
typedef bool (*t_UseUnityPalForTimeZoneInformation)();
typedef int32_t (*t_WriteZStream)(void* stream, void* buffer, int32_t length);

// Runtime / domain
typedef int32_t (*t_il2cpp_init)(const char* domain_name);
typedef int32_t (*t_il2cpp_init_utf16)(const Il2CppChar* domain_name);
typedef void (*t_il2cpp_shutdown)();
typedef void (*t_il2cpp_set_config_dir)(const char* config_path);
typedef void (*t_il2cpp_set_data_dir)(const char* data_path);
typedef void (*t_il2cpp_set_temp_dir)(const char* temp_path);
typedef void (*t_il2cpp_set_config)(const char* executablePath);
typedef void (*t_il2cpp_set_config_utf16)(const Il2CppChar* executablePath);
typedef void (*t_il2cpp_set_commandline_arguments)(int argc, const char* argv[], const char* basedir);
typedef void (*t_il2cpp_set_commandline_arguments_utf16)(int argc, const Il2CppChar* const argv[], const char* basedir);
typedef void (*t_il2cpp_set_memory_callbacks)(void* callbacks);
typedef void (*t_il2cpp_set_find_plugin_callback)(void* func);
typedef void (*t_il2cpp_set_default_thread_affinity)(int64_t affinity_mask);
typedef void (*t_il2cpp_register_log_callback)(Il2CppLogCallback method);
typedef const Il2CppImage* (*t_il2cpp_get_corlib)();
typedef void (*t_il2cpp_add_internal_call)(const char* name, Il2CppMethodPointer method);
typedef Il2CppMethodPointer (*t_il2cpp_resolve_icall)(const char* name);
typedef void* (*t_il2cpp_alloc)(size_t size);
typedef void (*t_il2cpp_free)(void* ptr);

typedef Il2CppDomain* (*t_il2cpp_domain_get)();
typedef const Il2CppAssembly* (*t_il2cpp_domain_assembly_open)(Il2CppDomain* domain, const char* name);
typedef const Il2CppAssembly** (*t_il2cpp_domain_get_assemblies)(const Il2CppDomain* domain, size_t* size);

typedef Il2CppThread* (*t_il2cpp_thread_current)();
typedef Il2CppThread* (*t_il2cpp_thread_attach)(Il2CppDomain* domain);
typedef void (*t_il2cpp_thread_detach)(Il2CppThread* thread);
typedef bool (*t_il2cpp_is_vm_thread)(Il2CppThread* thread);
typedef const Il2CppThread** (*t_il2cpp_thread_get_all_attached_threads)(size_t* size);
typedef void (*t_il2cpp_thread_walk_frame_stack)(Il2CppThread* thread, Il2CppFrameWalkFunc func, void* user_data);
typedef bool (*t_il2cpp_thread_get_top_frame)(Il2CppThread* thread, Il2CppStackFrameInfo* frame);
typedef bool (*t_il2cpp_thread_get_frame_at)(Il2CppThread* thread, int32_t offset, Il2CppStackFrameInfo* frame);
typedef int32_t (*t_il2cpp_thread_get_stack_depth)(Il2CppThread* thread);
typedef void (*t_il2cpp_current_thread_walk_frame_stack)(Il2CppFrameWalkFunc func, void* user_data);
typedef bool (*t_il2cpp_current_thread_get_top_frame)(Il2CppStackFrameInfo* frame);
typedef bool (*t_il2cpp_current_thread_get_frame_at)(int32_t offset, Il2CppStackFrameInfo* frame);
typedef int32_t (*t_il2cpp_current_thread_get_stack_depth)();

// Assemblies / images
typedef Il2CppImage* (*t_il2cpp_assembly_get_image)(const Il2CppAssembly* assembly);
typedef const char* (*t_il2cpp_image_get_name)(const Il2CppImage* image);
typedef const char* (*t_il2cpp_image_get_filename)(const Il2CppImage* image);
typedef const Il2CppAssembly* (*t_il2cpp_image_get_assembly)(const Il2CppImage* image);
typedef const MethodInfo* (*t_il2cpp_image_get_entry_point)(const Il2CppImage* image);
typedef size_t (*t_il2cpp_image_get_class_count)(const Il2CppImage* image);
typedef const Il2CppClass* (*t_il2cpp_image_get_class)(const Il2CppImage* image, size_t index);

// Classes
typedef Il2CppClass* (*t_il2cpp_class_from_name)(const Il2CppImage* image, const char* namespaze, const char* name);
typedef Il2CppClass* (*t_il2cpp_class_from_il2cpp_type)(const Il2CppType* type);
typedef Il2CppClass* (*t_il2cpp_class_from_type)(const Il2CppType* type);
typedef Il2CppClass* (*t_il2cpp_class_from_system_type)(Il2CppReflectionType* type);
typedef const char* (*t_il2cpp_class_get_name)(Il2CppClass* klass);
typedef const char* (*t_il2cpp_class_get_namespace)(Il2CppClass* klass);
typedef const char* (*t_il2cpp_class_get_assemblyname)(const Il2CppClass* klass);
typedef Il2CppClass* (*t_il2cpp_class_get_parent)(Il2CppClass* klass);
typedef Il2CppClass* (*t_il2cpp_class_get_declaring_type)(const Il2CppClass* klass);
typedef Il2CppClass* (*t_il2cpp_class_get_element_class)(Il2CppClass* klass);
typedef const Il2CppImage* (*t_il2cpp_class_get_image)(const Il2CppClass* klass);
typedef Il2CppType* (*t_il2cpp_class_get_type)(Il2CppClass* klass);
typedef const Il2CppType* (*t_il2cpp_class_enum_basetype)(Il2CppClass* klass);
typedef int32_t (*t_il2cpp_class_value_size)(Il2CppClass* klass, uint32_t* align);
typedef int32_t (*t_il2cpp_class_instance_size)(Il2CppClass* klass);
typedef int32_t (*t_il2cpp_class_array_element_size)(const Il2CppClass* klass);
typedef int32_t (*t_il2cpp_class_get_rank)(const Il2CppClass* klass);
typedef int32_t (*t_il2cpp_class_get_userdata_offset)();
typedef uint32_t (*t_il2cpp_class_num_fields)(const Il2CppClass* klass);
typedef uint32_t (*t_il2cpp_class_get_data_size)(const Il2CppClass* klass);
typedef uint32_t (*t_il2cpp_class_get_flags)(const Il2CppClass* klass);
typedef uint32_t (*t_il2cpp_class_get_type_token)(const Il2CppClass* klass);
typedef uint32_t (*t_il2cpp_class_get_bitmap_size)(const Il2CppClass* klass);
typedef void (*t_il2cpp_class_get_bitmap)(Il2CppClass* klass, uintptr_t* bitmap);
typedef void* (*t_il2cpp_class_get_static_field_data)(const Il2CppClass* klass);
typedef void (*t_il2cpp_class_set_userdata)(Il2CppClass* klass, void* userdata);
typedef void (*t_il2cpp_class_for_each)(Il2CppClassForEachFunc klassReportFunc, void* userData);
typedef bool (*t_il2cpp_class_is_enum)(const Il2CppClass* klass);
typedef bool (*t_il2cpp_class_is_valuetype)(const Il2CppClass* klass);
typedef bool (*t_il2cpp_class_is_interface)(const Il2CppClass* klass);
typedef bool (*t_il2cpp_class_is_abstract)(const Il2CppClass* klass);
typedef bool (*t_il2cpp_class_is_generic)(const Il2CppClass* klass);
typedef bool (*t_il2cpp_class_is_inflated)(const Il2CppClass* klass);
typedef bool (*t_il2cpp_class_is_blittable)(Il2CppClass* klass);
typedef bool (*t_il2cpp_class_has_references)(Il2CppClass* klass);
typedef bool (*t_il2cpp_class_is_assignable_from)(Il2CppClass* klass, Il2CppClass* oklass);
typedef bool (*t_il2cpp_class_is_subclass_of)(Il2CppClass* klass, Il2CppClass* klassc, bool check_interfaces);
typedef bool (*t_il2cpp_class_has_parent)(Il2CppClass* klass, Il2CppClass* klassc);
typedef bool (*t_il2cpp_class_has_attribute)(Il2CppClass* klass, Il2CppClass* attr_class);

// Members (methods / fields / properties / events)
typedef const MethodInfo* (*t_il2cpp_class_get_method_from_name)(Il2CppClass* klass, const char* name, int argsCount);
typedef const MethodInfo* (*t_il2cpp_class_get_methods)(Il2CppClass* klass, void** iter);
typedef FieldInfo* (*t_il2cpp_class_get_field_from_name)(Il2CppClass* klass, const char* name);
typedef FieldInfo* (*t_il2cpp_class_get_fields)(Il2CppClass* klass, void** iter);
typedef const PropertyInfo* (*t_il2cpp_class_get_properties)(Il2CppClass* klass, void** iter);
typedef const PropertyInfo* (*t_il2cpp_class_get_property_from_name)(Il2CppClass* klass, const char* name);
typedef const EventInfo* (*t_il2cpp_class_get_events)(Il2CppClass* klass, void** iter);
typedef Il2CppClass* (*t_il2cpp_class_get_interfaces)(Il2CppClass* klass, void** iter);
typedef const Il2CppClass* (*t_il2cpp_class_get_nested_types)(Il2CppClass* klass, void** iter);

// Objects / invocation
typedef Il2CppClass* (*t_il2cpp_object_get_class)(Il2CppObject* obj);
typedef uint32_t (*t_il2cpp_object_get_size)(Il2CppObject* obj);
typedef const MethodInfo* (*t_il2cpp_object_get_virtual_method)(Il2CppObject* obj, const MethodInfo* method);
typedef Il2CppObject* (*t_il2cpp_object_new)(Il2CppClass* klass);
typedef void* (*t_il2cpp_object_unbox)(Il2CppObject* obj);
typedef Il2CppObject* (*t_il2cpp_value_box)(Il2CppClass* klass, void* data);
typedef Il2CppObject* (*t_il2cpp_runtime_invoke)(const MethodInfo* method, void* obj, void** params, Il2CppException** exc);
typedef Il2CppObject* (*t_il2cpp_runtime_invoke_convert_args)(const MethodInfo* method, void* obj, Il2CppObject** params, int paramCount, Il2CppException** exc);
typedef void (*t_il2cpp_runtime_class_init)(Il2CppClass* klass);
typedef void (*t_il2cpp_runtime_object_init)(Il2CppObject* obj);
typedef void (*t_il2cpp_runtime_object_init_exception)(Il2CppObject* obj, Il2CppException** exc);
typedef void (*t_il2cpp_runtime_unhandled_exception_policy_set)(int32_t value);

// Exceptions
typedef void (*t_il2cpp_raise_exception)(Il2CppException* ex);
typedef Il2CppException* (*t_il2cpp_exception_from_name_msg)(const Il2CppImage* image, const char* name_space, const char* name, const char* msg);
typedef Il2CppException* (*t_il2cpp_get_exception_argument_null)(const char* arg);
typedef void (*t_il2cpp_format_exception)(const Il2CppException* ex, char* output, int message_size);
typedef void (*t_il2cpp_format_stack_trace)(const Il2CppException* ex, char* output, int output_size);
typedef void (*t_il2cpp_unhandled_exception)(Il2CppException* ex);

// Methods
typedef const char* (*t_il2cpp_method_get_name)(const MethodInfo* method);
typedef Il2CppClass* (*t_il2cpp_method_get_class)(const MethodInfo* method);
typedef Il2CppClass* (*t_il2cpp_method_get_declaring_type)(const MethodInfo* method);
typedef uint32_t (*t_il2cpp_method_get_param_count)(const MethodInfo* method);
typedef const Il2CppType* (*t_il2cpp_method_get_param)(const MethodInfo* method, uint32_t index);
typedef const char* (*t_il2cpp_method_get_param_name)(const MethodInfo* method, uint32_t index);
typedef const Il2CppType* (*t_il2cpp_method_get_return_type)(const MethodInfo* method);
typedef Il2CppReflectionMethod* (*t_il2cpp_method_get_object)(const MethodInfo* method, Il2CppClass* refclass);
typedef const MethodInfo* (*t_il2cpp_method_get_from_reflection)(const Il2CppReflectionMethod* method);
typedef uint32_t (*t_il2cpp_method_get_flags)(const MethodInfo* method, uint32_t* iflags);
typedef uint32_t (*t_il2cpp_method_get_token)(const MethodInfo* method);
typedef bool (*t_il2cpp_method_is_generic)(const MethodInfo* method);
typedef bool (*t_il2cpp_method_is_inflated)(const MethodInfo* method);
typedef bool (*t_il2cpp_method_is_instance)(const MethodInfo* method);
typedef bool (*t_il2cpp_method_has_attribute)(const MethodInfo* method, Il2CppClass* attr_class);

// Fields
typedef const char* (*t_il2cpp_field_get_name)(FieldInfo* field);
typedef int32_t (*t_il2cpp_field_get_offset)(FieldInfo* field);
typedef uint32_t (*t_il2cpp_field_get_flags)(FieldInfo* field);
typedef Il2CppClass* (*t_il2cpp_field_get_parent)(FieldInfo* field);
typedef const Il2CppType* (*t_il2cpp_field_get_type)(FieldInfo* field);
typedef void (*t_il2cpp_field_get_value)(Il2CppObject* obj, FieldInfo* field, void* value);
typedef Il2CppObject* (*t_il2cpp_field_get_value_object)(const Il2CppClass* klass, FieldInfo* field, Il2CppObject* obj);
typedef void (*t_il2cpp_field_set_value)(Il2CppObject* obj, FieldInfo* field, void* value);
typedef void (*t_il2cpp_field_set_value_object)(Il2CppObject* instance, FieldInfo* field, Il2CppObject* value);
typedef void (*t_il2cpp_field_static_get_value)(FieldInfo* field, void* value);
typedef void (*t_il2cpp_field_static_set_value)(FieldInfo* field, void* value);
typedef bool (*t_il2cpp_field_has_attribute)(FieldInfo* field, Il2CppClass* attr_class);
typedef bool (*t_il2cpp_field_is_literal)(FieldInfo* field);

// Properties
typedef uint32_t (*t_il2cpp_property_get_flags)(PropertyInfo* prop);
typedef const MethodInfo* (*t_il2cpp_property_get_get_method)(PropertyInfo* prop);
typedef const MethodInfo* (*t_il2cpp_property_get_set_method)(PropertyInfo* prop);
typedef const char* (*t_il2cpp_property_get_name)(PropertyInfo* prop);
typedef Il2CppClass* (*t_il2cpp_property_get_parent)(PropertyInfo* prop);

// Types
typedef Il2CppObject* (*t_il2cpp_type_get_object)(const Il2CppType* type);
typedef const char* (*t_il2cpp_type_get_name)(const Il2CppType* type);
typedef char* (*t_il2cpp_type_get_assembly_qualified_name)(const Il2CppType* type);
typedef void (*t_il2cpp_type_get_name_chunked)(const Il2CppType* type, Il2CppNameChunkFunc chunkReportFunc, void* userData);
typedef Il2CppTypeEnum (*t_il2cpp_type_get_type)(const Il2CppType* type);
typedef uint32_t (*t_il2cpp_type_get_attrs)(const Il2CppType* type);
typedef Il2CppClass* (*t_il2cpp_type_get_class_or_element_class)(const Il2CppType* type);
typedef bool (*t_il2cpp_type_is_static)(const Il2CppType* type);
typedef bool (*t_il2cpp_type_is_pointer_type)(const Il2CppType* type);
typedef bool (*t_il2cpp_type_is_byref)(const Il2CppType* type);
typedef bool (*t_il2cpp_type_equals)(const Il2CppType* type, const Il2CppType* otherType);

// Strings
typedef Il2CppString* (*t_il2cpp_string_new)(const char* str);
typedef Il2CppString* (*t_il2cpp_string_new_len)(const char* str, uint32_t length);
typedef Il2CppString* (*t_il2cpp_string_new_utf16)(const Il2CppChar* str, int32_t length);
typedef Il2CppString* (*t_il2cpp_string_new_wrapper)(const char* str);
typedef Il2CppString* (*t_il2cpp_string_intern)(Il2CppString* str);
typedef bool (*t_il2cpp_string_is_interned)(Il2CppString* str);
typedef uint32_t (*t_il2cpp_string_length)(Il2CppString* str);
typedef uint16_t* (*t_il2cpp_string_chars)(Il2CppString* str);

// Arrays
typedef Il2CppClass* (*t_il2cpp_array_class_get)(Il2CppClass* element_class, uint32_t rank);
typedef Il2CppClass* (*t_il2cpp_bounded_array_class_get)(Il2CppClass* element_class, uint32_t rank, bool bounded);
typedef Il2CppArray* (*t_il2cpp_array_new)(Il2CppClass* elementTypeInfo, uintptr_t length);
typedef Il2CppArray* (*t_il2cpp_array_new_specific)(Il2CppClass* arrayTypeInfo, uintptr_t length);
typedef Il2CppArray* (*t_il2cpp_array_new_full)(Il2CppClass* array_class, il2cpp_array_size_t* lengths, il2cpp_array_size_t* lower_bounds);
typedef uintptr_t (*t_il2cpp_array_length)(Il2CppArray* array);
typedef uint32_t (*t_il2cpp_array_get_byte_length)(Il2CppArray* array);
typedef int32_t (*t_il2cpp_array_element_size)(const Il2CppClass* arrayClass);
typedef int32_t (*t_il2cpp_array_object_header_size)();
typedef int32_t (*t_il2cpp_offset_of_array_length_in_array_object_header)();
typedef int32_t (*t_il2cpp_offset_of_array_bounds_in_array_object_header)();
typedef uint32_t (*t_il2cpp_allocation_granularity)();
typedef uint32_t (*t_il2cpp_object_header_size)();

// Monitors
typedef void (*t_il2cpp_monitor_enter)(Il2CppObject* obj);
typedef bool (*t_il2cpp_monitor_try_enter)(Il2CppObject* obj, uint32_t timeout);
typedef void (*t_il2cpp_monitor_exit)(Il2CppObject* obj);
typedef void (*t_il2cpp_monitor_pulse)(Il2CppObject* obj);
typedef void (*t_il2cpp_monitor_pulse_all)(Il2CppObject* obj);
typedef void (*t_il2cpp_monitor_wait)(Il2CppObject* obj);
typedef bool (*t_il2cpp_monitor_try_wait)(Il2CppObject* obj, uint32_t timeout);

// Garbage collector
typedef void (*t_il2cpp_gc_collect)(int maxGenerations);
typedef int32_t (*t_il2cpp_gc_collect_a_little)();
typedef void (*t_il2cpp_gc_disable)();
typedef void (*t_il2cpp_gc_enable)();
typedef bool (*t_il2cpp_gc_is_disabled)();
typedef bool (*t_il2cpp_gc_is_incremental)();
typedef int64_t (*t_il2cpp_gc_get_used_size)();
typedef int64_t (*t_il2cpp_gc_get_heap_size)();
typedef int64_t (*t_il2cpp_gc_get_max_time_slice_ns)();
typedef void (*t_il2cpp_gc_set_max_time_slice_ns)(int64_t maxTimeSlice);
typedef void (*t_il2cpp_gc_wbarrier_set_field)(Il2CppObject* obj, void** targetAddress, void* object);
typedef bool (*t_il2cpp_gc_has_strict_wbarriers)();
typedef void (*t_il2cpp_gc_set_external_allocation_tracker)(void* callback);
typedef void (*t_il2cpp_gc_set_external_wbarrier_tracker)(void* callback);
typedef void (*t_il2cpp_gc_foreach_heap)(Il2CppGCHeapFunc func, void* userData);
typedef void (*t_il2cpp_start_gc_world)();
typedef void (*t_il2cpp_stop_gc_world)();

// GC handles
typedef uint32_t (*t_il2cpp_gchandle_new)(Il2CppObject* obj, bool pinned);
typedef uint32_t (*t_il2cpp_gchandle_new_weakref)(Il2CppObject* obj, bool track_resurrection);
typedef Il2CppObject* (*t_il2cpp_gchandle_get_target)(uint32_t gchandle);
typedef void (*t_il2cpp_gchandle_free)(uint32_t gchandle);
typedef void (*t_il2cpp_gchandle_foreach_get_target)(Il2CppGCHeapFunc func, void* userData);

// Custom attributes
typedef Il2CppCustomAttrInfo* (*t_il2cpp_custom_attrs_from_class)(Il2CppClass* klass);
typedef Il2CppCustomAttrInfo* (*t_il2cpp_custom_attrs_from_method)(const MethodInfo* method);
typedef bool (*t_il2cpp_custom_attrs_has_attr)(Il2CppCustomAttrInfo* ainfo, Il2CppClass* attr_class);
typedef Il2CppObject* (*t_il2cpp_custom_attrs_get_attr)(Il2CppCustomAttrInfo* ainfo, Il2CppClass* attr_class);
typedef Il2CppArray* (*t_il2cpp_custom_attrs_construct)(Il2CppCustomAttrInfo* ainfo);
typedef void (*t_il2cpp_custom_attrs_free)(Il2CppCustomAttrInfo* ainfo);

// Debug / profiler
typedef Il2CppMemorySnapshot* (*t_il2cpp_capture_memory_snapshot)();
typedef void (*t_il2cpp_free_captured_memory_snapshot)(Il2CppMemorySnapshot* snapshot);
typedef void (*t_il2cpp_debugger_set_agent_options)(const char* options);
typedef bool (*t_il2cpp_is_debugger_attached)();
typedef bool (*t_il2cpp_register_debugger_agent_transport)(Il2CppDebuggerTransport* debuggerTransport);
typedef void (*t_il2cpp_debug_get_method_info)(const MethodInfo* method, Il2CppMethodDebugInfo* methodDebugInfo);
typedef void (*t_il2cpp_override_stack_backtrace)(Il2CppBacktraceFunc func);
typedef void (*t_il2cpp_profiler_install)(Il2CppProfiler* prof, Il2CppProfileFunc shutdown_callback);
typedef void (*t_il2cpp_profiler_set_events)(int32_t events);
typedef void (*t_il2cpp_profiler_install_enter_leave)(Il2CppProfileMethodFunc enter, Il2CppProfileMethodFunc fleave);
typedef void (*t_il2cpp_profiler_install_allocation)(Il2CppProfileAllocFunc callback);
typedef void (*t_il2cpp_profiler_install_gc)(Il2CppProfileGCFunc callback, Il2CppProfileGCResizeFunc heap_resize_callback);
typedef void (*t_il2cpp_profiler_install_fileio)(Il2CppProfileFileIOFunc callback);
typedef void (*t_il2cpp_profiler_install_thread)(Il2CppProfileThreadFunc start, Il2CppProfileThreadFunc end);

// Stats / misc
typedef void (*t_il2cpp_stats_dump_to_file)(const char* path);
typedef uint64_t (*t_il2cpp_stats_get_value)(int32_t stat);
typedef void (*t_il2cpp_unity_install_unitytls_interface)(const void* unitytlsInterfaceStruct);
typedef void (*t_il2cpp_unity_liveness_calculation_begin)(Il2CppObject* filter, int32_t max_object_count, Il2CppRegisterObjectFunc callback, void* userdata, Il2CppWorldChangedFunc onWorldStarted, Il2CppWorldChangedFunc onWorldStopped);
typedef void (*t_il2cpp_unity_liveness_calculation_end)(Il2CppObject* state);
typedef void (*t_il2cpp_unity_liveness_calculation_from_root)(Il2CppObject* root, Il2CppObject* state);
typedef void (*t_il2cpp_unity_liveness_calculation_from_statics)(Il2CppObject* state);

// Bound function pointers (resolved from GameAssembly.dll)
namespace Il2CppBindings {

// Non IL2CPP utility exports
inline t_CloseZStream CloseZStream = nullptr;
inline t_CreateZStream CreateZStream = nullptr;
inline t_DllCanUnloadNow DllCanUnloadNow = nullptr;
inline t_DllGetActivationFactory DllGetActivationFactory = nullptr;
inline t_Flush Flush = nullptr;
inline t_ReadZStream ReadZStream = nullptr;
inline t_UnityPalGetLocalTimeZoneData UnityPalGetLocalTimeZoneData = nullptr;
inline t_UnityPalGetTimeZoneDataForID UnityPalGetTimeZoneDataForID = nullptr;
inline t_UnityPalTimeZoneInfoGetTimeZoneIDs UnityPalTimeZoneInfoGetTimeZoneIDs = nullptr;
inline t_UseUnityPalForTimeZoneInformation UseUnityPalForTimeZoneInformation = nullptr;
inline t_WriteZStream WriteZStream = nullptr;

// Runtime / domain
inline t_il2cpp_init il2cpp_init = nullptr;
inline t_il2cpp_init_utf16 il2cpp_init_utf16 = nullptr;
inline t_il2cpp_shutdown il2cpp_shutdown = nullptr;
inline t_il2cpp_set_config_dir il2cpp_set_config_dir = nullptr;
inline t_il2cpp_set_data_dir il2cpp_set_data_dir = nullptr;
inline t_il2cpp_set_temp_dir il2cpp_set_temp_dir = nullptr;
inline t_il2cpp_set_config il2cpp_set_config = nullptr;
inline t_il2cpp_set_config_utf16 il2cpp_set_config_utf16 = nullptr;
inline t_il2cpp_set_commandline_arguments il2cpp_set_commandline_arguments = nullptr;
inline t_il2cpp_set_commandline_arguments_utf16 il2cpp_set_commandline_arguments_utf16 = nullptr;
inline t_il2cpp_set_memory_callbacks il2cpp_set_memory_callbacks = nullptr;
inline t_il2cpp_set_find_plugin_callback il2cpp_set_find_plugin_callback = nullptr;
inline t_il2cpp_set_default_thread_affinity il2cpp_set_default_thread_affinity = nullptr;
inline t_il2cpp_register_log_callback il2cpp_register_log_callback = nullptr;
inline t_il2cpp_get_corlib il2cpp_get_corlib = nullptr;
inline t_il2cpp_add_internal_call il2cpp_add_internal_call = nullptr;
inline t_il2cpp_resolve_icall il2cpp_resolve_icall = nullptr;
inline t_il2cpp_alloc il2cpp_alloc = nullptr;
inline t_il2cpp_free il2cpp_free = nullptr;

inline t_il2cpp_domain_get il2cpp_domain_get = nullptr;
inline t_il2cpp_domain_assembly_open il2cpp_domain_assembly_open = nullptr;
inline t_il2cpp_domain_get_assemblies il2cpp_domain_get_assemblies = nullptr;

inline t_il2cpp_thread_current il2cpp_thread_current = nullptr;
inline t_il2cpp_thread_attach il2cpp_thread_attach = nullptr;
inline t_il2cpp_thread_detach il2cpp_thread_detach = nullptr;
inline t_il2cpp_is_vm_thread il2cpp_is_vm_thread = nullptr;
inline t_il2cpp_thread_get_all_attached_threads il2cpp_thread_get_all_attached_threads = nullptr;
inline t_il2cpp_thread_walk_frame_stack il2cpp_thread_walk_frame_stack = nullptr;
inline t_il2cpp_thread_get_top_frame il2cpp_thread_get_top_frame = nullptr;
inline t_il2cpp_thread_get_frame_at il2cpp_thread_get_frame_at = nullptr;
inline t_il2cpp_thread_get_stack_depth il2cpp_thread_get_stack_depth = nullptr;
inline t_il2cpp_current_thread_walk_frame_stack il2cpp_current_thread_walk_frame_stack = nullptr;
inline t_il2cpp_current_thread_get_top_frame il2cpp_current_thread_get_top_frame = nullptr;
inline t_il2cpp_current_thread_get_frame_at il2cpp_current_thread_get_frame_at = nullptr;
inline t_il2cpp_current_thread_get_stack_depth il2cpp_current_thread_get_stack_depth = nullptr;

// Assemblies / images
inline t_il2cpp_assembly_get_image il2cpp_assembly_get_image = nullptr;
inline t_il2cpp_image_get_name il2cpp_image_get_name = nullptr;
inline t_il2cpp_image_get_filename il2cpp_image_get_filename = nullptr;
inline t_il2cpp_image_get_assembly il2cpp_image_get_assembly = nullptr;
inline t_il2cpp_image_get_entry_point il2cpp_image_get_entry_point = nullptr;
inline t_il2cpp_image_get_class_count il2cpp_image_get_class_count = nullptr;
inline t_il2cpp_image_get_class il2cpp_image_get_class = nullptr;

// Classes
inline t_il2cpp_class_from_name il2cpp_class_from_name = nullptr;
inline t_il2cpp_class_from_il2cpp_type il2cpp_class_from_il2cpp_type = nullptr;
inline t_il2cpp_class_from_type il2cpp_class_from_type = nullptr;
inline t_il2cpp_class_from_system_type il2cpp_class_from_system_type = nullptr;
inline t_il2cpp_class_get_name il2cpp_class_get_name = nullptr;
inline t_il2cpp_class_get_namespace il2cpp_class_get_namespace = nullptr;
inline t_il2cpp_class_get_assemblyname il2cpp_class_get_assemblyname = nullptr;
inline t_il2cpp_class_get_parent il2cpp_class_get_parent = nullptr;
inline t_il2cpp_class_get_declaring_type il2cpp_class_get_declaring_type = nullptr;
inline t_il2cpp_class_get_element_class il2cpp_class_get_element_class = nullptr;
inline t_il2cpp_class_get_image il2cpp_class_get_image = nullptr;
inline t_il2cpp_class_get_type il2cpp_class_get_type = nullptr;
inline t_il2cpp_class_enum_basetype il2cpp_class_enum_basetype = nullptr;
inline t_il2cpp_class_value_size il2cpp_class_value_size = nullptr;
inline t_il2cpp_class_instance_size il2cpp_class_instance_size = nullptr;
inline t_il2cpp_class_array_element_size il2cpp_class_array_element_size = nullptr;
inline t_il2cpp_class_get_rank il2cpp_class_get_rank = nullptr;
inline t_il2cpp_class_get_userdata_offset il2cpp_class_get_userdata_offset = nullptr;
inline t_il2cpp_class_num_fields il2cpp_class_num_fields = nullptr;
inline t_il2cpp_class_get_data_size il2cpp_class_get_data_size = nullptr;
inline t_il2cpp_class_get_flags il2cpp_class_get_flags = nullptr;
inline t_il2cpp_class_get_type_token il2cpp_class_get_type_token = nullptr;
inline t_il2cpp_class_get_bitmap_size il2cpp_class_get_bitmap_size = nullptr;
inline t_il2cpp_class_get_bitmap il2cpp_class_get_bitmap = nullptr;
inline t_il2cpp_class_get_static_field_data il2cpp_class_get_static_field_data = nullptr;
inline t_il2cpp_class_set_userdata il2cpp_class_set_userdata = nullptr;
inline t_il2cpp_class_for_each il2cpp_class_for_each = nullptr;
inline t_il2cpp_class_is_enum il2cpp_class_is_enum = nullptr;
inline t_il2cpp_class_is_valuetype il2cpp_class_is_valuetype = nullptr;
inline t_il2cpp_class_is_interface il2cpp_class_is_interface = nullptr;
inline t_il2cpp_class_is_abstract il2cpp_class_is_abstract = nullptr;
inline t_il2cpp_class_is_generic il2cpp_class_is_generic = nullptr;
inline t_il2cpp_class_is_inflated il2cpp_class_is_inflated = nullptr;
inline t_il2cpp_class_is_blittable il2cpp_class_is_blittable = nullptr;
inline t_il2cpp_class_has_references il2cpp_class_has_references = nullptr;
inline t_il2cpp_class_is_assignable_from il2cpp_class_is_assignable_from = nullptr;
inline t_il2cpp_class_is_subclass_of il2cpp_class_is_subclass_of = nullptr;
inline t_il2cpp_class_has_parent il2cpp_class_has_parent = nullptr;
inline t_il2cpp_class_has_attribute il2cpp_class_has_attribute = nullptr;

// Members (methods / fields / properties / events)
inline t_il2cpp_class_get_method_from_name il2cpp_class_get_method_from_name = nullptr;
inline t_il2cpp_class_get_methods il2cpp_class_get_methods = nullptr;
inline t_il2cpp_class_get_field_from_name il2cpp_class_get_field_from_name = nullptr;
inline t_il2cpp_class_get_fields il2cpp_class_get_fields = nullptr;
inline t_il2cpp_class_get_properties il2cpp_class_get_properties = nullptr;
inline t_il2cpp_class_get_property_from_name il2cpp_class_get_property_from_name = nullptr;
inline t_il2cpp_class_get_events il2cpp_class_get_events = nullptr;
inline t_il2cpp_class_get_interfaces il2cpp_class_get_interfaces = nullptr;
inline t_il2cpp_class_get_nested_types il2cpp_class_get_nested_types = nullptr;

// Objects / invocation
inline t_il2cpp_object_get_class il2cpp_object_get_class = nullptr;
inline t_il2cpp_object_get_size il2cpp_object_get_size = nullptr;
inline t_il2cpp_object_get_virtual_method il2cpp_object_get_virtual_method = nullptr;
inline t_il2cpp_object_new il2cpp_object_new = nullptr;
inline t_il2cpp_object_unbox il2cpp_object_unbox = nullptr;
inline t_il2cpp_value_box il2cpp_value_box = nullptr;
inline t_il2cpp_runtime_invoke il2cpp_runtime_invoke = nullptr;
inline t_il2cpp_runtime_invoke_convert_args il2cpp_runtime_invoke_convert_args = nullptr;
inline t_il2cpp_runtime_class_init il2cpp_runtime_class_init = nullptr;
inline t_il2cpp_runtime_object_init il2cpp_runtime_object_init = nullptr;
inline t_il2cpp_runtime_object_init_exception il2cpp_runtime_object_init_exception = nullptr;
inline t_il2cpp_runtime_unhandled_exception_policy_set il2cpp_runtime_unhandled_exception_policy_set = nullptr;

// Exceptions
inline t_il2cpp_raise_exception il2cpp_raise_exception = nullptr;
inline t_il2cpp_exception_from_name_msg il2cpp_exception_from_name_msg = nullptr;
inline t_il2cpp_get_exception_argument_null il2cpp_get_exception_argument_null = nullptr;
inline t_il2cpp_format_exception il2cpp_format_exception = nullptr;
inline t_il2cpp_format_stack_trace il2cpp_format_stack_trace = nullptr;
inline t_il2cpp_unhandled_exception il2cpp_unhandled_exception = nullptr;

// Methods
inline t_il2cpp_method_get_name il2cpp_method_get_name = nullptr;
inline t_il2cpp_method_get_class il2cpp_method_get_class = nullptr;
inline t_il2cpp_method_get_declaring_type il2cpp_method_get_declaring_type = nullptr;
inline t_il2cpp_method_get_param_count il2cpp_method_get_param_count = nullptr;
inline t_il2cpp_method_get_param il2cpp_method_get_param = nullptr;
inline t_il2cpp_method_get_param_name il2cpp_method_get_param_name = nullptr;
inline t_il2cpp_method_get_return_type il2cpp_method_get_return_type = nullptr;
inline t_il2cpp_method_get_object il2cpp_method_get_object = nullptr;
inline t_il2cpp_method_get_from_reflection il2cpp_method_get_from_reflection = nullptr;
inline t_il2cpp_method_get_flags il2cpp_method_get_flags = nullptr;
inline t_il2cpp_method_get_token il2cpp_method_get_token = nullptr;
inline t_il2cpp_method_is_generic il2cpp_method_is_generic = nullptr;
inline t_il2cpp_method_is_inflated il2cpp_method_is_inflated = nullptr;
inline t_il2cpp_method_is_instance il2cpp_method_is_instance = nullptr;
inline t_il2cpp_method_has_attribute il2cpp_method_has_attribute = nullptr;

// Fields
inline t_il2cpp_field_get_name il2cpp_field_get_name = nullptr;
inline t_il2cpp_field_get_offset il2cpp_field_get_offset = nullptr;
inline t_il2cpp_field_get_flags il2cpp_field_get_flags = nullptr;
inline t_il2cpp_field_get_parent il2cpp_field_get_parent = nullptr;
inline t_il2cpp_field_get_type il2cpp_field_get_type = nullptr;
inline t_il2cpp_field_get_value il2cpp_field_get_value = nullptr;
inline t_il2cpp_field_get_value_object il2cpp_field_get_value_object = nullptr;
inline t_il2cpp_field_set_value il2cpp_field_set_value = nullptr;
inline t_il2cpp_field_set_value_object il2cpp_field_set_value_object = nullptr;
inline t_il2cpp_field_static_get_value il2cpp_field_static_get_value = nullptr;
inline t_il2cpp_field_static_set_value il2cpp_field_static_set_value = nullptr;
inline t_il2cpp_field_has_attribute il2cpp_field_has_attribute = nullptr;
inline t_il2cpp_field_is_literal il2cpp_field_is_literal = nullptr;

// Properties
inline t_il2cpp_property_get_flags il2cpp_property_get_flags = nullptr;
inline t_il2cpp_property_get_get_method il2cpp_property_get_get_method = nullptr;
inline t_il2cpp_property_get_set_method il2cpp_property_get_set_method = nullptr;
inline t_il2cpp_property_get_name il2cpp_property_get_name = nullptr;
inline t_il2cpp_property_get_parent il2cpp_property_get_parent = nullptr;

// Types
inline t_il2cpp_type_get_object il2cpp_type_get_object = nullptr;
inline t_il2cpp_type_get_name il2cpp_type_get_name = nullptr;
inline t_il2cpp_type_get_assembly_qualified_name il2cpp_type_get_assembly_qualified_name = nullptr;
inline t_il2cpp_type_get_name_chunked il2cpp_type_get_name_chunked = nullptr;
inline t_il2cpp_type_get_type il2cpp_type_get_type = nullptr;
inline t_il2cpp_type_get_attrs il2cpp_type_get_attrs = nullptr;
inline t_il2cpp_type_get_class_or_element_class il2cpp_type_get_class_or_element_class = nullptr;
inline t_il2cpp_type_is_static il2cpp_type_is_static = nullptr;
inline t_il2cpp_type_is_pointer_type il2cpp_type_is_pointer_type = nullptr;
inline t_il2cpp_type_is_byref il2cpp_type_is_byref = nullptr;
inline t_il2cpp_type_equals il2cpp_type_equals = nullptr;

// Strings
inline t_il2cpp_string_new il2cpp_string_new = nullptr;
inline t_il2cpp_string_new_len il2cpp_string_new_len = nullptr;
inline t_il2cpp_string_new_utf16 il2cpp_string_new_utf16 = nullptr;
inline t_il2cpp_string_new_wrapper il2cpp_string_new_wrapper = nullptr;
inline t_il2cpp_string_intern il2cpp_string_intern = nullptr;
inline t_il2cpp_string_is_interned il2cpp_string_is_interned = nullptr;
inline t_il2cpp_string_length il2cpp_string_length = nullptr;
inline t_il2cpp_string_chars il2cpp_string_chars = nullptr;

// Arrays
inline t_il2cpp_array_class_get il2cpp_array_class_get = nullptr;
inline t_il2cpp_bounded_array_class_get il2cpp_bounded_array_class_get = nullptr;
inline t_il2cpp_array_new il2cpp_array_new = nullptr;
inline t_il2cpp_array_new_specific il2cpp_array_new_specific = nullptr;
inline t_il2cpp_array_new_full il2cpp_array_new_full = nullptr;
inline t_il2cpp_array_length il2cpp_array_length = nullptr;
inline t_il2cpp_array_get_byte_length il2cpp_array_get_byte_length = nullptr;
inline t_il2cpp_array_element_size il2cpp_array_element_size = nullptr;
inline t_il2cpp_array_object_header_size il2cpp_array_object_header_size = nullptr;
inline t_il2cpp_offset_of_array_length_in_array_object_header il2cpp_offset_of_array_length_in_array_object_header = nullptr;
inline t_il2cpp_offset_of_array_bounds_in_array_object_header il2cpp_offset_of_array_bounds_in_array_object_header = nullptr;
inline t_il2cpp_allocation_granularity il2cpp_allocation_granularity = nullptr;
inline t_il2cpp_object_header_size il2cpp_object_header_size = nullptr;

// Monitors
inline t_il2cpp_monitor_enter il2cpp_monitor_enter = nullptr;
inline t_il2cpp_monitor_try_enter il2cpp_monitor_try_enter = nullptr;
inline t_il2cpp_monitor_exit il2cpp_monitor_exit = nullptr;
inline t_il2cpp_monitor_pulse il2cpp_monitor_pulse = nullptr;
inline t_il2cpp_monitor_pulse_all il2cpp_monitor_pulse_all = nullptr;
inline t_il2cpp_monitor_wait il2cpp_monitor_wait = nullptr;
inline t_il2cpp_monitor_try_wait il2cpp_monitor_try_wait = nullptr;

// Garbage collector
inline t_il2cpp_gc_collect il2cpp_gc_collect = nullptr;
inline t_il2cpp_gc_collect_a_little il2cpp_gc_collect_a_little = nullptr;
inline t_il2cpp_gc_disable il2cpp_gc_disable = nullptr;
inline t_il2cpp_gc_enable il2cpp_gc_enable = nullptr;
inline t_il2cpp_gc_is_disabled il2cpp_gc_is_disabled = nullptr;
inline t_il2cpp_gc_is_incremental il2cpp_gc_is_incremental = nullptr;
inline t_il2cpp_gc_get_used_size il2cpp_gc_get_used_size = nullptr;
inline t_il2cpp_gc_get_heap_size il2cpp_gc_get_heap_size = nullptr;
inline t_il2cpp_gc_get_max_time_slice_ns il2cpp_gc_get_max_time_slice_ns = nullptr;
inline t_il2cpp_gc_set_max_time_slice_ns il2cpp_gc_set_max_time_slice_ns = nullptr;
inline t_il2cpp_gc_wbarrier_set_field il2cpp_gc_wbarrier_set_field = nullptr;
inline t_il2cpp_gc_has_strict_wbarriers il2cpp_gc_has_strict_wbarriers = nullptr;
inline t_il2cpp_gc_set_external_allocation_tracker il2cpp_gc_set_external_allocation_tracker = nullptr;
inline t_il2cpp_gc_set_external_wbarrier_tracker il2cpp_gc_set_external_wbarrier_tracker = nullptr;
inline t_il2cpp_gc_foreach_heap il2cpp_gc_foreach_heap = nullptr;
inline t_il2cpp_start_gc_world il2cpp_start_gc_world = nullptr;
inline t_il2cpp_stop_gc_world il2cpp_stop_gc_world = nullptr;

// GC handles
inline t_il2cpp_gchandle_new il2cpp_gchandle_new = nullptr;
inline t_il2cpp_gchandle_new_weakref il2cpp_gchandle_new_weakref = nullptr;
inline t_il2cpp_gchandle_get_target il2cpp_gchandle_get_target = nullptr;
inline t_il2cpp_gchandle_free il2cpp_gchandle_free = nullptr;
inline t_il2cpp_gchandle_foreach_get_target il2cpp_gchandle_foreach_get_target = nullptr;

// Custom attributes
inline t_il2cpp_custom_attrs_from_class il2cpp_custom_attrs_from_class = nullptr;
inline t_il2cpp_custom_attrs_from_method il2cpp_custom_attrs_from_method = nullptr;
inline t_il2cpp_custom_attrs_has_attr il2cpp_custom_attrs_has_attr = nullptr;
inline t_il2cpp_custom_attrs_get_attr il2cpp_custom_attrs_get_attr = nullptr;
inline t_il2cpp_custom_attrs_construct il2cpp_custom_attrs_construct = nullptr;
inline t_il2cpp_custom_attrs_free il2cpp_custom_attrs_free = nullptr;

// Debug / profiler
inline t_il2cpp_capture_memory_snapshot il2cpp_capture_memory_snapshot = nullptr;
inline t_il2cpp_free_captured_memory_snapshot il2cpp_free_captured_memory_snapshot = nullptr;
inline t_il2cpp_debugger_set_agent_options il2cpp_debugger_set_agent_options = nullptr;
inline t_il2cpp_is_debugger_attached il2cpp_is_debugger_attached = nullptr;
inline t_il2cpp_register_debugger_agent_transport il2cpp_register_debugger_agent_transport = nullptr;
inline t_il2cpp_debug_get_method_info il2cpp_debug_get_method_info = nullptr;
inline t_il2cpp_override_stack_backtrace il2cpp_override_stack_backtrace = nullptr;
inline t_il2cpp_profiler_install il2cpp_profiler_install = nullptr;
inline t_il2cpp_profiler_set_events il2cpp_profiler_set_events = nullptr;
inline t_il2cpp_profiler_install_enter_leave il2cpp_profiler_install_enter_leave = nullptr;
inline t_il2cpp_profiler_install_allocation il2cpp_profiler_install_allocation = nullptr;
inline t_il2cpp_profiler_install_gc il2cpp_profiler_install_gc = nullptr;
inline t_il2cpp_profiler_install_fileio il2cpp_profiler_install_fileio = nullptr;
inline t_il2cpp_profiler_install_thread il2cpp_profiler_install_thread = nullptr;

// Stats / misc
inline t_il2cpp_stats_dump_to_file il2cpp_stats_dump_to_file = nullptr;
inline t_il2cpp_stats_get_value il2cpp_stats_get_value = nullptr;
inline t_il2cpp_unity_install_unitytls_interface il2cpp_unity_install_unitytls_interface = nullptr;
inline t_il2cpp_unity_liveness_calculation_begin il2cpp_unity_liveness_calculation_begin = nullptr;
inline t_il2cpp_unity_liveness_calculation_end il2cpp_unity_liveness_calculation_end = nullptr;
inline t_il2cpp_unity_liveness_calculation_from_root il2cpp_unity_liveness_calculation_from_root = nullptr;
inline t_il2cpp_unity_liveness_calculation_from_statics il2cpp_unity_liveness_calculation_from_statics = nullptr;

// Resolves every export through the supplied binder. Required entries fail the
// load when missing, the rest are best effort.
template <typename BindFn>
inline bool BindAll(BindFn bind) {
    bool ok = true;

    // Required for the wrapper to function.
    ok &= bind("il2cpp_domain_get", il2cpp_domain_get, true);
    ok &= bind("il2cpp_thread_attach", il2cpp_thread_attach, true);
    ok &= bind("il2cpp_domain_get_assemblies", il2cpp_domain_get_assemblies, true);
    ok &= bind("il2cpp_assembly_get_image", il2cpp_assembly_get_image, true);
    ok &= bind("il2cpp_image_get_name", il2cpp_image_get_name, true);
    ok &= bind("il2cpp_class_from_name", il2cpp_class_from_name, true);
    ok &= bind("il2cpp_class_from_il2cpp_type", il2cpp_class_from_il2cpp_type, true);
    ok &= bind("il2cpp_class_get_name", il2cpp_class_get_name, true);
    ok &= bind("il2cpp_class_get_namespace", il2cpp_class_get_namespace, true);
    ok &= bind("il2cpp_class_get_parent", il2cpp_class_get_parent, true);
    ok &= bind("il2cpp_class_get_type", il2cpp_class_get_type, true);
    ok &= bind("il2cpp_class_get_method_from_name", il2cpp_class_get_method_from_name, true);
    ok &= bind("il2cpp_class_get_methods", il2cpp_class_get_methods, true);
    ok &= bind("il2cpp_method_get_name", il2cpp_method_get_name, true);
    ok &= bind("il2cpp_object_get_class", il2cpp_object_get_class, true);
    ok &= bind("il2cpp_runtime_invoke", il2cpp_runtime_invoke, true);
    ok &= bind("il2cpp_class_get_field_from_name", il2cpp_class_get_field_from_name, true);
    ok &= bind("il2cpp_class_get_fields", il2cpp_class_get_fields, true);
    ok &= bind("il2cpp_field_get_name", il2cpp_field_get_name, true);
    ok &= bind("il2cpp_field_get_value", il2cpp_field_get_value, true);
    ok &= bind("il2cpp_field_set_value", il2cpp_field_set_value, true);
    ok &= bind("il2cpp_field_static_get_value", il2cpp_field_static_get_value, true);
    ok &= bind("il2cpp_array_new", il2cpp_array_new, true);

    // Non IL2CPP utility exports (best effort).
    ok &= bind("CloseZStream", CloseZStream, false);
    ok &= bind("CreateZStream", CreateZStream, false);
    ok &= bind("DllCanUnloadNow", DllCanUnloadNow, false);
    ok &= bind("DllGetActivationFactory", DllGetActivationFactory, false);
    ok &= bind("Flush", Flush, false);
    ok &= bind("ReadZStream", ReadZStream, false);
    ok &= bind("UnityPalGetLocalTimeZoneData", UnityPalGetLocalTimeZoneData, false);
    ok &= bind("UnityPalGetTimeZoneDataForID", UnityPalGetTimeZoneDataForID, false);
    ok &= bind("UnityPalTimeZoneInfoGetTimeZoneIDs", UnityPalTimeZoneInfoGetTimeZoneIDs, false);
    ok &= bind("UseUnityPalForTimeZoneInformation", UseUnityPalForTimeZoneInformation, false);
    ok &= bind("WriteZStream", WriteZStream, false);

    // Optional IL2CPP exports.
    ok &= bind("il2cpp_init", il2cpp_init, false);
    ok &= bind("il2cpp_init_utf16", il2cpp_init_utf16, false);
    ok &= bind("il2cpp_shutdown", il2cpp_shutdown, false);
    ok &= bind("il2cpp_set_config_dir", il2cpp_set_config_dir, false);
    ok &= bind("il2cpp_set_data_dir", il2cpp_set_data_dir, false);
    ok &= bind("il2cpp_set_temp_dir", il2cpp_set_temp_dir, false);
    ok &= bind("il2cpp_set_config", il2cpp_set_config, false);
    ok &= bind("il2cpp_set_config_utf16", il2cpp_set_config_utf16, false);
    ok &= bind("il2cpp_set_commandline_arguments", il2cpp_set_commandline_arguments, false);
    ok &= bind("il2cpp_set_commandline_arguments_utf16", il2cpp_set_commandline_arguments_utf16, false);
    ok &= bind("il2cpp_set_memory_callbacks", il2cpp_set_memory_callbacks, false);
    ok &= bind("il2cpp_set_find_plugin_callback", il2cpp_set_find_plugin_callback, false);
    ok &= bind("il2cpp_set_default_thread_affinity", il2cpp_set_default_thread_affinity, false);
    ok &= bind("il2cpp_register_log_callback", il2cpp_register_log_callback, false);
    ok &= bind("il2cpp_get_corlib", il2cpp_get_corlib, false);
    ok &= bind("il2cpp_add_internal_call", il2cpp_add_internal_call, false);
    ok &= bind("il2cpp_resolve_icall", il2cpp_resolve_icall, false);
    ok &= bind("il2cpp_alloc", il2cpp_alloc, false);
    ok &= bind("il2cpp_free", il2cpp_free, false);

    ok &= bind("il2cpp_thread_detach", il2cpp_thread_detach, false);
    ok &= bind("il2cpp_domain_assembly_open", il2cpp_domain_assembly_open, false);
    ok &= bind("il2cpp_thread_current", il2cpp_thread_current, false);
    ok &= bind("il2cpp_is_vm_thread", il2cpp_is_vm_thread, false);
    ok &= bind("il2cpp_thread_get_all_attached_threads", il2cpp_thread_get_all_attached_threads, false);
    ok &= bind("il2cpp_thread_walk_frame_stack", il2cpp_thread_walk_frame_stack, false);
    ok &= bind("il2cpp_thread_get_top_frame", il2cpp_thread_get_top_frame, false);
    ok &= bind("il2cpp_thread_get_frame_at", il2cpp_thread_get_frame_at, false);
    ok &= bind("il2cpp_thread_get_stack_depth", il2cpp_thread_get_stack_depth, false);
    ok &= bind("il2cpp_current_thread_walk_frame_stack", il2cpp_current_thread_walk_frame_stack, false);
    ok &= bind("il2cpp_current_thread_get_top_frame", il2cpp_current_thread_get_top_frame, false);
    ok &= bind("il2cpp_current_thread_get_frame_at", il2cpp_current_thread_get_frame_at, false);
    ok &= bind("il2cpp_current_thread_get_stack_depth", il2cpp_current_thread_get_stack_depth, false);

    ok &= bind("il2cpp_image_get_class_count", il2cpp_image_get_class_count, false);
    ok &= bind("il2cpp_image_get_class", il2cpp_image_get_class, false);
    ok &= bind("il2cpp_image_get_filename", il2cpp_image_get_filename, false);
    ok &= bind("il2cpp_image_get_assembly", il2cpp_image_get_assembly, false);
    ok &= bind("il2cpp_image_get_entry_point", il2cpp_image_get_entry_point, false);

    ok &= bind("il2cpp_class_from_type", il2cpp_class_from_type, false);
    ok &= bind("il2cpp_class_from_system_type", il2cpp_class_from_system_type, false);
    ok &= bind("il2cpp_class_get_declaring_type", il2cpp_class_get_declaring_type, false);
    ok &= bind("il2cpp_class_get_element_class", il2cpp_class_get_element_class, false);
    ok &= bind("il2cpp_class_get_assemblyname", il2cpp_class_get_assemblyname, false);
    ok &= bind("il2cpp_class_get_image", il2cpp_class_get_image, false);
    ok &= bind("il2cpp_class_enum_basetype", il2cpp_class_enum_basetype, false);
    ok &= bind("il2cpp_class_value_size", il2cpp_class_value_size, false);
    ok &= bind("il2cpp_class_instance_size", il2cpp_class_instance_size, false);
    ok &= bind("il2cpp_class_array_element_size", il2cpp_class_array_element_size, false);
    ok &= bind("il2cpp_class_get_rank", il2cpp_class_get_rank, false);
    ok &= bind("il2cpp_class_get_userdata_offset", il2cpp_class_get_userdata_offset, false);
    ok &= bind("il2cpp_class_num_fields", il2cpp_class_num_fields, false);
    ok &= bind("il2cpp_class_get_data_size", il2cpp_class_get_data_size, false);
    ok &= bind("il2cpp_class_get_flags", il2cpp_class_get_flags, false);
    ok &= bind("il2cpp_class_get_type_token", il2cpp_class_get_type_token, false);
    ok &= bind("il2cpp_class_get_bitmap_size", il2cpp_class_get_bitmap_size, false);
    ok &= bind("il2cpp_class_get_bitmap", il2cpp_class_get_bitmap, false);
    ok &= bind("il2cpp_class_get_static_field_data", il2cpp_class_get_static_field_data, false);
    ok &= bind("il2cpp_class_set_userdata", il2cpp_class_set_userdata, false);
    ok &= bind("il2cpp_class_for_each", il2cpp_class_for_each, false);
    ok &= bind("il2cpp_class_is_enum", il2cpp_class_is_enum, false);
    ok &= bind("il2cpp_class_is_valuetype", il2cpp_class_is_valuetype, false);
    ok &= bind("il2cpp_class_is_interface", il2cpp_class_is_interface, false);
    ok &= bind("il2cpp_class_is_abstract", il2cpp_class_is_abstract, false);
    ok &= bind("il2cpp_class_is_generic", il2cpp_class_is_generic, false);
    ok &= bind("il2cpp_class_is_inflated", il2cpp_class_is_inflated, false);
    ok &= bind("il2cpp_class_is_blittable", il2cpp_class_is_blittable, false);
    ok &= bind("il2cpp_class_has_references", il2cpp_class_has_references, false);
    ok &= bind("il2cpp_class_is_assignable_from", il2cpp_class_is_assignable_from, false);
    ok &= bind("il2cpp_class_is_subclass_of", il2cpp_class_is_subclass_of, false);
    ok &= bind("il2cpp_class_has_parent", il2cpp_class_has_parent, false);
    ok &= bind("il2cpp_class_has_attribute", il2cpp_class_has_attribute, false);

    ok &= bind("il2cpp_class_get_properties", il2cpp_class_get_properties, false);
    ok &= bind("il2cpp_class_get_property_from_name", il2cpp_class_get_property_from_name, false);
    ok &= bind("il2cpp_class_get_events", il2cpp_class_get_events, false);
    ok &= bind("il2cpp_class_get_interfaces", il2cpp_class_get_interfaces, false);
    ok &= bind("il2cpp_class_get_nested_types", il2cpp_class_get_nested_types, false);

    ok &= bind("il2cpp_object_get_size", il2cpp_object_get_size, false);
    ok &= bind("il2cpp_object_get_virtual_method", il2cpp_object_get_virtual_method, false);
    ok &= bind("il2cpp_object_new", il2cpp_object_new, false);
    ok &= bind("il2cpp_object_unbox", il2cpp_object_unbox, false);
    ok &= bind("il2cpp_value_box", il2cpp_value_box, false);
    ok &= bind("il2cpp_runtime_invoke_convert_args", il2cpp_runtime_invoke_convert_args, false);
    ok &= bind("il2cpp_runtime_class_init", il2cpp_runtime_class_init, false);
    ok &= bind("il2cpp_runtime_object_init", il2cpp_runtime_object_init, false);
    ok &= bind("il2cpp_runtime_object_init_exception", il2cpp_runtime_object_init_exception, false);
    ok &= bind("il2cpp_runtime_unhandled_exception_policy_set", il2cpp_runtime_unhandled_exception_policy_set, false);

    ok &= bind("il2cpp_raise_exception", il2cpp_raise_exception, false);
    ok &= bind("il2cpp_exception_from_name_msg", il2cpp_exception_from_name_msg, false);
    ok &= bind("il2cpp_get_exception_argument_null", il2cpp_get_exception_argument_null, false);
    ok &= bind("il2cpp_format_exception", il2cpp_format_exception, false);
    ok &= bind("il2cpp_format_stack_trace", il2cpp_format_stack_trace, false);
    ok &= bind("il2cpp_unhandled_exception", il2cpp_unhandled_exception, false);

    ok &= bind("il2cpp_method_get_class", il2cpp_method_get_class, false);
    ok &= bind("il2cpp_method_get_declaring_type", il2cpp_method_get_declaring_type, false);
    ok &= bind("il2cpp_method_get_param_count", il2cpp_method_get_param_count, false);
    ok &= bind("il2cpp_method_get_param", il2cpp_method_get_param, false);
    ok &= bind("il2cpp_method_get_param_name", il2cpp_method_get_param_name, false);
    ok &= bind("il2cpp_method_get_return_type", il2cpp_method_get_return_type, false);
    ok &= bind("il2cpp_method_get_object", il2cpp_method_get_object, false);
    ok &= bind("il2cpp_method_get_from_reflection", il2cpp_method_get_from_reflection, false);
    ok &= bind("il2cpp_method_get_flags", il2cpp_method_get_flags, false);
    ok &= bind("il2cpp_method_get_token", il2cpp_method_get_token, false);
    ok &= bind("il2cpp_method_is_generic", il2cpp_method_is_generic, false);
    ok &= bind("il2cpp_method_is_inflated", il2cpp_method_is_inflated, false);
    ok &= bind("il2cpp_method_is_instance", il2cpp_method_is_instance, false);
    ok &= bind("il2cpp_method_has_attribute", il2cpp_method_has_attribute, false);

    ok &= bind("il2cpp_field_get_offset", il2cpp_field_get_offset, false);
    ok &= bind("il2cpp_field_get_flags", il2cpp_field_get_flags, false);
    ok &= bind("il2cpp_field_get_parent", il2cpp_field_get_parent, false);
    ok &= bind("il2cpp_field_get_type", il2cpp_field_get_type, false);
    ok &= bind("il2cpp_field_get_value_object", il2cpp_field_get_value_object, false);
    ok &= bind("il2cpp_field_set_value_object", il2cpp_field_set_value_object, false);
    ok &= bind("il2cpp_field_static_set_value", il2cpp_field_static_set_value, false);
    ok &= bind("il2cpp_field_has_attribute", il2cpp_field_has_attribute, false);
    ok &= bind("il2cpp_field_is_literal", il2cpp_field_is_literal, false);

    ok &= bind("il2cpp_property_get_flags", il2cpp_property_get_flags, false);
    ok &= bind("il2cpp_property_get_get_method", il2cpp_property_get_get_method, false);
    ok &= bind("il2cpp_property_get_set_method", il2cpp_property_get_set_method, false);
    ok &= bind("il2cpp_property_get_name", il2cpp_property_get_name, false);
    ok &= bind("il2cpp_property_get_parent", il2cpp_property_get_parent, false);

    ok &= bind("il2cpp_type_get_object", il2cpp_type_get_object, false);
    ok &= bind("il2cpp_type_get_name", il2cpp_type_get_name, false);
    ok &= bind("il2cpp_type_get_assembly_qualified_name", il2cpp_type_get_assembly_qualified_name, false);
    ok &= bind("il2cpp_type_get_name_chunked", il2cpp_type_get_name_chunked, false);
    ok &= bind("il2cpp_type_get_type", il2cpp_type_get_type, false);
    ok &= bind("il2cpp_type_get_attrs", il2cpp_type_get_attrs, false);
    ok &= bind("il2cpp_type_get_class_or_element_class", il2cpp_type_get_class_or_element_class, false);
    ok &= bind("il2cpp_type_is_static", il2cpp_type_is_static, false);
    ok &= bind("il2cpp_type_is_pointer_type", il2cpp_type_is_pointer_type, false);
    ok &= bind("il2cpp_type_is_byref", il2cpp_type_is_byref, false);
    ok &= bind("il2cpp_type_equals", il2cpp_type_equals, false);

    ok &= bind("il2cpp_string_new", il2cpp_string_new, false);
    ok &= bind("il2cpp_string_new_len", il2cpp_string_new_len, false);
    ok &= bind("il2cpp_string_new_utf16", il2cpp_string_new_utf16, false);
    ok &= bind("il2cpp_string_new_wrapper", il2cpp_string_new_wrapper, false);
    ok &= bind("il2cpp_string_intern", il2cpp_string_intern, false);
    ok &= bind("il2cpp_string_is_interned", il2cpp_string_is_interned, false);
    ok &= bind("il2cpp_string_length", il2cpp_string_length, false);
    ok &= bind("il2cpp_string_chars", il2cpp_string_chars, false);

    ok &= bind("il2cpp_array_class_get", il2cpp_array_class_get, false);
    ok &= bind("il2cpp_bounded_array_class_get", il2cpp_bounded_array_class_get, false);
    ok &= bind("il2cpp_array_new_specific", il2cpp_array_new_specific, false);
    ok &= bind("il2cpp_array_new_full", il2cpp_array_new_full, false);
    ok &= bind("il2cpp_array_length", il2cpp_array_length, false);
    ok &= bind("il2cpp_array_get_byte_length", il2cpp_array_get_byte_length, false);
    ok &= bind("il2cpp_array_element_size", il2cpp_array_element_size, false);
    ok &= bind("il2cpp_array_object_header_size", il2cpp_array_object_header_size, false);
    ok &= bind("il2cpp_offset_of_array_length_in_array_object_header", il2cpp_offset_of_array_length_in_array_object_header, false);
    ok &= bind("il2cpp_offset_of_array_bounds_in_array_object_header", il2cpp_offset_of_array_bounds_in_array_object_header, false);
    ok &= bind("il2cpp_allocation_granularity", il2cpp_allocation_granularity, false);
    ok &= bind("il2cpp_object_header_size", il2cpp_object_header_size, false);

    ok &= bind("il2cpp_monitor_enter", il2cpp_monitor_enter, false);
    ok &= bind("il2cpp_monitor_try_enter", il2cpp_monitor_try_enter, false);
    ok &= bind("il2cpp_monitor_exit", il2cpp_monitor_exit, false);
    ok &= bind("il2cpp_monitor_pulse", il2cpp_monitor_pulse, false);
    ok &= bind("il2cpp_monitor_pulse_all", il2cpp_monitor_pulse_all, false);
    ok &= bind("il2cpp_monitor_wait", il2cpp_monitor_wait, false);
    ok &= bind("il2cpp_monitor_try_wait", il2cpp_monitor_try_wait, false);

    ok &= bind("il2cpp_gc_collect", il2cpp_gc_collect, false);
    ok &= bind("il2cpp_gc_collect_a_little", il2cpp_gc_collect_a_little, false);
    ok &= bind("il2cpp_gc_disable", il2cpp_gc_disable, false);
    ok &= bind("il2cpp_gc_enable", il2cpp_gc_enable, false);
    ok &= bind("il2cpp_gc_is_disabled", il2cpp_gc_is_disabled, false);
    ok &= bind("il2cpp_gc_is_incremental", il2cpp_gc_is_incremental, false);
    ok &= bind("il2cpp_gc_get_used_size", il2cpp_gc_get_used_size, false);
    ok &= bind("il2cpp_gc_get_heap_size", il2cpp_gc_get_heap_size, false);
    ok &= bind("il2cpp_gc_get_max_time_slice_ns", il2cpp_gc_get_max_time_slice_ns, false);
    ok &= bind("il2cpp_gc_set_max_time_slice_ns", il2cpp_gc_set_max_time_slice_ns, false);
    ok &= bind("il2cpp_gc_wbarrier_set_field", il2cpp_gc_wbarrier_set_field, false);
    ok &= bind("il2cpp_gc_has_strict_wbarriers", il2cpp_gc_has_strict_wbarriers, false);
    ok &= bind("il2cpp_gc_set_external_allocation_tracker", il2cpp_gc_set_external_allocation_tracker, false);
    ok &= bind("il2cpp_gc_set_external_wbarrier_tracker", il2cpp_gc_set_external_wbarrier_tracker, false);
    ok &= bind("il2cpp_gc_foreach_heap", il2cpp_gc_foreach_heap, false);
    ok &= bind("il2cpp_start_gc_world", il2cpp_start_gc_world, false);
    ok &= bind("il2cpp_stop_gc_world", il2cpp_stop_gc_world, false);

    ok &= bind("il2cpp_gchandle_new", il2cpp_gchandle_new, false);
    ok &= bind("il2cpp_gchandle_new_weakref", il2cpp_gchandle_new_weakref, false);
    ok &= bind("il2cpp_gchandle_get_target", il2cpp_gchandle_get_target, false);
    ok &= bind("il2cpp_gchandle_free", il2cpp_gchandle_free, false);
    ok &= bind("il2cpp_gchandle_foreach_get_target", il2cpp_gchandle_foreach_get_target, false);

    ok &= bind("il2cpp_custom_attrs_from_class", il2cpp_custom_attrs_from_class, false);
    ok &= bind("il2cpp_custom_attrs_from_method", il2cpp_custom_attrs_from_method, false);
    ok &= bind("il2cpp_custom_attrs_has_attr", il2cpp_custom_attrs_has_attr, false);
    ok &= bind("il2cpp_custom_attrs_get_attr", il2cpp_custom_attrs_get_attr, false);
    ok &= bind("il2cpp_custom_attrs_construct", il2cpp_custom_attrs_construct, false);
    ok &= bind("il2cpp_custom_attrs_free", il2cpp_custom_attrs_free, false);

    ok &= bind("il2cpp_capture_memory_snapshot", il2cpp_capture_memory_snapshot, false);
    ok &= bind("il2cpp_free_captured_memory_snapshot", il2cpp_free_captured_memory_snapshot, false);
    ok &= bind("il2cpp_debugger_set_agent_options", il2cpp_debugger_set_agent_options, false);
    ok &= bind("il2cpp_is_debugger_attached", il2cpp_is_debugger_attached, false);
    ok &= bind("il2cpp_register_debugger_agent_transport", il2cpp_register_debugger_agent_transport, false);
    ok &= bind("il2cpp_debug_get_method_info", il2cpp_debug_get_method_info, false);
    ok &= bind("il2cpp_override_stack_backtrace", il2cpp_override_stack_backtrace, false);
    ok &= bind("il2cpp_profiler_install", il2cpp_profiler_install, false);
    ok &= bind("il2cpp_profiler_set_events", il2cpp_profiler_set_events, false);
    ok &= bind("il2cpp_profiler_install_enter_leave", il2cpp_profiler_install_enter_leave, false);
    ok &= bind("il2cpp_profiler_install_allocation", il2cpp_profiler_install_allocation, false);
    ok &= bind("il2cpp_profiler_install_gc", il2cpp_profiler_install_gc, false);
    ok &= bind("il2cpp_profiler_install_fileio", il2cpp_profiler_install_fileio, false);
    ok &= bind("il2cpp_profiler_install_thread", il2cpp_profiler_install_thread, false);

    ok &= bind("il2cpp_stats_dump_to_file", il2cpp_stats_dump_to_file, false);
    ok &= bind("il2cpp_stats_get_value", il2cpp_stats_get_value, false);
    ok &= bind("il2cpp_unity_install_unitytls_interface", il2cpp_unity_install_unitytls_interface, false);
    ok &= bind("il2cpp_unity_liveness_calculation_begin", il2cpp_unity_liveness_calculation_begin, false);
    ok &= bind("il2cpp_unity_liveness_calculation_end", il2cpp_unity_liveness_calculation_end, false);
    ok &= bind("il2cpp_unity_liveness_calculation_from_root", il2cpp_unity_liveness_calculation_from_root, false);
    ok &= bind("il2cpp_unity_liveness_calculation_from_statics", il2cpp_unity_liveness_calculation_from_statics, false);

    return ok;
}

} // namespace Il2CppBindings
