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
#include <unordered_set>
#include <map>
#include <memory>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <unistd.h>
#include <regex>
#include <algorithm>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"
#include "stringbuilder/stringbuilder.h"
#include "rapidjson/document.h"
#include "stringutil.h"
#include "header_constants.h"
#include "StructInfo.h"
#include "base64.hpp"

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

std::set<std::string> structNameHashSet;
std::set<StructInfo> structCache;
std::set<Il2CppMethodPointer> methodInfoCache;
// For class declaration.
std::unordered_set<std::string> all_type_names;

std::map<Il2CppClass *, std::string> structNameDic;
std::map<Il2CppGenericClass *, std::string> genericClassStructNameDic;
std::map<std::string, Il2CppType *> nameGenericClassDic;
std::map<std::string, StructInfo> structInfoWithStructName;
std::vector<StructInfo> structInfoList;
std::vector<Il2CppGenericClass *> genericClassList;

auto arrayClassHeader = sbldr::stringbuilder<10>{};
auto methodInfoHeader = sbldr::stringbuilder<10>{};

std::string pattern1("^[0-9]");
std::string pattern2("[^a-zA-Z0-9_]");
std::regex r1(pattern1);
std::regex r2(pattern2);

void parseArrayClassStruct(const Il2CppType *il2CppType, Il2CppGenericContext *context);

std::string getFullTypeName(const Il2CppClass *klass);

struct ClassStruct {
    std::string parentName;
    std::string classStruct;
};

struct GenericInst {
    std::string name;
    std::string generic_class_name;

    bool operator==(const GenericInst &other) const {
        return name == other.name && generic_class_name == other.generic_class_name;
    }
};

// Specialize std::hash
namespace std {
    template<>
    struct hash<GenericInst> {
        size_t operator()(const GenericInst &g) const {
            size_t h1 = std::hash<std::string>{}(g.name);
            size_t h2 = std::hash<std::string>{}(g.generic_class_name);
            return h1 ^ (h2 << 1); // Combine hashes (simple way)
        }
    };
}

// For generic instance.
std::unordered_set<GenericInst> all_generic_instances;

// Store all struct_class.
std::map<std::string, ClassStruct> allClasses;

// Store sorted struct class.
std::vector<std::string> sortedClasses;

// Stored classes that has been sorted.
std::unordered_set<std::string> visited;

bool containsChineseRegex(const std::string &str) {
    // Pattern: match any character between U+4E00 and U+9FFF
    std::regex chinese("[\xE4\xB8\x80-\xE9\xBE\xBF]");
    return std::regex_search(str, chinese);
}

std::vector<std::string> parseTemplateParams(const std::string &input) {
    std::vector<std::string> result;

    size_t start = input.find('<');
    if (start == std::string::npos) {
        return result; // no < found
    }

    start++; // move after '<'
    int depth = 0;
    size_t lastPos = start;

    for (size_t i = start; i < input.length(); ++i) {
        char c = input[i];
        if (c == '<') {
            depth++;
        } else if (c == '>') {
            if (depth == 0) {
                // end of top-level parameters
                result.push_back(input.substr(lastPos, i - lastPos));
                break;
            }
            depth--;
        } else if (c == ',' && depth == 0) {
            // top-level comma
            result.push_back(input.substr(lastPos, i - lastPos));
            lastPos = i + 1;
        }
    }

    return result;
}

bool dfs(const std::string &className) {
    if (visited.count(className)) {
        return true; // Class has been sorted.
    }

    auto it = allClasses.find(className);
    if (it == allClasses.end()) {
        // Not found this class.
        return true;
    }

    visited.insert(className);

    const ClassStruct &cls = it->second;
    if (!cls.parentName.empty()) {
        // Sort parent first.
        if (!dfs(cls.parentName)) {
            return false;
        }
    }

    // Sort self.
    sortedClasses.emplace_back(cls.classStruct);
    return true;
}

