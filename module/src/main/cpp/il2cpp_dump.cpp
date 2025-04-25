//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <unistd.h>
#include <regex>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"
#include "stringbuilder/stringbuilder.h"
#include "rapidjson/document.h"
#include "stringutil.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

static std::set<std::string> keywords = {"klass", "monitor", "register", "_cs", "auto", "friend",
                                         "template", "flat", "default", "_ds", "interrupt",
                                         "unsigned", "signed", "asm", "if", "case", "break",
                                         "continue", "do", "new", "_", "short", "union", "class",
                                         "namespace"};

static std::set<std::string> specialKeywords = {"inline", "near", "far"};

std::string pattern1("^[0-9]");
std::string pattern2("[^a-zA-Z0-9_]");
std::regex r1(pattern1);
std::regex r2(pattern2);

static std::string GetMethodTypeSignature(std::vector<Il2CppTypeEnum> types) {
    std::string signature;
    for (const auto &type: types) {
        switch (type) {
            case IL2CPP_TYPE_VOID:
                signature += "v";
                break;
            case IL2CPP_TYPE_BOOLEAN:
            case IL2CPP_TYPE_CHAR:
            case IL2CPP_TYPE_I1:
            case IL2CPP_TYPE_U1:
            case IL2CPP_TYPE_I2:
            case IL2CPP_TYPE_U2:
            case IL2CPP_TYPE_I4:
            case IL2CPP_TYPE_U4:
                signature += "i";
                break;
            case IL2CPP_TYPE_I8:
            case IL2CPP_TYPE_U8:
                signature += "j";
                break;
            case IL2CPP_TYPE_R4:
                signature += "f";
                break;
            case IL2CPP_TYPE_R8:
                signature += "d";
                break;
            case IL2CPP_TYPE_STRING:
            case IL2CPP_TYPE_PTR:
            case IL2CPP_TYPE_VALUETYPE:
            case IL2CPP_TYPE_CLASS:
            case IL2CPP_TYPE_VAR:
            case IL2CPP_TYPE_ARRAY:
            case IL2CPP_TYPE_GENERICINST:
            case IL2CPP_TYPE_TYPEDBYREF:
            case IL2CPP_TYPE_I:
            case IL2CPP_TYPE_U:
            case IL2CPP_TYPE_OBJECT:
            case IL2CPP_TYPE_SZARRAY:
            case IL2CPP_TYPE_MVAR:
                signature += "i";
                break;
            default:
                throw std::invalid_argument("Type not supported");
        }
    }
    return signature;
}

static std::string FixName(std::string str) {
    if (keywords.contains(str)) {
        str = "_" + str;
    } else if (specialKeywords.contains(str)) {
        str = "_" + str + "_";
    }

    if (std::regex_match(str, r1)) {
        return "_" + str;
    } else {
        return std::regex_replace(str, r2, "_");
    }
}

