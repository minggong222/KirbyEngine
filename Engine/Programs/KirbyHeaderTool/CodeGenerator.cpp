
#include "CodeGenerator.h"

#include <fstream>
#include <unordered_set>

bool FCodeGenerator::Generate(const std::vector<FReflectedClassInfo>& Classes, const std::filesystem::path& OutputDirectory, std::string& OutError) const
{
    if (Classes.empty())
    {
        OutError = "No UCLASS declarations were found.";
        return false;
    }

    std::error_code ErrorCode;
    std::filesystem::create_directories(OutputDirectory, ErrorCode);

    if (ErrorCode)
    {
        OutError = "Failed to create generated output directory: " + OutputDirectory.string();
        return false;
    }

    std::unordered_set<std::string> ClassNames;

    for (const FReflectedClassInfo& ClassInfo : Classes)
    {
        if (!ClassNames.insert(ClassInfo.ClassName).second)
        {
            OutError = "Duplicate reflected class: " + ClassInfo.ClassName;
            return false;
        }

        const std::filesystem::path GeneratedHeaderPath = OutputDirectory / ClassInfo.GeneratedIncludePath;

        std::filesystem::create_directories(GeneratedHeaderPath.parent_path(), ErrorCode);

        if (ErrorCode)
        {
            OutError = "Failed to create generated header directory: " + GeneratedHeaderPath.parent_path().string();
            return false;
        }

        std::ofstream GeneratedHeader(GeneratedHeaderPath, std::ios::binary);

        if (!GeneratedHeader.is_open())
        {
            OutError = "Failed to create generated header: " + GeneratedHeaderPath.string();
            return false;
        }

        GeneratedHeader << "#pragma once\n\n";
        GeneratedHeader << "#undef CURRENT_FILE_ID\n";
        GeneratedHeader << "#define CURRENT_FILE_ID " << ClassInfo.FileId << "\n\n";
        GeneratedHeader << "#define " << ClassInfo.FileId << "_GENERATED_BODY_LINE_" << ClassInfo.GeneratedBodyLine << " \\\n";
        GeneratedHeader << "    public: \\\n";
        GeneratedHeader << "    static UClass* StaticClass(); \\\n";
        GeneratedHeader << "    UClass* GetClass() const override; \\\n";
        GeneratedHeader << "    static void RegisterProperties(UClass& ClassInfo);\n";
    }

    const std::filesystem::path GeneratedCppPath = OutputDirectory / "KirbyEngine.generated.cpp";
    std::ofstream GeneratedCpp(GeneratedCppPath, std::ios::binary);

    if (!GeneratedCpp.is_open())
    {
        OutError = "Failed to create generated cpp: " + GeneratedCppPath.string();
        return false;
    }

    GeneratedCpp << "#include \"UObject/Class.h\"\n";
    GeneratedCpp << "#include \"UObject/Object.h\"\n\n";

    for (const FReflectedClassInfo& ClassInfo : Classes)
    {
        GeneratedCpp << "#include \"" << ClassInfo.HeaderIncludePath << "\"\n";
    }

    GeneratedCpp << "\n";

    for (const FReflectedClassInfo& ClassInfo : Classes)
    {
        GeneratedCpp << "UClass* " << ClassInfo.ClassName << "::StaticClass()\n";
        GeneratedCpp << "{\n";
        GeneratedCpp << "    static UClass* ClassInfo = new UClass(\"" << ClassInfo.ClassName << "\", " << ClassInfo.SuperClassName << "::StaticClass(), sizeof(" << ClassInfo.ClassName << "), []() -> UObject* { return new " << ClassInfo.ClassName << "(); });\n";
        GeneratedCpp << "    static const bool bRegistered = []() { " << ClassInfo.ClassName << "::RegisterProperties(*ClassInfo); return true; }();\n";
        GeneratedCpp << "    (void)bRegistered;\n";
        GeneratedCpp << "    return ClassInfo;\n";
        GeneratedCpp << "}\n\n";

        GeneratedCpp << "UClass* " << ClassInfo.ClassName << "::GetClass() const\n";
        GeneratedCpp << "{\n";
        GeneratedCpp << "    return " << ClassInfo.ClassName << "::StaticClass();\n";
        GeneratedCpp << "}\n\n";
    }

    return true;
}