std::string FixName(std::string str) {
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

bool isCustomType(const Il2CppType *il2CppType, Il2CppGenericContext *context) {
    switch (il2CppType->type) {
        case IL2CPP_TYPE_PTR: {
            auto oriType = il2CppType->data.type;
            return isCustomType(oriType, context);
        }
        case IL2CPP_TYPE_VALUETYPE:
        case IL2CPP_TYPE_STRING:
        case IL2CPP_TYPE_CLASS:
        case IL2CPP_TYPE_GENERICINST:
        case IL2CPP_TYPE_ARRAY:
        case IL2CPP_TYPE_SZARRAY: {
            return true;
        }
        case IL2CPP_TYPE_ENUM: {
            auto klass = il2cpp_class_from_type(il2CppType);
            auto enum_basetype = il2cpp_class_enum_basetype(klass);
            if (enum_basetype != nullptr) {
                return isCustomType(enum_basetype, context);
            } else {
                return false;
            }
        }
        case IL2CPP_TYPE_VAR: {
            if (context != nullptr) {
                auto genericInst = context->class_inst;
                auto type_argv = genericInst->type_argv;
                // TODO: Get correct index of generic parameter.
                auto type = type_argv[0];
                return isCustomType(type, nullptr);
            }
            return false;
        }
        case IL2CPP_TYPE_MVAR: {
            if (context != nullptr) {
                auto genericInst = context->method_inst;
                auto type_argv = genericInst->type_argv;
                // TODO: Get correct index of generic parameter.
                auto type = type_argv[0];
                return isCustomType(type, nullptr);
            }
            return false;
        }
        default:
            return false;
    }
}

void generateMethodInfo(const std::string &methodInfoName, const std::string &structTypeName,
                        std::vector<StructRGCTXInfo> rgctxs = {}) {
    if (!rgctxs.empty()) {
        methodInfoHeader
                .append("struct ").append(methodInfoName).append("_RGCTXs {\n");
        for (int i = 0; i < rgctxs.size(); i++) {
            auto rgctx = rgctxs[i];
            switch (rgctx.Type) {
                case IL2CPP_RGCTX_DATA_TYPE:
                    methodInfoHeader
                            .append("\tIl2CppType* _").append(std::to_string(i))
                            .append("_").append(rgctx.TypeName).append(";\n");
                    break;
                case IL2CPP_RGCTX_DATA_CLASS:
                    methodInfoHeader
                            .append("\tIl2CppClass* _").append(std::to_string(i))
                            .append("_").append(rgctx.ClassName).append(";\n");
                    break;
                case IL2CPP_RGCTX_DATA_METHOD:
                    methodInfoHeader
                            .append("\tMethodInfo* _").append(std::to_string(i))
                            .append("_").append(rgctx.MethodName).append(";\n");
                    break;
            }
        }
        methodInfoHeader.append("};\n");
    }
    methodInfoHeader.append("\nstruct ").append(methodInfoName).append(" {\n");
    methodInfoHeader.append("\tIl2CppMethodPointer methodPointer;\n");
    methodInfoHeader.append("\tIl2CppMethodPointer virtualMethodPointer;\n");
    methodInfoHeader.append("\tInvokerMethod invoker_method;\n");
    methodInfoHeader.append("\tconst char* name;\n");
    methodInfoHeader.append("\t").append(structTypeName).append("_c *klass;\n");
    methodInfoHeader.append("\tconst Il2CppType *return_type;\n");
    methodInfoHeader.append("\tconst Il2CppType** parameters;\n");

    if (!rgctxs.empty()) {
        methodInfoHeader.append("\tconst ").append(methodInfoName).append("_RGCTXs* rgctx_data;\n");
    } else {
        methodInfoHeader.append("\tconst Il2CppRGCTXData* rgctx_data;\n");
    }
    methodInfoHeader.append("\tunion\n");
    methodInfoHeader.append("\t{\n");
    methodInfoHeader.append("\t\tconst void* genericMethod;\n");
    methodInfoHeader.append("\t\tconst void* genericContainerHandle;\n");
    methodInfoHeader.append("\t};\n");
    methodInfoHeader.append("\tuint32_t token;\n");
    methodInfoHeader.append("\tuint16_t flags;\n");
    methodInfoHeader.append("\tuint16_t iflags;\n");
    methodInfoHeader.append("\tuint16_t slot;\n");
    methodInfoHeader.append("\tuint8_t parameters_count;\n");
    methodInfoHeader.append("\tuint8_t bitflags;\n");
    methodInfoHeader.append("};\n");
}

std::string recursionStructInfo(const StructInfo &info) {
    if (!structCache.insert(info).second) {
        return "";
    }

    auto sb = sbldr::stringbuilder<10>{};
    auto pre = sbldr::stringbuilder<10>{};

    if (!info.Parent.empty()) {
        auto parentStructName = info.Parent + "_o";
        pre.append(recursionStructInfo(structInfoWithStructName[parentStructName]));
        sb.append("\nstruct ").append(info.TypeName).append("_Fields : ").append(info.Parent)
                .append("_Fields {\n");
    } else {
        sb.append("\nstruct ").append(info.TypeName).append("_Fields {\n");
    }
    for (const auto &field: info.Fields) {
        if (field.IsValueType) {
            auto fieldInfo = structInfoWithStructName[field.FieldTypeName];
            pre.append(recursionStructInfo(fieldInfo));
        }
        if (field.IsCustomType) {
            sb.append("\tstruct ").append(field.FieldTypeName).append(" {field.FieldName};\n");
        } else {
            sb.append("\t").append(field.FieldTypeName).append(" {field.FieldName};\n");
        }
    }
    sb.append("};\n");

    if (!info.RGCTXs.empty()) {
        sb.append("\nstruct ").append(info.TypeName).append("_RGCTXs {\n");
        for (int i = 0; i < info.RGCTXs.size(); i++) {
            auto rgctx = info.RGCTXs[i];
            switch (rgctx.Type) {
                case IL2CPP_RGCTX_DATA_TYPE:
                    sb.append("\tIl2CppType* _").append(std::to_string(i))
                            .append("_").append(rgctx.TypeName).append(";\n");
                    break;
                case IL2CPP_RGCTX_DATA_CLASS:
                    sb.append("\tIl2CppClass* _").append(std::to_string(i))
                            .append("_").append(rgctx.ClassName).append(";\n");
                    break;
                case IL2CPP_RGCTX_DATA_METHOD:
                    sb.append("\tMethodInfo* _").append(std::to_string(i))
                            .append("_").append(rgctx.MethodName).append(";\n");
                    break;
            }
        }
        sb.append("};\n");
    }

    if (!info.VTableMethod.empty()) {
        sb.append("\nstruct ").append(info.TypeName).append("_VTable {\n");
        for (int i = 0; i < info.VTableMethod.size(); i++) {
            sb.append("\tVirtualInvokeData _").append(std::to_string(i)).append("_");
            auto method = info.VTableMethod[i];
            sb.append(method.MethodName);
            sb.append(";\n");
        }
        sb.append("};\n");
    }

    sb.append("\nstruct ").append(info.TypeName).append("_c {\n");
    sb.append("\tIl2CppClass_1 _1;\n");
    if (!info.StaticFields.empty()) {
        sb.append("\tstruct ").append(info.TypeName).append("_StaticFields* static_fields;\n");
    } else {
        sb.append("\tvoid* static_fields;\n");
    }
    if (!info.RGCTXs.empty()) {
        sb.append("\t").append(info.TypeName).append("_RGCTXs* rgctx_data;\n");
    } else {
        sb.append("\tIl2CppRGCTXData* rgctx_data;\n");
    }
    sb.append("\tIl2CppClass_2 _2;\n");
    if (!info.VTableMethod.empty()) {
        sb.append("\t").append(info.TypeName).append("_VTable vtable;\n");
    } else {
        sb.append("\tVirtualInvokeData vtable[32];\n");
    }
    sb.append("};\n");

    sb.append("\nstruct ").append(info.TypeName).append("_o {\n");
    if (!info.IsValueType) {
        sb.append("\t").append(info.TypeName).append("_c *klass;\n");
        sb.append("\tvoid *monitor;\n");
    }
    sb.append("\t").append(info.TypeName).append("_Fields fields;\n");
    sb.append("};\n");

    if (!info.StaticFields.empty()) {
        sb.append("\nstruct ").append(info.TypeName).append("_StaticFields {\n");
        for (const auto &field: info.StaticFields) {
            if (field.IsValueType) {
                auto fieldInfo = structInfoWithStructName[field.FieldTypeName];
                pre.append(recursionStructInfo(fieldInfo));
            }
            if (field.IsCustomType) {
                sb.append("\tstruct ").append(field.FieldTypeName).append(" {field.FieldName};\n");
            } else {
                sb.append("\t").append(field.FieldTypeName).append(" {field.FieldName};\n");
            }
        }
        sb.append("};\n");
    }

    return pre.append(sb).str();
}

std::string
getIl2CppStructName(const Il2CppType *il2CppType, Il2CppGenericContext *context = nullptr) {
    switch (il2CppType->type) {
        case IL2CPP_TYPE_VOID:
        case IL2CPP_TYPE_BOOLEAN:
        case IL2CPP_TYPE_CHAR:
        case IL2CPP_TYPE_I1:
        case IL2CPP_TYPE_U1:
        case IL2CPP_TYPE_I2:
        case IL2CPP_TYPE_U2:
        case IL2CPP_TYPE_I4:
        case IL2CPP_TYPE_U4:
        case IL2CPP_TYPE_I8:
        case IL2CPP_TYPE_U8:
        case IL2CPP_TYPE_R4:
        case IL2CPP_TYPE_R8:
        case IL2CPP_TYPE_STRING:
        case IL2CPP_TYPE_TYPEDBYREF:
        case IL2CPP_TYPE_I:
        case IL2CPP_TYPE_U:
        case IL2CPP_TYPE_VALUETYPE:
        case IL2CPP_TYPE_CLASS:
        case IL2CPP_TYPE_OBJECT: {
            auto klass = il2cpp_class_from_type(il2CppType);
            return structNameDic[klass];
        }
        case IL2CPP_TYPE_PTR: {
            auto oriType = il2CppType->data.type;
            return getIl2CppStructName(oriType, context);
        }
        case IL2CPP_TYPE_ARRAY: {
            auto arrayType = il2CppType->data.array;
            auto elementType = arrayType->etype;
            auto elementStructName = getIl2CppStructName(elementType, context);
            auto typeStructName = elementStructName + "_array";
            if (structNameHashSet.insert(typeStructName).second) {
                parseArrayClassStruct(elementType, context);
            }
            return typeStructName;
        }
        case IL2CPP_TYPE_SZARRAY: {
            auto elementType = il2CppType->data.type;
            auto elementStructName = getIl2CppStructName(elementType, context);
            auto typeStructName = elementStructName + "_array";
            if (structNameHashSet.insert(typeStructName).second) {
                parseArrayClassStruct(elementType, context);
            }
            return typeStructName;
        }
        case IL2CPP_TYPE_GENERICINST: {
            auto typeStructName = genericClassStructNameDic[il2CppType->data.generic_class];
            if (structNameHashSet.insert(typeStructName).second) {
                genericClassList.emplace_back(il2CppType->data.generic_class);
            }
            return typeStructName;
        }
        case IL2CPP_TYPE_VAR: {
            if (context != nullptr) {
                auto genericInst = context->class_inst;
                auto type_argv = genericInst->type_argv;
                // TODO: Get correct index of generic parameter.
                auto type = type_argv[0];
                return getIl2CppStructName(type);
            }
            return "System_Object";
        }
        case IL2CPP_TYPE_MVAR: {
            if (context != nullptr) {
                auto genericInst = context->method_inst;
                auto type_argv = genericInst->type_argv;
                // TODO: Get correct index of generic parameter.
                auto type = type_argv[0];
                return getIl2CppStructName(type);
            }
            return "System_Object";
        }
        default:
            throw std::invalid_argument("Type not supported");
    }
}

std::string
parseTypeForClassStructH(const Il2CppType *il2CppType, Il2CppGenericContext *context = nullptr) {
    switch (il2CppType->type) {
        case IL2CPP_TYPE_VOID:
            return "void";
        case IL2CPP_TYPE_BOOLEAN:
            return "bool";
        case IL2CPP_TYPE_CHAR:
            return "unsigned __int16"; //Il2CppChar
        case IL2CPP_TYPE_I1:
            return "__int8";
        case IL2CPP_TYPE_U1:
            return "unsigned __int8";
        case IL2CPP_TYPE_I2:
            return "__int16";
        case IL2CPP_TYPE_U2:
            return "unsigned __int16";
        case IL2CPP_TYPE_I4:
            return "__int32";
        case IL2CPP_TYPE_U4:
            return "unsigned __int32";
        case IL2CPP_TYPE_I8:
            return "__int64";
        case IL2CPP_TYPE_U8:
            return "unsigned __int64";
        case IL2CPP_TYPE_R4:
            return "float";
        case IL2CPP_TYPE_R8:
            return "double";
        case IL2CPP_TYPE_STRING:
            return "System_String*";
        case IL2CPP_TYPE_PTR: {
            auto oriType = il2CppType->data.type;
            return parseTypeForClassStructH(oriType, context) + "*";
        }
        case IL2CPP_TYPE_VALUETYPE: {
            auto klass = il2cpp_class_from_type(il2CppType);
            auto klass_full_name = FixName(getFullTypeName(klass));
            all_type_names.insert(klass_full_name);
            return klass_full_name + "*";
        }
        case IL2CPP_TYPE_CLASS: {
            auto klass = il2cpp_class_from_type(il2CppType);
            auto klass_full_name = FixName(getFullTypeName(klass));
            all_type_names.insert(klass_full_name);
            return klass_full_name + "*";
        }
        case IL2CPP_TYPE_VAR: {
            return "Il2CppObject*";
        }
        case IL2CPP_TYPE_ARRAY: {
            auto klass = il2cpp_class_from_type(il2CppType);
            auto klass_full_name = FixName(getFullTypeName(klass));
            all_type_names.insert(klass_full_name);
            return klass_full_name + "*";
        }
        case IL2CPP_TYPE_GENERICINST: {
            auto genericClass = il2CppType->data.generic_class;
            auto klass = il2cpp_class_from_type(il2CppType);
            auto klass_full_name = FixName(getFullTypeName(klass));
            all_type_names.insert(klass_full_name);
            return klass_full_name + "*";
        }
        case IL2CPP_TYPE_TYPEDBYREF:
            return "Il2CppObject*";
        case IL2CPP_TYPE_I:
            return "int*";
        case IL2CPP_TYPE_U:
            return "uint*";
        case IL2CPP_TYPE_OBJECT:
            return "Il2CppObject*";
        case IL2CPP_TYPE_SZARRAY: {
            auto elementType = il2CppType->data.type;
            auto klass = il2cpp_class_from_type(elementType);
            auto klass_full_name = FixName(getFullTypeName(klass)).append("_array");
            all_type_names.insert(klass_full_name);
            return klass_full_name + "*";
        }
        case IL2CPP_TYPE_MVAR: {
            return "Il2CppObject*";
        }
        default:
            throw std::invalid_argument("Type not supported");
    }
}

std::string parseType(const Il2CppType *il2CppType, Il2CppGenericContext *context = nullptr) {
    switch (il2CppType->type) {
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
            auto oriType = il2CppType->data.type;
            return parseType(oriType, context) + "*";
        }
        case IL2CPP_TYPE_VALUETYPE: {
            auto klass = il2cpp_class_from_type(il2CppType);
//            auto is_enum = il2cpp_class_is_enum(klass);
//            if (is_enum) {
//                auto oriType = *il2CppType.data.type;
//                return parseType(oriType, context);
//            }
            auto klass_name = il2cpp_class_get_name(klass);
            return FixName(klass_name) + "_o";
        }
        case IL2CPP_TYPE_CLASS: {
            auto klass = il2cpp_class_from_type(il2CppType);
            auto klass_name = il2cpp_class_get_name(klass);
            return FixName(klass_name) + "_o*";
        }
        case IL2CPP_TYPE_VAR: {
//            if (context != nullptr) {
//                auto genericParameter = executor.GetGenericParameteFromIl2CppType(il2CppType);
//                auto genericInst = il2Cpp.MapVATR<Il2CppGenericInst>(context.class_inst);
//                auto pointers = il2Cpp.MapVATR<ulong>(genericInst.type_argv, genericInst.type_argc);
//                auto pointer = pointers[genericParameter.num];
//                auto type = il2Cpp.GetIl2CppType(pointer);
//                return ParseType(type);
//            }
            return "Il2CppObject*";
        }
        case IL2CPP_TYPE_ARRAY: {
//            auto arrayType = il2CppType.data.array;
//            auto elementType = il2Cpp.GetIl2CppType(arrayType->etype);
//            auto elementStructName = GetIl2CppStructName(elementType, context);
//            auto typeStructName = elementStructName + "_array";
//            if (structNameHashSet.Add(typeStructName)) {
//                ParseArrayClassStruct(elementType, context);
//            }
            auto klass = il2cpp_class_from_type(il2CppType);
            auto klass_name = il2cpp_class_get_name(klass);
            return FixName(klass_name) + "*";
        }
        case IL2CPP_TYPE_GENERICINST: {
            auto genericClass = il2CppType->data.generic_class;
            auto klass = il2cpp_class_from_type(il2CppType);
            auto klass_name = il2cpp_class_get_name(klass);
            return FixName(klass_name) + "_o*";
//            auto typeDef = executor.GetGenericClassTypeDefinition(genericClass);
//            auto typeStructName = genericClassStructNameDic[il2CppType.data.generic_class];
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
            auto elementType = il2CppType->data.type;
            auto klass = il2cpp_class_from_type(elementType);
            auto klass_name = il2cpp_class_get_name(klass);
            return FixName(klass_name) + "_array*";
        }
        case IL2CPP_TYPE_MVAR: {
//            if (context != nullptr) {
//                auto genericParameter = executor.GetGenericParameteFromIl2CppType(il2CppType);
//                //https://github.com/Perfare/Il2CppDumper/issues/687
//                if (context.method_inst == 0 && context.class_inst != 0) {
//                    goto
//                    case Il2CppTypeEnum.IL2CPP_TYPE_VAR;
//                }
//                auto genericInst = il2Cpp.MapVATR<Il2CppGenericInst>(context.method_inst);
//                auto pointers = il2Cpp.MapVATR<ulong>(genericInst.type_argv, genericInst.type_argc);
//                auto pointer = pointers[genericParameter.num];
//                auto type = il2Cpp.GetIl2CppType(pointer);
//                return ParseType(type);
//            }
            return "Il2CppObject*";
        }
        default:
            throw std::invalid_argument("Type not supported");
    }
}