std::string parseType(Il2CppType il2CppType, Il2CppGenericContext *context = nullptr) {
    switch (il2CppType.type) {
        case IL2CPP_TYPE_VOID:
            return "void";
        case IL2CPP_TYPE_BOOLEAN:
            return "bool";
        case IL2CPP_TYPE_CHAR:
            return "uint16_t"; //Il2CppChar
        case IL2CPP_TYPE_I1:
            return "int8_t";
        case IL2CPP_TYPE_U1:
            return "uint8_t";
        case IL2CPP_TYPE_I2:
            return "int16_t";
        case IL2CPP_TYPE_U2:
            return "uint16_t";
        case IL2CPP_TYPE_I4:
            return "int32_t";
        case IL2CPP_TYPE_U4:
            return "uint32_t";
        case IL2CPP_TYPE_I8:
            return "int64_t";
        case IL2CPP_TYPE_U8:
            return "uint64_t";
        case IL2CPP_TYPE_R4:
            return "float";
        case IL2CPP_TYPE_R8:
            return "double";
        case IL2CPP_TYPE_STRING:
            return "System_String_o*";
        case IL2CPP_TYPE_PTR: {
            auto oriType = *il2CppType.data.type;
            return parseType(oriType, context) + "*";
        }
        case IL2CPP_TYPE_VALUETYPE: {
            auto klass = il2cpp_class_from_type(&il2CppType);
//            auto is_enum = il2cpp_class_is_enum(klass);
//            if (is_enum) {
//                auto oriType = *il2CppType.data.type;
//                return parseType(oriType, context);
//            }
            auto klass_name = il2cpp_class_get_name(klass);
            return FixName(klass_name) + "_o";
        }
        case IL2CPP_TYPE_CLASS: {
            auto klass = il2cpp_class_from_type(&il2CppType);
            auto klass_name = il2cpp_class_get_name(klass);
            return FixName(klass_name) + "_o*";
        }
        case IL2CPP_TYPE_VAR: {
//            if (context != nullptr) {
//                var genericParameter = executor.GetGenericParameteFromIl2CppType(il2CppType);
//                var genericInst = il2Cpp.MapVATR<Il2CppGenericInst>(context.class_inst);
//                var pointers = il2Cpp.MapVATR<ulong>(genericInst.type_argv, genericInst.type_argc);
//                var pointer = pointers[genericParameter.num];
//                var type = il2Cpp.GetIl2CppType(pointer);
//                return ParseType(type);
//            }
            return "Il2CppObject*";
        }
        case IL2CPP_TYPE_ARRAY: {
//            auto arrayType = il2CppType.data.array;
//            var elementType = il2Cpp.GetIl2CppType(arrayType->etype);
//            var elementStructName = GetIl2CppStructName(elementType, context);
//            var typeStructName = elementStructName + "_array";
//            if (structNameHashSet.Add(typeStructName)) {
//                ParseArrayClassStruct(elementType, context);
//            }
            auto klass = il2cpp_class_from_type(&il2CppType);
            auto klass_name = il2cpp_class_get_name(klass);
            return FixName(klass_name) + "*";
        }
        case IL2CPP_TYPE_GENERICINST: {
            auto genericClass = il2CppType.data.generic_class;
            auto klass = il2cpp_class_from_type(&il2CppType);
            auto klass_name = il2cpp_class_get_name(klass);
            return FixName(klass_name) + "_o*";
//            var typeDef = executor.GetGenericClassTypeDefinition(genericClass);
//            var typeStructName = genericClassStructNameDic[il2CppType.data.generic_class];
//            if (structNameHashSet.Add(typeStructName)) {
//                genericClassList.Add();
//            }
//            if (typeDef.IsValueType) {
//                if (typeDef.IsEnum) {
//                    return ParseType(il2Cpp.types[typeDef.elementTypeIndex]);
//                }
//                return typeStructName + "_o";
//            }
//            return typeStructName + "_o*";
        }
        case IL2CPP_TYPE_TYPEDBYREF:
            return "Il2CppObject*";
        case IL2CPP_TYPE_I:
            return "intptr_t";
        case IL2CPP_TYPE_U:
            return "uintptr_t";
        case IL2CPP_TYPE_OBJECT:
            return "Il2CppObject*";
        case IL2CPP_TYPE_SZARRAY: {
            auto elementType = il2CppType.data.type;
            auto klass = il2cpp_class_from_type(elementType);
            auto klass_name = il2cpp_class_get_name(klass);
            return FixName(klass_name) + "_array*";
        }
        case IL2CPP_TYPE_MVAR: {
//            if (context != nullptr) {
//                var genericParameter = executor.GetGenericParameteFromIl2CppType(il2CppType);
//                //https://github.com/Perfare/Il2CppDumper/issues/687
//                if (context.method_inst == 0 && context.class_inst != 0) {
//                    goto
//                    case Il2CppTypeEnum.IL2CPP_TYPE_VAR;
//                }
//                var genericInst = il2Cpp.MapVATR<Il2CppGenericInst>(context.method_inst);
//                var pointers = il2Cpp.MapVATR<ulong>(genericInst.type_argv, genericInst.type_argc);
//                var pointer = pointers[genericParameter.num];
//                var type = il2Cpp.GetIl2CppType(pointer);
//                return ParseType(type);
//            }
            return "Il2CppObject*";
        }
        default:
            throw std::invalid_argument("Type not supported");
    }
}

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    auto byref = type->byref;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
}

