typedef uint16_t Il2CppChar;
typedef uintptr_t il2cpp_array_size_t;
typedef int32_t TypeDefinitionIndex;
typedef int32_t GenericParameterIndex;
typedef char Il2CppNativeChar;

typedef struct Il2CppMemoryCallbacks Il2CppMemoryCallbacks;
typedef struct Il2CppImage Il2CppImage;
typedef struct Il2CppClass Il2CppClass;
typedef struct Il2CppType Il2CppType;
typedef struct Il2CppArrayBounds Il2CppArrayBounds;
typedef struct Il2CppAssembly Il2CppAssembly;
typedef struct Il2CppArrayType Il2CppArrayType;
typedef struct Il2CppReflectionType Il2CppReflectionType;
typedef struct MonitorData MonitorData;
typedef Il2CppClass Il2CppVTable;
typedef struct EventInfo EventInfo;
typedef struct PropertyInfo PropertyInfo;
typedef struct Il2CppDomain Il2CppDomain;
typedef struct Il2CppException Il2CppException;
typedef struct Il2CppObject Il2CppObject;
typedef struct Il2CppReflectionMethod Il2CppReflectionMethod;
typedef struct Il2CppString Il2CppString;
typedef struct Il2CppThread Il2CppThread;
typedef struct Il2CppStackFrameInfo Il2CppStackFrameInfo;
typedef struct Il2CppManagedMemorySnapshot Il2CppManagedMemorySnapshot;
typedef struct Il2CppDebuggerTransport Il2CppDebuggerTransport;
typedef struct Il2CppMethodDebugInfo Il2CppMethodDebugInfo;
typedef struct Il2CppCustomAttrInfo Il2CppCustomAttrInfo;
typedef const struct ___Il2CppMetadataTypeHandle *Il2CppMetadataTypeHandle;
typedef const struct ___Il2CppMetadataGenericParameterHandle *Il2CppMetadataGenericParameterHandle;

typedef struct Il2CppIUnknown *(*CreateCCWFunc)(Il2CppObject *obj);

typedef void (*Il2CppMethodPointer)();

typedef void (*il2cpp_register_object_callback)(Il2CppObject **arr, int size, void *userdata);

typedef void *(*il2cpp_liveness_reallocate_callback)(void *ptr, size_t size, void *userdata);

typedef void (*Il2CppFrameWalkFunc)(const Il2CppStackFrameInfo *info, void *user_data);

typedef size_t(*Il2CppBacktraceFunc)(Il2CppMethodPointer *buffer, size_t maxSize);

typedef const Il2CppNativeChar *(*Il2CppSetFindPlugInCallback)(const Il2CppNativeChar *);

typedef void (*Il2CppLogCallback)(const char *);

typedef void (*PInvokeMarshalToNativeFunc)(void *managedStructure, void *marshaledStructure);

typedef void (*PInvokeMarshalFromNativeFunc)(void *marshaledStructure, void *managedStructure);

typedef void (*PInvokeMarshalCleanupFunc)(void *marshaledStructure);

typedef enum {
    IL2CPP_UNHANDLED_POLICY_LEGACY,
    IL2CPP_UNHANDLED_POLICY_CURRENT
} Il2CppRuntimeUnhandledExceptionPolicy;

typedef enum {
    IL2CPP_GC_MODE_DISABLED = 0,
    IL2CPP_GC_MODE_ENABLED = 1,
    IL2CPP_GC_MODE_MANUAL = 2
} Il2CppGCMode;

typedef enum Il2CppStat {
    IL2CPP_STAT_NEW_OBJECT_COUNT,
    IL2CPP_STAT_INITIALIZED_CLASS_COUNT,
    IL2CPP_STAT_METHOD_COUNT,
    IL2CPP_STAT_CLASS_STATIC_DATA_SIZE,
    IL2CPP_STAT_GENERIC_INSTANCE_COUNT,
    IL2CPP_STAT_GENERIC_CLASS_COUNT,
    IL2CPP_STAT_INFLATED_METHOD_COUNT,
    IL2CPP_STAT_INFLATED_TYPE_COUNT,
} Il2CppStat;