void parseArrayClassStruct(const Il2CppType *il2CppType, Il2CppGenericContext *context) {
    auto structName = getIl2CppStructName(il2CppType, context);
    auto simpleType = parseType(il2CppType, context);
    arrayClassHeader
            .append("struct ").append(structName).append("_array {\n")
            .append("\tIl2CppObject obj;\n")
            .append("\tIl2CppArrayBounds *bounds;\n")
            .append("\til2cpp_array_size_t max_length;\n")
            .append("\t").append(simpleType).append(" m_Items[65535];\n").append("};\n");
}

void PrintIl2CppType(const Il2CppType *il2CppType) {
    LOGI("data.dummy: %p", il2CppType->data.dummy);
    LOGI("data.klassIndex: %d", il2CppType->data.klassIndex);
    LOGI("data.type: %p", il2CppType->data.type);
    LOGI("data.type type: 0x%x", il2CppType->data.type->type);
    LOGI("data.genericParameterIndex: %d", il2CppType->data.genericParameterIndex);
    auto generic_class = il2CppType->data.generic_class;
    LOGI("data.generic_class: %p", generic_class);
    if (generic_class != nullptr) {
        LOGI("data.generic_class type: %p", (void *) (generic_class->type));
        auto cached_class = generic_class->cached_class;
        LOGI("data.generic_class cached_class: %p", cached_class);
//        if (cached_class != nullptr) {
//            LOGI("data.generic_class cached_class name: %s", cached_class->name);
//        }
    }
    LOGI("attrs: %u", il2CppType->attrs);
    LOGI("type: 0x%x", il2CppType->type);
    LOGI("num_mods: %u", il2CppType->num_mods);
    LOGI("byref: %u", il2CppType->byref);
    LOGI("pinned: %u", il2CppType->pinned);
}