std::string dump_method(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Methods\n";
    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        //TODO attribute
        if (method->methodPointer) {
            outPut << "\t// RVA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer - il2cpp_base;
            outPut << " VA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer;
        } else {
            outPut << "\t// RVA: 0x VA: 0x0";
        }
        /*if (method->slot != 65535) {
            outPut << " Slot: " << std::dec << method->slot;
        }*/
        outPut << "\n\t";
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        outPut << get_method_modifier(flags);
        //TODO genericContainerIndex
        auto return_type = il2cpp_method_get_return_type(method);
        if (_il2cpp_type_is_byref(return_type)) {
            outPut << "ref ";
        }
        auto return_class = il2cpp_class_from_type(return_type);
        std::string return_class_full_name;
        auto _namespace = il2cpp_class_get_namespace(return_class);
        if (_namespace != nullptr && _namespace[0] != '\0') {
            return_class_full_name += std::string(_namespace);
            return_class_full_name += ".";
        }
        auto declaringType = return_class->declaringType;
        if (declaringType != nullptr) {
            return_class_full_name += std::string(declaringType->name);
            return_class_full_name += ".";
        }
        return_class_full_name += std::string(il2cpp_class_get_name(return_class));
        outPut << return_class_full_name << " " << il2cpp_method_get_name(method)
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto parameter_class = il2cpp_class_from_type(param);
            std::string parameter_class_full_name;
            auto _namespace = il2cpp_class_get_namespace(parameter_class);
            if (_namespace != nullptr && _namespace[0] != '\0') {
                parameter_class_full_name += std::string(_namespace);
                parameter_class_full_name += ".";
            }
            auto declaringType = parameter_class->declaringType;
            if (declaringType != nullptr) {
                parameter_class_full_name += std::string(declaringType->name);
                parameter_class_full_name += ".";
            }
            parameter_class_full_name += std::string(il2cpp_class_get_name(parameter_class));
            outPut << parameter_class_full_name << " "
                   << il2cpp_method_get_param_name(method, i);
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
        //TODO GenericInstMethod
    }
    return outPut.str();
}

std::string dump_property(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Properties\n";
    void *iter = nullptr;
    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        //TODO attribute
        auto prop = const_cast<PropertyInfo *>(prop_const);
        auto get = il2cpp_property_get_get_method(prop);
        auto set = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);
        outPut << "\t";
        Il2CppClass *prop_class = nullptr;
        uint32_t iflags = 0;
        if (get) {
            outPut << get_method_modifier(il2cpp_method_get_flags(get, &iflags));
            prop_class = il2cpp_class_from_type(il2cpp_method_get_return_type(get));
        } else if (set) {
            outPut << get_method_modifier(il2cpp_method_get_flags(set, &iflags));
            auto param = il2cpp_method_get_param(set, 0);
            prop_class = il2cpp_class_from_type(param);
        }
        if (prop_class) {
            std::string prop_class_full_name;
            auto _namespace = il2cpp_class_get_namespace(prop_class);
            if (_namespace != nullptr && _namespace[0] != '\0') {
                prop_class_full_name += std::string(_namespace);
                prop_class_full_name += ".";
            }
            auto declaringType = prop_class->declaringType;
            if (declaringType != nullptr) {
                prop_class_full_name += std::string(declaringType->name);
                prop_class_full_name += ".";
            }
            prop_class_full_name += std::string(il2cpp_class_get_name(prop_class));
            outPut << prop_class_full_name << " " << prop_name << " { ";
            if (get) {
                outPut << "get; ";
            }
            if (set) {
                outPut << "set; ";
            }
            outPut << "}\n";
        } else {
            if (prop_name) {
                outPut << " // unknown property " << prop_name;
            }
        }
    }
    return outPut.str();
}