typedef enum Il2CppTypeEnum {
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
    IL2CPP_TYPE_CMOD_REQD = 0x1f,
    IL2CPP_TYPE_CMOD_OPT = 0x20,
    IL2CPP_TYPE_INTERNAL = 0x21,
    IL2CPP_TYPE_MODIFIER = 0x40,
    IL2CPP_TYPE_SENTINEL = 0x41,
    IL2CPP_TYPE_PINNED = 0x45,
    IL2CPP_TYPE_ENUM = 0x55,
    IL2CPP_TYPE_IL2CPP_TYPE_INDEX = 0xff
} Il2CppTypeEnum;

typedef enum Il2CppRGCTXDataType {
    IL2CPP_RGCTX_DATA_INVALID,
    IL2CPP_RGCTX_DATA_TYPE,
    IL2CPP_RGCTX_DATA_CLASS,
    IL2CPP_RGCTX_DATA_METHOD,
    IL2CPP_RGCTX_DATA_ARRAY,
    IL2CPP_RGCTX_DATA_CONSTRAINED,
} Il2CppRGCTXDataType;

typedef struct Il2CppGenericInst {
    uint32_t type_argc;
    const Il2CppType **type_argv;
} Il2CppGenericInst;

typedef struct Il2CppGenericContext {
    /* The instantiation corresponding to the class generic parameters */
    const Il2CppGenericInst *class_inst;
    /* The instantiation corresponding to the method generic parameters */
    const Il2CppGenericInst *method_inst;
} Il2CppGenericContext;

typedef struct Il2CppGenericClass {
    TypeDefinitionIndex type;        /* the generic type definition */
    Il2CppGenericContext context;   /* a context that contains the type instantiation doesn't contain any method instantiation */
    Il2CppClass *cached_class; /* if present, the Il2CppClass corresponding to the instantiation.  */
} Il2CppGenericClass;

typedef struct Il2CppType {
    union {
        void *dummy;
        TypeDefinitionIndex klassIndex;
        const Il2CppType *type;
        Il2CppArrayType *array;
        GenericParameterIndex genericParameterIndex;
        Il2CppGenericClass *generic_class;
    } data;
    unsigned int attrs: 16;
    Il2CppTypeEnum type: 8;
    unsigned int num_mods: 6;
    unsigned int byref: 1;
    unsigned int pinned: 1;
} Il2CppType;

typedef struct MethodInfo {
    Il2CppMethodPointer methodPointer;
} MethodInfo;

typedef struct Il2CppObject {
    union {
        Il2CppClass *klass;
        Il2CppVTable *vtable;
    };
    MonitorData *monitor;
} Il2CppObject;

typedef struct Il2CppArray {
    Il2CppObject obj;
    Il2CppArrayBounds *bounds;
    il2cpp_array_size_t max_length;
    void *vector[32];
} Il2CppArray;

typedef struct Il2CppTypeDefinition {
    uint32_t nameIndex;
    uint32_t namespaceIndex;
    int32_t byvalTypeIndex;

    int32_t declaringTypeIndex;
    int32_t parentIndex;
    int32_t elementTypeIndex; // we can probably remove this one. Only used for enums

    int32_t genericContainerIndex;

    uint32_t flags;

    int32_t fieldStart;
    int32_t methodStart;
    int32_t eventStart;
    int32_t propertyStart;
    int32_t nestedTypesStart;
    int32_t interfacesStart;
    int32_t vtableStart;
    int32_t interfaceOffsetsStart;

    uint16_t method_count;
    uint16_t property_count;
    uint16_t field_count;
    uint16_t event_count;
    uint16_t nested_type_count;
    uint16_t vtable_count;
    uint16_t interfaces_count;
    uint16_t interface_offsets_count;

    // bitfield to portably encode boolean values as single bits
    // 01 - valuetype;
    // 02 - enumtype;
    // 03 - has_finalize;
    // 04 - has_cctor;
    // 05 - is_blittable;
    // 06 - is_import_or_windows_runtime;
    // 07-10 - One of nine possible PackingSize values (0, 1, 2, 4, 8, 16, 32, 64, or 128)
    // 11 - PackingSize is default
    // 12 - ClassSize is default
    // 13-16 - One of nine possible PackingSize values (0, 1, 2, 4, 8, 16, 32, 64, or 128) - the specified packing size (even for explicit layouts)
    uint32_t bitfield;
    uint32_t token;
} Il2CppTypeDefinition;