void PrintFieldInfo(const FieldInfo *fieldInfo) {
    LOGI("fieldInfo name: %s", fieldInfo->name);
    LOGI("fieldInfo type: %p", fieldInfo->type);
    PrintIl2CppType(fieldInfo->type);
    auto parent = fieldInfo->parent;
    LOGI("fieldInfo parent: %p", parent);
    if (parent != nullptr) {
        LOGI("fieldInfo parent name: %s", parent->name);
    }
    LOGI("fieldInfo offset: %d", fieldInfo->offset);
    LOGI("fieldInfo token: %u", fieldInfo->token);
}

void PrintIl2CppTypeDefinition(const Il2CppTypeDefinition *il2CppTypeDefinition) {
    LOGI("nameIndex: %u", il2CppTypeDefinition->nameIndex);
    LOGI("namespaceIndex: %u", il2CppTypeDefinition->namespaceIndex);
    LOGI("byvalTypeIndex: %d", il2CppTypeDefinition->byvalTypeIndex);
    LOGI("declaringTypeIndex: %d", il2CppTypeDefinition->declaringTypeIndex);
    LOGI("parentIndex: %d", il2CppTypeDefinition->parentIndex);
    LOGI("elementTypeIndex: %d", il2CppTypeDefinition->elementTypeIndex);
    LOGI("genericContainerIndex: %d", il2CppTypeDefinition->genericContainerIndex);
    LOGI("flags: %u", il2CppTypeDefinition->flags);
    LOGI("fieldStart: %u", il2CppTypeDefinition->fieldStart);
    LOGI("methodStart: %u", il2CppTypeDefinition->methodStart);
    LOGI("eventStart: %d", il2CppTypeDefinition->eventStart);
    LOGI("propertyStart: %d", il2CppTypeDefinition->propertyStart);
    LOGI("nestedTypesStart: %d", il2CppTypeDefinition->nestedTypesStart);
    LOGI("interfacesStart: %d", il2CppTypeDefinition->interfacesStart);
    LOGI("vtableStart: %d", il2CppTypeDefinition->vtableStart);
    LOGI("interfaceOffsetsStart: %d", il2CppTypeDefinition->interfaceOffsetsStart);
    LOGI("method_count: %hu", il2CppTypeDefinition->method_count);
    LOGI("property_count: %hu", il2CppTypeDefinition->property_count);
    LOGI("field_count: %hu", il2CppTypeDefinition->field_count);
    LOGI("event_count: %hu", il2CppTypeDefinition->event_count);
    LOGI("nested_type_count: %hu", il2CppTypeDefinition->nested_type_count);
    LOGI("vtable_count: %hu", il2CppTypeDefinition->vtable_count);
    LOGI("interfaces_count: %hu", il2CppTypeDefinition->interfaces_count);
    LOGI("interface_offsets_count: %hu", il2CppTypeDefinition->interface_offsets_count);
    LOGI("bitfield: %u", il2CppTypeDefinition->bitfield);
    LOGI("token: %u", il2CppTypeDefinition->token);
}

int declaring_type_count = 0;

void PrintIl2CppClassShort(const Il2CppClass *il2CppClass) {
    auto name = il2CppClass->name;
    auto namespaze = il2CppClass->namespaze;
    auto declaringType = il2CppClass->declaringType;
    auto declaringTypeName = declaringType == nullptr ? "" : declaringType->name;
    if (declaringType != nullptr && namespaze[0] != '\0') {
        LOGI("name: %s, namespace: %s, declaringType: %p, declaringTypeName: %s", name, namespaze,
             declaringType, declaringTypeName);
        declaring_type_count++;
    }
}

