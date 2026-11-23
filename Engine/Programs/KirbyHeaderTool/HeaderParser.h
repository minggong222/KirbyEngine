
#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct FReflectedClassInfo
{
    std::string ClassName;
    std::string SuperClassName;
    std::string HeaderIncludePath;
    std::string GeneratedIncludePath;
    std::string FileId;
    unsigned int GeneratedBodyLine = 0;
};

class FHeaderParser
{
public:
    bool ParseFile(const std::filesystem::path& HeaderFile, const std::filesystem::path& InputRoot, std::vector<FReflectedClassInfo>& OutClasses, std::string& OutError) const;
};
