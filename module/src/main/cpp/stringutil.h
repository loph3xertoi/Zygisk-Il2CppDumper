//
// Created by daw on 4/23/25.
//

#ifndef ZYGISK_IL2CPPDUMPER_STRINGUTIL_H
#define ZYGISK_IL2CPPDUMPER_STRINGUTIL_H

#include <string>
#include <vector>
#include <iostream>
#include <cassert>

std::vector<std::string> StringSplit(std::string str, const std::string delim);

std::string StringJoin(const std::vector<std::string> &strs, const std::string delim);

#endif //ZYGISK_IL2CPPDUMPER_STRINGUTIL_H