void PrintIl2CppClass(const Il2CppClass *il2CppClass) {
    LOGI("--------- Start Print ---------");
    LOGI("Il2CppClass address: %p", il2CppClass);
    LOGI("image: %p, image name: %s", il2CppClass->image,
         il2cpp_image_get_name(il2CppClass->image));
    LOGI("gc_desc: %p", il2CppClass->gc_desc);
    LOGI("name: %s", il2CppClass->name);
    LOGI("namespaze: %s", il2CppClass->namespaze);
    auto byval_arg_ptr = &(il2CppClass->byval_arg);
    LOGI("byval_arg: %p", byval_arg_ptr);
    LOGI("--------- Print byval_arg ---------");
    PrintIl2CppType(byval_arg_ptr);
    LOGI("--------- Print byval_arg end ---------");
    auto this_arg_ptr = &(il2CppClass->this_arg);
    LOGI("this_arg: %p", this_arg_ptr);
    LOGI("--------- Print this_arg ---------");
    PrintIl2CppType(this_arg_ptr);
    LOGI("--------- Print this_arg end ---------");
    auto element_class = il2CppClass->element_class;
    LOGI("element_class: %p", element_class);
    if (element_class != nullptr) {
        LOGI("element_class name: %s", element_class->name);
    }
    auto castClass = il2CppClass->castClass;
    LOGI("castClass: %p", castClass);
    if (castClass != nullptr) {
        LOGI("castClass name: %s", castClass->name);
    }
    auto declaringType = il2CppClass->declaringType;
    LOGI("declaringType: %p", declaringType);
    if (declaringType != nullptr) {
        LOGI("declaringType name: %s", declaringType->name);
    }
    auto parent = il2CppClass->parent;
    LOGI("parent: %p", parent);
    if (parent != nullptr) {
        LOGI("parent name: %s", parent->name);
    }
    auto generic_class = il2CppClass->generic_class;
    LOGI("generic_class: %p", generic_class);
    if (generic_class != nullptr) {
        auto generic_type_definition = (void *) (generic_class->type);
        LOGI("generic_class type: %p", generic_type_definition);
//        if (generic_type_definition != nullptr) {
//            LOGI("--------- Print generic_type_definition ---------");
//            PrintIl2CppTypeDefinition(generic_type_definition);
//            LOGI("--------- Print generic_type_definition end ---------");
//        }
        auto context = generic_class->context;
        auto class_inst = context.class_inst;
        LOGI("context class_inst: %p", class_inst);
        if (class_inst != nullptr) {
            LOGI("--------- Print class_inst ---------");
            auto type_argc = class_inst->type_argc;
            LOGI("type_argc: %u", type_argc);
            auto type_argv = class_inst->type_argv;
            LOGI("type_argv: %p", type_argv);
            if (type_argv != nullptr) {
                for (int i = 0; i < type_argc; i++) {
                    LOGI("type_argv_%d: %p", i, type_argv[i]);
                    auto klass = il2cpp_class_from_type(type_argv[i]);
                    auto klass_name = il2cpp_class_get_name(klass);
                    LOGI("klass name: %s", klass_name);
                }
            }
            LOGI("--------- Print class_inst end ---------");
        }

        auto method_inst = context.method_inst;
        LOGI("context method_inst: %p", method_inst);
        if (method_inst != nullptr) {
            LOGI("--------- Print method_inst ---------");
            auto type_argc = method_inst->type_argc;
            LOGI("type_argc: %u", type_argc);
            auto type_argv = method_inst->type_argv;
            LOGI("type_argv: %p", type_argv);
            if (type_argv != nullptr) {
                for (int i = 0; i < type_argc; i++) {
                    LOGI("type_argv_%d: %p", i, type_argv[i]);
                    auto klass = il2cpp_class_from_type(type_argv[i]);
                    auto klass_name = il2cpp_class_get_name(klass);
                    LOGI("klass name: %s", klass_name);
                }
            }
            LOGI("--------- Print method_inst end ---------");
        }

        auto cached_class = generic_class->cached_class;
        LOGI("generic_class cached_class: %p", cached_class);
        if (cached_class != nullptr) {
            LOGI("generic_class cached_class name: %s", cached_class->name);
        }
    }
    auto typeMetadataHandle = il2CppClass->typeMetadataHandle;
    LOGI("typeMetadataHandle: %p", typeMetadataHandle);
    if (typeMetadataHandle != nullptr) {
        LOGI("--------- Print typeMetadataHandle ---------");
        PrintIl2CppTypeDefinition(il2CppClass->typeMetadataHandle);
        LOGI("--------- Print typeMetadataHandle end ---------");
    }
    auto interopData = il2CppClass->interopData;
    LOGI("interopData: %p", interopData);
    if (interopData != nullptr) {
        LOGI("interopData type: %p", interopData->type);
        LOGI("--------- Print interopData type ---------");
        PrintIl2CppType(interopData->type);
        LOGI("--------- Print interopData type end ---------");
    }
    auto klass = il2CppClass->klass;
    LOGI("klass: %p", klass);
    if (klass != nullptr) {
        LOGI("klass name: %s", klass->name);
    }
    auto fields = il2CppClass->fields;
    LOGI("fields: %p", fields);
    if (fields != nullptr) {
        LOGI("--------- Print fields ---------");
        PrintFieldInfo(il2CppClass->fields);
        LOGI("--------- Print fields end ---------");
    }
    LOGI("events: %p", il2CppClass->events);
    LOGI("properties: %p", il2CppClass->properties);
    LOGI("methods: %p", il2CppClass->methods);
    auto nestedTypes = il2CppClass->nestedTypes;
    LOGI("nestedTypes: %p", nestedTypes);
    if (nestedTypes != nullptr && *nestedTypes != nullptr) {
        LOGI("nestedTypes name: %s", (*nestedTypes)->name);
    }
    auto implementedInterfaces = il2CppClass->implementedInterfaces;
    LOGI("implementedInterfaces: %p", implementedInterfaces);
    if (implementedInterfaces != nullptr && *implementedInterfaces != nullptr) {
        LOGI("implementedInterfaces name: %s", (*implementedInterfaces)->name);
    }
    auto interfaceOffsets = il2CppClass->interfaceOffsets;
    LOGI("interfaceOffsets: %p", interfaceOffsets);
    if (interfaceOffsets != nullptr && interfaceOffsets->interfaceType != nullptr) {
        LOGI("interfaceOffsets itf name: %s", interfaceOffsets->interfaceType->name);
    }
    LOGI("static_fields: %p", il2CppClass->static_fields);
    LOGI("rgctx_data: %p", il2CppClass->rgctx_data);

    auto typeHierarchy = il2CppClass->typeHierarchy;
    LOGI("typeHierarchy: %p", typeHierarchy);
    if (typeHierarchy != nullptr && *typeHierarchy != nullptr) {
        LOGI("typeHierarchy name1: %s", (*typeHierarchy)->name);
//        LOGI("typeHierarchy name2: %s", (*(il2CppClass->typeHierarchy) + 0x8)->name);
//        LOGI("typeHierarchy name3: %s", (*(il2CppClass->typeHierarchy) + 0x10)->name);
//        LOGI("typeHierarchy name4: %s", (*(il2CppClass->typeHierarchy) + 0x18)->name);
    }
    LOGI("unity_user_data: %p", il2CppClass->unity_user_data);
    LOGI("initializationExceptionGCHandle: %u", il2CppClass->initializationExceptionGCHandle);
    LOGI("cctor_started: %u", il2CppClass->cctor_started);
    LOGI("cctor_finished: %u", il2CppClass->cctor_finished);
    LOGI("cctor_thread: %zu", il2CppClass->cctor_thread);
    LOGI("genericContainerHandle: %p", il2CppClass->genericContainerHandle);
    LOGI("instance_size: 0x%x", il2CppClass->instance_size);
    LOGI("actualSize: 0x%x", il2CppClass->actualSize);
    LOGI("element_size: 0x%x", il2CppClass->element_size);
    LOGI("placeholder: 0x%x", il2CppClass->placeholder);
    LOGI("native_size: 0x%x", il2CppClass->native_size);
    LOGI("static_fields_size: 0x%x", il2CppClass->static_fields_size);
    LOGI("thread_static_fields_size: 0x%x", il2CppClass->thread_static_fields_size);
    LOGI("thread_static_fields_offset: 0x%x", il2CppClass->thread_static_fields_offset);
    LOGI("flags: 0x%x", il2CppClass->flags);
    LOGI("token: 0x%x", il2CppClass->token);
    LOGI("method_count: %hu", il2CppClass->method_count);
    LOGI("property_count: %hu", il2CppClass->property_count);
    LOGI("field_count: %hu", il2CppClass->field_count);
    LOGI("event_count: %hu", il2CppClass->event_count);
    LOGI("nested_type_count: %hu", il2CppClass->nested_type_count);
    LOGI("vtable_count: %hu", il2CppClass->vtable_count);
    LOGI("interfaces_count: %hu", il2CppClass->interfaces_count);
    LOGI("interface_offsets_count: %hu", il2CppClass->interface_offsets_count);
    LOGI("typeHierarchyDepth: %hhu", il2CppClass->typeHierarchyDepth);
    LOGI("genericRecursionDepth: %hhu", il2CppClass->genericRecursionDepth);
    LOGI("rank: %hhu", il2CppClass->rank);
    LOGI("minimumAlignment: %hhu", il2CppClass->minimumAlignment);
    LOGI("naturalAligment: %hhu", il2CppClass->naturalAligment);
    LOGI("packingSize: %hhu", il2CppClass->packingSize);
    LOGI("bitflags1: %hhu", il2CppClass->bitflags1);
    LOGI("bitflags2: %hhu", il2CppClass->bitflags2);

    LOGI("--------- End Print ---------");
}

