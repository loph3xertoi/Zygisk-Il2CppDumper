//
// Created by daw on 4/24/25.
//

#ifndef ZYGISK_IL2CPPDUMPER_STRUCTINFO_H
#define ZYGISK_IL2CPPDUMPER_STRUCTINFO_H

#include <iostream>
#include <vector>

typedef struct StructFieldInfo {
    std::string FieldTypeName;
    std::string FieldName;
    bool IsValueType;
    bool IsCustomType;

    bool operator<(const StructFieldInfo &other) const {
        return std::tie(
                FieldTypeName,
                FieldName,
                IsValueType,
                IsCustomType
        ) < std::tie(
                FieldTypeName,
                FieldName,
                IsValueType,
                IsCustomType
        );
    }
} StructFieldInfo;

typedef struct StructVTableMethodInfo {
    std::string MethodName;

    bool operator<(const StructVTableMethodInfo &other) const {
        return std::tie(MethodName) < std::tie(MethodName);
    }
} StructVTableMethodInfo;

typedef struct StructRGCTXInfo {
    Il2CppRGCTXDataType Type;
    std::string TypeName;
    std::string ClassName;
    std::string MethodName;

    bool operator<(const StructRGCTXInfo &other) const {
        return std::tie(
                TypeName,
                ClassName,
                MethodName
        ) < std::tie(
                TypeName,
                ClassName,
                MethodName
        );
    }
} StructRGCTXInfo;

typedef struct StructInfo {
    std::string TypeName;
    bool IsValueType;
    std::string Parent;
    std::vector<StructFieldInfo> Fields;
    std::vector<StructFieldInfo> StaticFields;
    std::vector<StructVTableMethodInfo> VTableMethod;
    std::vector<StructRGCTXInfo> RGCTXs;

    bool operator<(const StructInfo &other) const {
        return std::tie(
                TypeName,
                IsValueType,
                Parent,
                Fields,
                StaticFields,
                VTableMethod,
                RGCTXs
        ) < std::tie(
                other.TypeName,
                other.IsValueType,
                other.Parent,
                other.Fields,
                other.StaticFields,
                other.VTableMethod,
                other.RGCTXs
        );
    }
} StructInfo;

#endif //ZYGISK_IL2CPPDUMPER_STRUCTINFO_H
