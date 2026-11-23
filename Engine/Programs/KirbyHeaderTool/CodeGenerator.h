
#pragma once

#include "HeaderParser.h"

#include <filesystem>
#include <string>
#include <vector>

class FCodeGenerator
{
public:
    bool Generate(const std::vector<FReflectedClassInfo>& Classes, const std::filesystem::path& OutputDirectory, std::string& OutError) const;
};