std::string getFullTypeName(const Il2CppClass *klass) {
    std::vector<std::string> declaringTypeNames;
    std::vector<std::string> genericParameterNames;
    std::string fullTypeName;

    // Iterate declaringTypes.
    auto declaringType = klass;
    auto is_generic = il2cpp_class_is_generic(klass);
    while (declaringType != nullptr) {
        // Add declaringTypeName.
        std::string declaringTypeName = declaringType->name;
//        LOGI("declaringTypeName: %s", declaringTypeName.c_str());
        if (declaringTypeName.find('`') != std::string::npos) {
            std::vector<std::string> tokens = StringSplit(declaringTypeName, "`");
            declaringTypeName = tokens[0];
            auto param_count = std::stoi(tokens[1]);
            // TODO: Get the real type of generic parameters.
            if (is_generic) {
                for (auto i = 0; i < param_count; i++) {
                    auto paramName = std::string("T").append(std::to_string(i));
                    genericParameterNames.emplace_back(paramName);
                }
            } else {
                auto generic_class = klass->generic_class;
                if (generic_class != nullptr) {
                    if (generic_class != nullptr) {
                        auto context = generic_class->context;
                        auto class_inst = context.class_inst;
                        if (class_inst != nullptr) {
                            auto type_argc = class_inst->type_argc;
                            auto type_argv = class_inst->type_argv;
                            if (type_argv != nullptr) {
                                for (int i = 0; i < type_argc; i++) {
                                    auto klass = il2cpp_class_from_type(
                                            type_argv[type_argc - 1 - i]);
                                    auto parameterName = getFullTypeName(klass);
                                    genericParameterNames.emplace_back(parameterName);
                                }
                            }
                        }
                    }
                } else { // TODO: Generic instance arrays.
                }
            }
        }
//        if (!is_generic) {
//            auto generic_class = klass->generic_class;
//            if (generic_class != nullptr) {
//                if (generic_class != nullptr) {
//                    auto context = generic_class->context;
//                    auto class_inst = context.class_inst;
//                    if (class_inst != nullptr) {
//                        auto type_argc = class_inst->type_argc;
//                        auto type_argv = class_inst->type_argv;
//                        if (type_argv != nullptr) {
//                            for (int i = 0; i < type_argc; i++) {
//                                auto klass = il2cpp_class_from_type(type_argv[type_argc - 1 - i]);
//                                auto parameterName = getFullTypeName(klass);
//                                genericParameterNames.emplace_back(parameterName);
//                            }
//                        }
//                    }
//                }
//            }
//        }
        declaringTypeNames.emplace_back(declaringTypeName);
        auto nextDeclaringType = declaringType->declaringType;
        if (nextDeclaringType == nullptr) {
            std::string namespaze = declaringType->namespaze;
            if (!namespaze.empty()) {
                declaringTypeNames.emplace_back(namespaze);
            }
        }
        declaringType = nextDeclaringType;
    }

    std::reverse(declaringTypeNames.begin(), declaringTypeNames.end());
    auto declaringTypeNamesString = StringJoin(declaringTypeNames, ".");
    std::string genericParameterNamesString;
    if (!genericParameterNames.empty()) {
        if (!is_generic) {
            std::reverse(genericParameterNames.begin(), genericParameterNames.end());
        } else {
            if (genericParameterNames.size() == 1) {
                genericParameterNames[0] = "T";
            }
        }
        genericParameterNamesString = "<" + StringJoin(genericParameterNames, ", ") + ">";
    }
    fullTypeName = declaringTypeNamesString + genericParameterNamesString;
    return fullTypeName;
}

std::string getUniqueName(const std::string &name) {
    auto fixName = name;
    int i = 1;
    while (!structNameHashSet.insert(fixName).second) {
        fixName = name + std::to_string(i++);
    }
    return fixName;
}

void createStructNameDic(Il2CppClass *klass) {
    auto typeName = getFullTypeName(klass);
    auto typeStructName = FixName(typeName);
    auto uniqueName = getUniqueName(typeStructName);
    structNameDic[klass] = uniqueName;
}

void createGenericClassStructNameDic(Il2CppClass *klass) {
    auto generic_class = klass->generic_class;
    if (generic_class == nullptr) {
        return;
    }
    auto typeStructName = structNameDic[klass];
    auto il2CppType = &klass->this_arg;
    nameGenericClassDic[typeStructName] = il2CppType;
    genericClassStructNameDic[generic_class] = typeStructName;
}

void addParents(Il2CppClass *klass, StructInfo structInfo) {
    if (!il2cpp_class_is_valuetype(klass) && !il2cpp_class_is_enum(klass)) {
        auto parent = klass->parent;
        auto parent_type = il2cpp_class_get_type(klass);
        if (parent_type->type != IL2CPP_TYPE_OBJECT) {
            structInfo.Parent = getIl2CppStructName(parent_type);
        }
    }
}

void addFields(Il2CppClass *klass, StructInfo structInfo, Il2CppGenericContext *context = nullptr) {
    void *iter = nullptr;
    auto field_index = 0;
    std::set<std::string> cache;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        auto attrs = il2cpp_field_get_flags(field);
        if ((attrs & FIELD_ATTRIBUTE_LITERAL) != 0) {
            continue;
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        StructFieldInfo structFieldInfo;
        structFieldInfo.FieldTypeName = parseType(field_type, context);
        auto field_name = FixName(il2cpp_field_get_name(field));
        if (!cache.insert(field_name).second) {
            field_name = std::string("_");
            field_name.append(std::to_string(field_index)).append("_").append(field_name);
        }
        structFieldInfo.FieldName = field_name;
        structFieldInfo.IsValueType = il2cpp_class_is_valuetype(field_class);
        structFieldInfo.IsCustomType = isCustomType(field_type, context);

        if ((attrs & FIELD_ATTRIBUTE_STATIC) != 0) {
            structInfo.StaticFields.emplace_back(structFieldInfo);
        } else {
            structInfo.Fields.emplace_back(structFieldInfo);
        }

        field_index++;
    }
}

void addVTableMethod(Il2CppClass *klass, StructInfo structInfo) {
    std::map<int, int *> dic;
    auto method_count = klass->vtable_count;
    if (method_count > 0) {
        structInfo.VTableMethod = std::vector<StructVTableMethodInfo>(method_count);
        auto vtable = klass->vtable;
        if (vtable != nullptr) {
            for (int i = 0; i < method_count; i++) {
                StructVTableMethodInfo structVTableMethodInfo;
                auto methodInfo = vtable[i].method;
                if (methodInfo == nullptr) {
                    continue;
                }
                auto methodName = il2cpp_method_get_name(methodInfo);
                if (methodName != nullptr) {
                    auto FixedMethodName = FixName(methodName);
                    structVTableMethodInfo.MethodName = FixedMethodName;
                    structInfo.VTableMethod[i] = structVTableMethodInfo;
                }
            }
        }
    }
}