typedef struct Il2CppInteropData {
    Il2CppMethodPointer delegatePInvokeWrapperFunction;
    PInvokeMarshalToNativeFunc pinvokeMarshalToNativeFunction;
    PInvokeMarshalFromNativeFunc pinvokeMarshalFromNativeFunction;
    PInvokeMarshalCleanupFunc pinvokeMarshalCleanupFunction;
    CreateCCWFunc createCCWFunction;
    const unsigned long *guid;
    const Il2CppType *type;
} Il2CppInteropData;

typedef struct Il2CppRuntimeInterfaceOffsetPair {
    Il2CppClass *interfaceType;
    int32_t offset;
} Il2CppRuntimeInterfaceOffsetPair;

typedef union Il2CppRGCTXData {
    void *rgctxDataDummy;
    const MethodInfo *method;
    const Il2CppType *type;
    Il2CppClass *klass;
} Il2CppRGCTXData;

typedef struct VirtualInvokeData {
    Il2CppMethodPointer methodPtr;
    const MethodInfo *method;
} VirtualInvokeData;

typedef struct FieldInfo {
    const char *name;
    const Il2CppType *type;
    Il2CppClass *parent;
    int32_t offset; // If offset is -1, then it's thread static
    uint32_t token;
} FieldInfo;

typedef struct Il2CppClass {
    // Il2CppClass_1
    const Il2CppImage *image;
    void *gc_desc;
    const char *name;
    const char *namespaze;
    Il2CppType byval_arg;
    Il2CppType this_arg;
    Il2CppClass *element_class;
    Il2CppClass *castClass;
    Il2CppClass *declaringType;
    Il2CppClass *parent;
    Il2CppGenericClass *generic_class;
    const Il2CppTypeDefinition *typeMetadataHandle;
    const Il2CppInteropData *interopData;
    Il2CppClass *klass;
    FieldInfo *fields;
    const EventInfo *events;
    const PropertyInfo *properties;
    const MethodInfo **methods;
    Il2CppClass **nestedTypes;
    Il2CppClass **implementedInterfaces;
    Il2CppRuntimeInterfaceOffsetPair *interfaceOffsets;

    void *static_fields;
    const Il2CppRGCTXData *rgctx_data;

    // Il2CppClass_2
    Il2CppClass **typeHierarchy;
    void *unity_user_data;
    uint32_t initializationExceptionGCHandle;
    uint32_t cctor_started;
    uint32_t cctor_finished;
    size_t cctor_thread;
    void *genericContainerHandle;
    uint32_t instance_size;
    uint32_t actualSize;
    uint32_t element_size;
    uint32_t placeholder;
    int32_t native_size;
    uint32_t static_fields_size;
    uint32_t thread_static_fields_size;
    int32_t thread_static_fields_offset;
    uint32_t flags;
    uint32_t token;
    uint16_t method_count;
    uint16_t property_count;
    uint16_t field_count;
    uint16_t event_count;
    uint16_t nested_type_count;
    uint16_t vtable_count;
    uint16_t interfaces_count;
    uint16_t interface_offsets_count;
    uint8_t typeHierarchyDepth;
    uint8_t genericRecursionDepth;
    uint8_t rank;
    uint8_t minimumAlignment;
    uint8_t naturalAligment;
    uint8_t packingSize;
    uint8_t bitflags1;
    uint8_t bitflags2;

    VirtualInvokeData vtable[255];
} Il2CppClass;
