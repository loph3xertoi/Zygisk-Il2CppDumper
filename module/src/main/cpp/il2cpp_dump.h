//
// Created by Perfare on 2020/7/4.
//

#ifndef ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H
#define ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H

void il2cpp_api_init(void *handle);

void il2cpp_dump(const char *outDir);

void il2cpp_dump_script_json(const char *outDir);

void il2cpp_dump_il2cpp_h(const char *outDir);

void il2cpp_dump_class_struct_h(const char *outDir);

#endif //ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H