// TODO: Fix RGCTX.
void addRGCTX(Il2CppClass *klass, StructInfo structInfo) {
    auto rgctx_data = klass->rgctx_data;
    if (rgctx_data != nullptr) {
        StructRGCTXInfo structRgctxInfo;
        auto rgctx_type = rgctx_data->type;
        structRgctxInfo.TypeName = getIl2CppStructName(rgctx_type);
        auto rgctx_klass = rgctx_data->klass;
        structRgctxInfo.ClassName = getIl2CppStructName(&rgctx_klass->this_arg);
        auto rgctx_method = rgctx_data->method;
        structRgctxInfo.MethodName = FixName(il2cpp_method_get_name(rgctx_method));
        structInfo.RGCTXs.emplace_back(structRgctxInfo);
    }
}

void add_struct(Il2CppClass *klass) {
    StructInfo structInfo;
    structInfo.TypeName = structNameDic[klass];
    structInfo.IsValueType = il2cpp_class_is_valuetype(klass);
    addParents(klass, structInfo);

    addFields(klass, structInfo);

    addVTableMethod(klass, structInfo);

    addRGCTX(klass, structInfo);

    structInfoList.emplace_back(structInfo);
}

void addGenericClassStruct(Il2CppGenericClass *generic_class) {
    StructInfo structInfo;
    auto cached_class = generic_class->cached_class;
    if (cached_class == nullptr) {
        LOGI("Empty cached_class");
        return;
    }
    structInfo.TypeName = genericClassStructNameDic[generic_class];
    structInfo.IsValueType = il2cpp_class_is_valuetype(cached_class);
    addParents(cached_class, structInfo);
    addFields(cached_class, structInfo, &generic_class->context);
    addVTableMethod(cached_class, structInfo);
    structInfoList.emplace_back(structInfo);
}

std::string GetMethodTypeSignature(const std::vector<Il2CppTypeEnum> &types) {
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
    auto classFullName = getFullTypeName(klass);
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
        auto return_class_full_name = getFullTypeName(return_class);
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
            auto parameter_class_full_name = getFullTypeName(parameter_class);
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
            auto prop_class_full_name = getFullTypeName(prop_class);
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
        auto field_class_full_name = getFullTypeName(field_class);
        std::string field_name = il2cpp_field_get_name(field);
        if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            if (keywords.contains(field_name)) {
                auto firstChar = field_name[0];
                if (firstChar == '_') {
                    field_name = "_" + field_name;
                } else {
                    field_name[0] = static_cast<char>(std::toupper(
                            static_cast<unsigned char>(field_name[0])));
                }
            }
        }
        outPut << field_class_full_name << " " << field_name;
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

std::string dump_field_for_class_struct_h(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n";
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;
    if (is_enum) {
        std::string enum_type;
        auto field_index = 0;
        auto field_count = klass->field_count;
        while (auto field = il2cpp_class_get_fields(klass, &iter)) {
            // Get enum type.
            if (enum_type.empty()) {
                auto field_type = il2cpp_field_get_type(field);
                enum_type = parseTypeForClassStructH(field_type);
                continue;
            }
            std::string field_name = il2cpp_field_get_name(field);
            auto isChineseString = containsChineseRegex(field_name);
            if (isChineseString) {
                auto field_name_hex_encoded = encodeToVariableName(field_name);
                outPut << "\t" << field_name_hex_encoded;
            } else {
                if (keywords.contains(field_name)) {
                    auto firstChar = field_name[0];
                    if (firstChar == '_') {
                        field_name = "_" + field_name;
                    } else {
                        field_name[0] = static_cast<char>(std::toupper(
                                static_cast<unsigned char>(field_name[0])));
                    }
                }
                outPut << "\t" << field_name;
            }
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
            // Except __value.
            if (field_index < field_count - 2) {
                if (isChineseString) {
                    outPut << ", // " << field_name << "\n";
                } else {
                    outPut << ",\n";
                }
            } else {
                if (isChineseString) {
                    outPut << " // " << field_name << "\n";
                } else {
                    outPut << "\n";
                }
            }
            field_index++;
        }
    } else {
        while (auto field = il2cpp_class_get_fields(klass, &iter)) {
            auto attrs = il2cpp_field_get_flags(field);
            if (attrs & FIELD_ATTRIBUTE_LITERAL || attrs & FIELD_ATTRIBUTE_STATIC) {
                continue;
            }
            outPut << "\t";
            auto field_type = il2cpp_field_get_type(field);
            auto field_name = il2cpp_field_get_name(field);
            auto type_name = parseTypeForClassStructH(field_type);
            outPut << type_name << " " << FixName(field_name);
            outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
        }
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

    auto className = il2cpp_class_get_name(klass);
    auto is_generic = il2cpp_class_is_generic(klass);
    auto class_full_name = getFullTypeName(klass);
    outPut << class_full_name; //TODO genericContainerIndex
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);
        auto parent_full_name = getFullTypeName(parent);
        extends.emplace_back(parent_full_name);
    }
    void *iter = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
        auto interface_full_name = getFullTypeName(itf);
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

void dump_type_for_class_struct_h(const Il2CppType *type) {
    std::stringstream outPut;
    auto *klass = il2cpp_class_from_type(type);
    auto is_generic = il2cpp_class_is_generic(klass);

    auto flags = il2cpp_class_get_flags(klass);
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    std::string type_short_name;
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        type_short_name = "interface";
    } else if (is_enum) {
        type_short_name = "enum";
    } else if (is_valuetype) {
        type_short_name = "struct";
    } else {
        type_short_name = "class";
    }

    // No field for interface.
    if (type_short_name == "interface") {
        return;
    } else if (type_short_name == "enum") {
        outPut << "enum ";
    } else {
        outPut << "struct ";
    }

    auto className = il2cpp_class_get_name(klass);
    auto class_full_name = FixName(getFullTypeName(klass));
    if (type_short_name != "enum") {
        all_type_names.insert(class_full_name);
    }
    outPut << class_full_name;
    auto parent = il2cpp_class_get_parent(klass);
    std::string parent_full_name;
    if (!is_valuetype && !is_enum && parent) {
        parent_full_name = FixName(getFullTypeName(parent));
        auto parent_ori_full_name = getFullTypeName(parent);
        auto parent_type = il2cpp_class_get_type(parent);
        auto is_generic_inst = parent_type->type == IL2CPP_TYPE_GENERICINST;
        auto is_generic = il2cpp_class_is_generic(parent);

        if (is_generic_inst) {
            std::string parent_name = parent->name;
            int generic_params_count;
            std::string parent_short_name;
            if (parent_name.find('`') != std::string::npos) {
                std::vector<std::string> tokens = StringSplit(parent_name, "`");
                parent_short_name = tokens[0];
                generic_params_count = std::stoi(tokens[1]);
            } else {
                parent_short_name = parent_name;
                generic_params_count = parseTemplateParams(parent_ori_full_name).size();
            }
            auto end_pos = parent_ori_full_name.find('<');
            auto parent_generic_class_name = FixName(parent_ori_full_name.substr(0, end_pos));
            if (generic_params_count == 1) {
                parent_generic_class_name.append("_T_");
            } else {
                for (auto i = 0; i < generic_params_count; i++) {
                    parent_generic_class_name.append("_T").append(std::to_string(i)).append("_");
                }
            }
            all_generic_instances.insert({parent_full_name, parent_generic_class_name});
        }
        all_type_names.insert(parent_full_name);
        outPut << " : " << parent_full_name;
    }

    outPut << "\n{";
    if (is_generic && !is_enum) {
        outPut << "\n";
    } else {
        outPut << dump_field_for_class_struct_h(klass);
    }
    outPut << "};\n\n";
    ClassStruct classStruct = {parent_full_name, outPut.str()};
    allClasses[class_full_name] = classStruct;
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
    auto outPath = std::string(outDir).append("/files/Il2Cpp/dump.cs");
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
//                auto className = il2cpp_class_get_name(klass);
                auto classFullName = getFullTypeName(klass);
                auto methodName = il2cpp_method_get_name(method);
                auto methodFullNameString =
                        std::string(FixName(classFullName)) + "$$" + std::string(methodName);
