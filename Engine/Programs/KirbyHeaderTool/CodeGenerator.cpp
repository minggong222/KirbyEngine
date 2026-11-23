
#include "CodeGenerator.h"

#include <fstream>
#include <map>
#include <set>
#include <unordered_set>

namespace
{
    std::string EscapeCppString(const std::string& Value)
    {
        std::string Result;

        for (char Character : Value)
        {
            switch (Character)
            {
            case '\\': Result += "\\\\"; break;
            case '"': Result += "\\\""; break;
            case '\n': Result += "\\n"; break;
            case '\r': Result += "\\r"; break;
            case '\t': Result += "\\t"; break;
            default: Result += Character; break;
            }
        }

        return Result;
    }

    std::string MakeFunctionParamsName(const FReflectedClassInfo& ClassInfo, const FReflectedFunctionInfo& FunctionInfo)
    {
        return "FKirbyParams_" + ClassInfo.ClassName + "_" + FunctionInfo.FunctionName;
    }

    std::string MakeFunctionThunkName(const FReflectedFunctionInfo& FunctionInfo)
    {
        return "Exec_" + FunctionInfo.FunctionName;
    }

    std::string MakePropertyFlags(const FReflectedPropertyInfo& PropertyInfo)
    {
        if (PropertyInfo.bVisible && PropertyInfo.bEditable)
        {
            return "Property_Visible | Property_Editable";
        }

        if (PropertyInfo.bVisible)
        {
            return "Property_Visible";
        }

        return "Property_None";
    }

    void GenerateFunctionParameterStruct(std::ofstream& Output, const FReflectedClassInfo& ClassInfo, const FReflectedFunctionInfo& FunctionInfo)
    {
        Output << "struct " << MakeFunctionParamsName(ClassInfo, FunctionInfo) << "\n";
        Output << "{\n";

        if (FunctionInfo.ReturnType != "void")
        {
            Output << "    " << FunctionInfo.ReturnType << " ReturnValue{};\n";
        }

        for (const FReflectedFunctionParameterInfo& Parameter : FunctionInfo.Parameters)
        {
            Output << "    " << Parameter.TypeName << " " << Parameter.ParameterName << "{};\n";
        }

        Output << "};\n\n";
    }

    void GenerateClassBodyMacro(std::ofstream& Output, const FReflectedClassInfo& ClassInfo)
    {
        Output << "#define " << ClassInfo.FileId << "_GENERATED_BODY_LINE_" << ClassInfo.GeneratedBodyLine << " \\\n";
        Output << "    public: \\\n";
        Output << "    static UClass* StaticClass(); \\\n";
        Output << "    UClass* GetClass() const override; \\\n";
        Output << "    static void RegisterProperties(UClass& ClassInfo); \\\n";
        Output << "    static void RegisterFunctions(UClass& ClassInfo);";

        for (const FReflectedFunctionInfo& Function : ClassInfo.Functions)
        {
            Output << " \\\n";
            Output << "    static void " << MakeFunctionThunkName(Function) << "(UObject* Object, void* Parameters);";
        }

        Output << "\n\n";
    }

