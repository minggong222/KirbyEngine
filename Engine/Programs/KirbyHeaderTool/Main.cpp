
#include "HeaderParser.h"
#include "CodeGenerator.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <exception>

int main(int argc, char* argv[])
{
    std::filesystem::path InputDirectory;
    std::filesystem::path OutputDirectory;

    for (int Index = 1; Index < argc; ++Index)
    {
        const std::string Argument = argv[Index];

        if (Argument == "--input-dir" && Index + 1 < argc)
        {
            InputDirectory = argv[++Index];
        }
        else if (Argument == "--output-dir" && Index + 1 < argc)
        {
            OutputDirectory = argv[++Index];
        }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << Argument << '\n';
            return 2;
        }
    }

    if (InputDirectory.empty() || OutputDirectory.empty())
    {
        std::cerr << "Usage: KirbyHeaderTool --input-dir <RuntimeDirectory> --output-dir <GeneratedDirectory>\n";
        return 2;
    }

    try
    {
        InputDirectory = std::filesystem::absolute(InputDirectory).lexically_normal();
        OutputDirectory = std::filesystem::absolute(OutputDirectory).lexically_normal();

        if (!std::filesystem::is_directory(InputDirectory))
        {
            std::cerr << "Input directory does not exist: " << InputDirectory.string() << '\n';
            return 1;
        }

        FHeaderParser Parser;
        std::vector<FReflectedClassInfo> Classes;
        std::string Error;

        for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(InputDirectory))
        {
            if (!Entry.is_regular_file() || Entry.path().extension() != ".h")
            {
                continue;
            }

            if (!Parser.ParseFile(Entry.path(), InputDirectory, Classes, Error))
            {
                std::cerr << "[KirbyHeaderTool] " << Error << '\n';
                return 1;
            }
        }

        FCodeGenerator Generator;

        if (!Generator.Generate(Classes, OutputDirectory, Error))
        {
            std::cerr << "[KirbyHeaderTool] " << Error << '\n';
            return 1;
        }

        std::cout << "[KirbyHeaderTool] Generated " << Classes.size() << " reflected class(es).\n";
        std::cout << "[KirbyHeaderTool] Output: " << OutputDirectory.string() << '\n';
    }
    catch (const std::filesystem::filesystem_error& Exception)
    {
        std::cerr << "[KirbyHeaderTool] Filesystem error: " << Exception.what() << '\n';
        return 1;
    }
    catch (const std::exception& Exception)
    {
        std::cerr << "[KirbyHeaderTool] Error: " << Exception.what() << '\n';
        return 1;
    }

    return 0;
}