std::string dump_field(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Fields\n";
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        //TODO attribute
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        std::string field_class_full_name;
        auto _namespace = il2cpp_class_get_namespace(field_class);
        if (_namespace != nullptr && _namespace[0] != '\0') {
            field_class_full_name += std::string(_namespace);
            field_class_full_name += ".";
        }
        auto declaringType = field_class->declaringType;
        if (declaringType != nullptr) {
            field_class_full_name += std::string(declaringType->name);
            field_class_full_name += ".";
        }
        field_class_full_name += std::string(il2cpp_class_get_name(field_class));
        outPut << field_class_full_name << " " << il2cpp_field_get_name(field);
        //TODO 获取构造函数初始化后的字段值
        if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
        }
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(const Il2CppType *type) {
    std::stringstream outPut;
    auto *klass = il2cpp_class_from_type(type);
    outPut << "\n// Namespace: " << il2cpp_class_get_namespace(klass) << "\n";
    auto flags = il2cpp_class_get_flags(klass);
    if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
        outPut << "[Serializable]\n";
    }
    //TODO attribute
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
    switch (visibility) {
        case TYPE_ATTRIBUTE_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_PUBLIC:
            outPut << "public ";
            break;
        case TYPE_ATTRIBUTE_NOT_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
        case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
            outPut << "internal ";
            break;
        case TYPE_ATTRIBUTE_NESTED_PRIVATE:
            outPut << "private ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAMILY:
            outPut << "protected ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "static ";
    } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
    } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "sealed ";
    }
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        outPut << "interface ";
    } else if (is_enum) {
        outPut << "enum ";
    } else if (is_valuetype) {
        outPut << "struct ";
    } else {
        outPut << "class ";
    }
    auto _namespace = il2cpp_class_get_namespace(klass);
    if (_namespace != nullptr && _namespace[0] != '\0') {
        outPut << _namespace << ".";
    }
    auto declaringType = klass->declaringType;
    if (declaringType != nullptr) {
        outPut << declaringType->name << ".";
    }
    outPut << il2cpp_class_get_name(klass); //TODO genericContainerIndex
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);
        if (parent_type->type != IL2CPP_TYPE_OBJECT) {
            std::string parent_full_name;
            auto _namespace = il2cpp_class_get_namespace(parent);
            if (_namespace != nullptr && _namespace[0] != '\0') {
                parent_full_name += std::string(_namespace);
                parent_full_name += ".";
            }
            auto declaringType = parent->declaringType;
            if (declaringType != nullptr) {
                parent_full_name += std::string(declaringType->name);
                parent_full_name += ".";
            }
            parent_full_name += std::string(il2cpp_class_get_name(parent));
            extends.emplace_back(parent_full_name);
        }
    }
    void *iter = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
        std::string interface_full_name;
        auto _itf_namespace = il2cpp_class_get_namespace(itf);
        if (_itf_namespace != nullptr && _itf_namespace[0] != '\0') {
            interface_full_name += std::string(_itf_namespace);
            interface_full_name += ".";
        }
        auto declaringType = itf->declaringType;
        if (declaringType != nullptr) {
            interface_full_name += std::string(declaringType->name);
            interface_full_name += ".";
        }
        interface_full_name += std::string(il2cpp_class_get_name(itf));
        extends.emplace_back(interface_full_name);
    }
    if (!extends.empty()) {
        outPut << " : " << extends[0];
        for (int i = 1; i < extends.size(); ++i) {
            outPut << ", " << extends[i];
        }
    }
    outPut << "\n{";
    outPut << dump_field(klass);
    outPut << dump_property(klass);
    outPut << dump_method(klass);
    //TODO EventInfo
    outPut << "}\n";
    return outPut.str();
}