    void GenerateFunctionThunk(std::ofstream& Output, const FReflectedClassInfo& ClassInfo, const FReflectedFunctionInfo& FunctionInfo)
    {
        const std::string ParamsName = MakeFunctionParamsName(ClassInfo, FunctionInfo);
        const std::string ThunkName = MakeFunctionThunkName(FunctionInfo);

        Output << "void " << ClassInfo.ClassName << "::" << ThunkName << "(UObject* Object, void* Parameters)\n";
        Output << "{\n";
        Output << "    if (Object == nullptr || Parameters == nullptr)\n";
        Output << "    {\n";
        Output << "        return;\n";
        Output << "    }\n\n";
        Output << "    " << ClassInfo.ClassName << "* TypedObject = static_cast<" << ClassInfo.ClassName << "*>(Object);\n";
        Output << "    " << ParamsName << "* TypedParameters = static_cast<" << ParamsName << "*>(Parameters);\n\n";

        if (FunctionInfo.ReturnType == "void")
        {
            Output << "    TypedObject->" << FunctionInfo.FunctionName << "(";

            for (size_t Index = 0; Index < FunctionInfo.Parameters.size(); ++Index)
            {
                if (Index > 0) Output << ", ";
                Output << "TypedParameters->" << FunctionInfo.Parameters[Index].ParameterName;
            }

            Output << ");\n";
        }
        else
        {
            Output << "    TypedParameters->ReturnValue = TypedObject->" << FunctionInfo.FunctionName << "(";

            for (size_t Index = 0; Index < FunctionInfo.Parameters.size(); ++Index)
            {
                if (Index > 0) Output << ", ";
                Output << "TypedParameters->" << FunctionInfo.Parameters[Index].ParameterName;
            }

            Output << ");\n";
        }

        Output << "}\n\n";
    }
}

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
    std::map<std::string, std::vector<const FReflectedClassInfo*>> HeaderGroups;

    for (const FReflectedClassInfo& ClassInfo : Classes)
    {
        if (!ClassNames.insert(ClassInfo.ClassName).second)
        {
            OutError = "Duplicate reflected class: " + ClassInfo.ClassName;
            return false;
        }

        std::unordered_set<std::string> FunctionNames;

        for (const FReflectedFunctionInfo& Function : ClassInfo.Functions)
        {
            if (!FunctionNames.insert(Function.FunctionName).second)
            {
                OutError = "Overloaded UFUNCTION is not supported: " + ClassInfo.ClassName + "::" + Function.FunctionName;
                return false;
            }
        }

        HeaderGroups[ClassInfo.GeneratedIncludePath].push_back(&ClassInfo);
    }

    for (const auto& [RelativePath, GroupClasses] : HeaderGroups)
    {
        const std::filesystem::path GeneratedHeaderPath = OutputDirectory / RelativePath;

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
        GeneratedHeader << "#define CURRENT_FILE_ID " << GroupClasses.front()->FileId << "\n\n";

        for (const FReflectedClassInfo* ClassInfo : GroupClasses)
        {
            for (const FReflectedFunctionInfo& Function : ClassInfo->Functions)
            {
                GenerateFunctionParameterStruct(GeneratedHeader, *ClassInfo, Function);
            }
        }

        for (const FReflectedClassInfo* ClassInfo : GroupClasses)
        {
            GenerateClassBodyMacro(GeneratedHeader, *ClassInfo);
        }
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

    std::set<std::string> IncludedHeaders;

    for (const FReflectedClassInfo& ClassInfo : Classes)
    {
        if (IncludedHeaders.insert(ClassInfo.HeaderIncludePath).second)
        {
            GeneratedCpp << "#include \"" << ClassInfo.HeaderIncludePath << "\"\n";
        }
    }

    GeneratedCpp << "\n";

    for (const FReflectedClassInfo& ClassInfo : Classes)
    {
        for (const FReflectedFunctionInfo& Function : ClassInfo.Functions)
        {
            GenerateFunctionThunk(GeneratedCpp, ClassInfo, Function);
        }
    }

    for (const FReflectedClassInfo& ClassInfo : Classes)
    {
        GeneratedCpp << "void " << ClassInfo.ClassName << "::RegisterProperties(UClass& ClassInfo)\n";
        GeneratedCpp << "{\n";

        for (const FReflectedPropertyInfo& Property : ClassInfo.Properties)
        {
            const std::string DisplayName = Property.DisplayName.empty() ? Property.PropertyName : Property.DisplayName;
            const std::string Category = Property.Category.empty() ? "Default" : Property.Category;

            GeneratedCpp << "    ClassInfo.AddProperty(MakeProperty(\"" << EscapeCppString(Property.PropertyName) << "\", &" << ClassInfo.ClassName << "::" << Property.PropertyName << ", " << MakePropertyFlags(Property) << ", \"" << EscapeCppString(DisplayName) << "\", \"" << EscapeCppString(Category) << "\"));\n";
        }

        GeneratedCpp << "}\n\n";

        GeneratedCpp << "void " << ClassInfo.ClassName << "::RegisterFunctions(UClass& ClassInfo)\n";
        GeneratedCpp << "{\n";

        for (const FReflectedFunctionInfo& Function : ClassInfo.Functions)
        {
            const std::string Category = Function.Category.empty() ? "Default" : Function.Category;

            GeneratedCpp << "    ClassInfo.AddFunction(new UFunction(\"" << EscapeCppString(Function.FunctionName) << "\", \"" << EscapeCppString(Function.ReturnType) << "\", \"" << EscapeCppString(Category) << "\", &" << ClassInfo.ClassName << "::" << MakeFunctionThunkName(Function) << ", {";

            for (size_t Index = 0; Index < Function.Parameters.size(); ++Index)
            {
                if (Index > 0) GeneratedCpp << ", ";

                const FReflectedFunctionParameterInfo& Parameter = Function.Parameters[Index];
                GeneratedCpp << "{\"" << EscapeCppString(Parameter.ParameterName) << "\", \"" << EscapeCppString(Parameter.TypeName) << "\"}";
            }

            GeneratedCpp << "}));\n";
        }

        GeneratedCpp << "}\n\n";

        GeneratedCpp << "UClass* " << ClassInfo.ClassName << "::StaticClass()\n";
        GeneratedCpp << "{\n";
        GeneratedCpp << "    static UClass* ClassInfo = new UClass(\"" << ClassInfo.ClassName << "\", " << ClassInfo.SuperClassName << "::StaticClass(), sizeof(" << ClassInfo.ClassName << "), []() -> UObject* { return new " << ClassInfo.ClassName << "(); });\n";
        GeneratedCpp << "    static const bool bRegistered = []() { " << ClassInfo.ClassName << "::RegisterProperties(*ClassInfo); " << ClassInfo.ClassName << "::RegisterFunctions(*ClassInfo); return true; }();\n";
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