//                LOGI("MethodFullName: %s", methodFullNameString.c_str());
                rapidjson::Value methodFullName;
                methodFullName.SetString(methodFullNameString.c_str(), allocator);
                methodObject.AddMember("Name", methodFullName, allocator);

                // Apply signature.
                auto return_type = il2cpp_method_get_return_type(method);
                auto return_class = il2cpp_class_from_type(return_type);
                auto returnType = parseType(return_type);
                if (return_type->byref == 1) {
                    returnType += "*";
                }
                auto fixedName = FixName(methodFullNameString);
                auto signatureString = returnType.append(" ").append(fixedName).append(" (");
                std::vector<std::string> parameterStrs;
                uint32_t iflags = 0;
                auto flags = il2cpp_method_get_flags(method, &iflags);
                // Add this param.
                if ((flags & METHOD_ATTRIBUTE_STATIC) == 0) {
                    auto klass_type = parseType(type);
                    parameterStrs.emplace_back(klass_type + " __this");
                }
                // Add other params.
                auto param_count = il2cpp_method_get_param_count(method);
                for (auto k = 0; k < param_count; k++) {
                    auto param = il2cpp_method_get_param(method, k);
//                    auto parameter_class = il2cpp_class_from_type(param);
//                    auto parameterType = il2cpp_class_get_name(parameter_class);
                    auto parameterCType = parseType(param);
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
    auto outPath = std::string(outDir).append("/files/Il2Cpp/script.json");
    FILE *fp = fopen(outPath.c_str(), "w");
    char writeBuffer[65536];
    rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
    d.Accept(writer);
    fclose(fp);

    LOGI("dump script.json done!");
}

// TODO: fix headerStruct for class field.
void il2cpp_dump_il2cpp_h(const char *outDir) {
    LOGI("dumping il2cpp.h ...");
    auto sb = sbldr::stringbuilder<10>{};
    sb.append(HeaderConstants::GenericHeader);
    sb.append(HeaderConstants::HeaderV29);

    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);

    // Init structNameDic.
    // Iterate images.
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        auto classCount = il2cpp_image_get_class_count(image);
        // Iterate classes.
        for (int j = 0; j < classCount; ++j) {
            auto const_klass = il2cpp_image_get_class(image, j);
            auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(const_klass));
            auto *klass = il2cpp_class_from_type(type);
            createStructNameDic(klass);
        }
    }

    // Init genericClassStructNameDic.
    // Iterate images.
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        auto classCount = il2cpp_image_get_class_count(image);
        // Iterate classes.
        for (int j = 0; j < classCount; ++j) {
            auto const_klass = il2cpp_image_get_class(image, j);
            auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(const_klass));
            auto *klass = il2cpp_class_from_type(type);
            createGenericClassStructNameDic(klass);
        }
    }

    // Iterate images.
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        auto classCount = il2cpp_image_get_class_count(image);
        // Iterate classes.
        for (int j = 0; j < classCount; ++j) {
            auto const_klass = il2cpp_image_get_class(image, j);
            auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(const_klass));
            auto *klass = il2cpp_class_from_type(type);
            add_struct(klass);

            // Iterate methods.
            void *iter = nullptr;
            while (auto method = il2cpp_class_get_methods(klass, &iter)) {
                auto is_generic = il2cpp_method_is_generic(method);
                if (is_generic) {
                    auto method_pointer = method->methodPointer;
//                    uint64_t rva = (uint64_t) method->methodPointer - il2cpp_base;
                    std::stringstream ss;
                    ss << std::uppercase << std::hex << (uint64_t) method_pointer;
                    auto methodInfoName = "MethodInfo_" + ss.str();
                    auto structTypeName = structNameDic[klass];
//                    auto rgctxs = GenerateRGCTX(imageName, methodDef);
                    if (methodInfoCache.insert(method_pointer).second) {
                        generateMethodInfo(methodInfoName, structTypeName);
                    }
                }
            }
        }
    }

    for (const auto &generic_class: genericClassList) {
        addGenericClassStruct(generic_class);
    }

    auto headerStruct = sbldr::stringbuilder<10>{};
    for (const auto &structInfo: structInfoList) {
        structInfoWithStructName[structInfo.TypeName + "_o"] = structInfo;
    }

    for (const auto &structInfo: structInfoList) {
        auto structInfoString = recursionStructInfo(structInfo);
        headerStruct.append(structInfoString);
    }

    sb.append(headerStruct);
    // TODO: Fix arrayClassHeader.
    sb.append(arrayClassHeader);
//    LOGI("arrayClassHeader: %s", arrayClassHeader.str().c_str());
    sb.append(methodInfoHeader);

    LOGI("write il2cpp.h");
    auto outPath = std::string(outDir).append("/files/Il2Cpp/il2cpp.h");
    std::ofstream outStream(outPath);;
    outStream << sb.str();
    outStream.close();
    LOGI("dump il2cpp.h done!");
}

// Dump class struct for ida by dump.cs.
void il2cpp_dump_class_struct_h(const char *outDir) {
    LOGI("dumping class_struct.h ...");
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    if (il2cpp_image_get_class) {
        // Iterate images.
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            auto classCount = il2cpp_image_get_class_count(image);
            // Iterate classes.
            for (int j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                auto class_full_name = getFullTypeName(klass);
//                LOGI("class name: %s", class_full_name.c_str());
                dump_type_for_class_struct_h(type);
            }
        }
    }

    for (const auto &generic_inst: all_generic_instances) {
        auto generic_class_name = generic_inst.generic_class_name;
        if (allClasses.find(generic_class_name) != allClasses.end()) {
            auto generic_class_struct = allClasses[generic_class_name];
            auto generic_class_struct_string = generic_class_struct.classStruct;
            auto pos = generic_class_struct_string.find(generic_class_name);
            if (pos != std::string::npos) {
                generic_class_struct_string.replace(pos, generic_class_name.length(),
                                                    generic_inst.name);
            }
            ClassStruct generic_inst_struct = {generic_class_struct.parentName,
                                               generic_class_struct_string};
            allClasses[generic_inst.name] = generic_inst_struct;
        }
    }

    // Sort parent class.
    for (const auto &[name, cls]: allClasses) {
        if (!visited.count(name)) {
            if (!dfs(name)) {
                LOGE("Found circular dependency!");
                throw std::invalid_argument("Found circular dependency!");
            }
        }
    }

    LOGI("write class_struct.h");
    auto outPath = std::string(outDir).append("/files/Il2Cpp/class_struct.h");
    std::ofstream outStream(outPath);

    outStream << HeaderConstants::GenericHeader;
    outStream << HeaderConstants::HeaderV29 << "\n";

    // Declaration of classes.
    for (const auto &type_name: all_type_names) {
        outStream << "struct " << type_name << ";\n\n";
    }

    // Definitions of classes.
    auto count = sortedClasses.size();
    for (int i = 0; i < count; ++i) {
        outStream << sortedClasses[i];
    }
    outStream.close();
    LOGI("dump class_struct.h done!");
}