void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}

void il2cpp_dump(const char *outDir) {
    LOGI("dumping dump.cs ...");
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    std::stringstream imageOutput;
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        imageOutput << "// Image " << i << ": " << il2cpp_image_get_name(image) << "\n";
    }
    std::vector<std::string> outPuts;
    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");
        //使用il2cpp_image_get_class
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            imageStr << "\n// Dll : " << il2cpp_image_get_name(image);
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.emplace_back(outPut);
            }
        }
    } else {
        LOGI("Version less than 2018.3");
        //使用反射
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);
        if (assemblyLoad && assemblyLoad->methodPointer) {
            LOGI("Assembly::Load: %p", assemblyLoad->methodPointer);
        } else {
            LOGI("miss Assembly::Load");
            return;
        }
        if (assemblyGetTypes && assemblyGetTypes->methodPointer) {
            LOGI("Assembly::GetTypes: %p", assemblyGetTypes->methodPointer);
        } else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
        typedef void *(*Assembly_Load_ftn)(void *, Il2CppString *, void *);
        typedef Il2CppArray *(*Assembly_GetTypes_ftn)(void *, void *);
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            auto image_name = il2cpp_image_get_name(image);
            imageStr << "\n// Dll : " << image_name;
            //LOGD("image name : %s", image->name);
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.data());
            auto reflectionAssembly = ((Assembly_Load_ftn) assemblyLoad->methodPointer)(nullptr,
                                                                                        assemblyFileName,
                                                                                        nullptr);
            auto reflectionTypes = ((Assembly_GetTypes_ftn) assemblyGetTypes->methodPointer)(
                    reflectionAssembly, nullptr);
            auto items = reflectionTypes->vector;
            for (int j = 0; j < reflectionTypes->max_length; ++j) {
                auto klass = il2cpp_class_from_system_type((Il2CppReflectionType *) items[j]);
                auto type = il2cpp_class_get_type(klass);
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.emplace_back(outPut);
            }
        }
    }
    LOGI("write dump.cs");
    auto outPath = std::string(outDir).append("/files/dump.cs");
    std::ofstream outStream(outPath);
    outStream << imageOutput.str();
    auto count = outPuts.size();
    for (int i = 0; i < count; ++i) {
        outStream << outPuts[i];
    }
    outStream.close();
    LOGI("dump dump.cs done!");
}

