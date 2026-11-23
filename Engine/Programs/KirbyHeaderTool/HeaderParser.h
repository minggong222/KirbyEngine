
#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct FReflectedPropertyInfo
{
    std::string TypeName;
    std::string PropertyName;
    std::string DisplayName;
    std::string Category;
    bool bVisible = false;
    bool bEditable = false;
};

struct FReflectedFunctionParameterInfo
{
    std::string TypeName;
    std::string ParameterName;
};

struct FReflectedFunctionInfo
{
    std::string ReturnType;
    std::string FunctionName;
    std::string Category;
    std::vector<FReflectedFunctionParameterInfo> Parameters;
    bool bIsConst = false;
};

struct FReflectedClassInfo
{
    std::string ClassName;
    std::string SuperClassName;
    std::string HeaderIncludePath;
    std::string GeneratedIncludePath;
    std::string FileId;
    unsigned int GeneratedBodyLine = 0;

    std::vector<FReflectedPropertyInfo> Properties;
    std::vector<FReflectedFunctionInfo> Functions;
};

class FHeaderParser
{
public:
    bool ParseFile(const std::filesystem::path& HeaderFile, const std::filesystem::path& InputRoot, std::vector<FReflectedClassInfo>& OutClasses, std::string& OutError) const;
};