void il2cpp_dump_script_json(const char *outDir) {
    LOGI("dumping script.json ...");
    rapidjson::Document d(rapidjson::kObjectType);
    rapidjson::Value ScriptMethod(rapidjson::kArrayType);
    rapidjson::Value ScriptString(rapidjson::kArrayType);
    rapidjson::Value ScriptMetadata(rapidjson::kArrayType);
    rapidjson::Value ScriptMetadataMethod(rapidjson::kArrayType);
    rapidjson::Value Addresses(rapidjson::kArrayType);
    auto allocator = d.GetAllocator();

    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);

    // Iterate images.
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        auto classCount = il2cpp_image_get_class_count(image);
        // Iterate classes.
        for (int j = 0; j < classCount; ++j) {
            auto const_klass = il2cpp_image_get_class(image, j);
            auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(const_klass));
            auto *klass = il2cpp_class_from_type(type);
            // Iterate methods.
            void *iter = nullptr;
            while (auto method = il2cpp_class_get_methods(klass, &iter)) {
                rapidjson::Value methodObject(rapidjson::kObjectType);

                // Apply address.
                if (method->methodPointer) {
                    uint64_t rva = (uint64_t) method->methodPointer - il2cpp_base;
                    methodObject.AddMember("Address", rva, allocator);
                } else {
                    methodObject.AddMember("Address", 0, allocator);
                }

                // Apply name.
                auto className = il2cpp_class_get_name(klass);
                auto methodName = il2cpp_method_get_name(method);
                auto methodFullNameString =
                        std::string(className) + "$$" + std::string(methodName);
//                LOGI("MethodFullName: %s", methodFullNameString.c_str());
                rapidjson::Value methodFullName;
                methodFullName.SetString(methodFullNameString.c_str(), allocator);
                methodObject.AddMember("Name", methodFullName, allocator);

                // Apply signature.
                auto return_type = il2cpp_method_get_return_type(method);
                auto return_class = il2cpp_class_from_type(return_type);
                auto returnType = parseType(*return_type);
                if (return_type->byref == 1) {
                    returnType += "*";
                }
                auto fixedName = FixName(methodFullNameString);
                auto signatureString = returnType + " " + fixedName + " (";
                std::vector<std::string> parameterStrs;
                uint32_t iflags = 0;
                auto flags = il2cpp_method_get_flags(method, &iflags);
                // Add this param.
                if ((flags & METHOD_ATTRIBUTE_STATIC) == 0) {
                    auto klass_type = parseType(*type);
                    parameterStrs.emplace_back(klass_type + " __this");
                }
                // Add other params.
                auto param_count = il2cpp_method_get_param_count(method);
                for (auto k = 0; k < param_count; k++) {
                    auto param = il2cpp_method_get_param(method, k);
//                    auto parameter_class = il2cpp_class_from_type(param);
//                    auto parameterType = il2cpp_class_get_name(parameter_class);
                    auto parameterCType = parseType(*param);
                    if (param->byref == 1) {
                        parameterCType += "*";
                    }
                    auto parameterName = il2cpp_method_get_param_name(method, k);
                    parameterStrs.emplace_back(parameterCType + " " + FixName(parameterName));
                }

                parameterStrs.emplace_back("const MethodInfo* method");
                signatureString += StringJoin(parameterStrs, ", ");
                signatureString += ");";
                rapidjson::Value signature;
                signature.SetString(signatureString.c_str(), allocator);
                methodObject.AddMember("Signature", signature, allocator);

                // Apply typeSignature.
                std::vector<Il2CppTypeEnum> methodTypeSignature;
                methodTypeSignature.emplace_back(
                        return_type->byref == 1 ? IL2CPP_TYPE_PTR : return_type->type);
                if ((flags & METHOD_ATTRIBUTE_STATIC) == 0) {
                    methodTypeSignature.emplace_back(type->type);
                }
                for (auto k = 0; k < param_count; k++) {
                    auto param = il2cpp_method_get_param(method, k);
                    methodTypeSignature.emplace_back(
                            param->byref == 1 ? IL2CPP_TYPE_PTR : param->type);
                }
                methodTypeSignature.emplace_back(IL2CPP_TYPE_PTR);
                rapidjson::Value methodTypeSignatureFinal;
                methodTypeSignatureFinal.SetString(
                        GetMethodTypeSignature(methodTypeSignature).c_str(), allocator);
                methodObject.AddMember("TypeSignature", methodTypeSignatureFinal, allocator);

                ScriptMethod.PushBack(methodObject, allocator);
            }
        }
    }

    d.AddMember("ScriptMethod", ScriptMethod, allocator);
    d.AddMember("ScriptString", ScriptString, allocator);
    d.AddMember("ScriptMetadata", ScriptMetadata, allocator);
    d.AddMember("ScriptMetadataMethod", ScriptMetadataMethod, allocator);
    d.AddMember("Addresses", Addresses, allocator);

    LOGI("write script.json");
    auto outPath = std::string(outDir).append("/files/script.json");
    FILE *fp = fopen(outPath.c_str(), "w");
    char writeBuffer[65536];
    rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
    d.Accept(writer);
    fclose(fp);

    LOGI("dump script.json done!");
}