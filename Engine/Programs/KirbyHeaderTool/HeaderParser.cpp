
#include "HeaderParser.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace
{
    struct FToken
    {
        std::string Text;
        unsigned int Line = 1;
    };

    bool Tokenize(const std::string& Source, std::vector<FToken>& OutTokens, std::string& OutError)
    {
        size_t Index = 0;
        unsigned int Line = 1;
        bool bAtLineStart = true;

        while (Index < Source.size())
        {
            const char Current = Source[Index];
            const char Next = Index + 1 < Source.size() ? Source[Index + 1] : '\0';

            if (Current == '\n')
            {
                ++Line;
                ++Index;
                bAtLineStart = true;
                continue;
            }

            if (Current == '\r' || Current == ' ' || Current == '\t' || Current == '\f' || Current == '\v')
            {
                ++Index;
                continue;
            }

            if (bAtLineStart && Current == '#')
            {
                while (Index < Source.size() && Source[Index] != '\n')
                {
                    ++Index;
                }

                continue;
            }

            bAtLineStart = false;

            if (Current == '/' && Next == '/')
            {
                while (Index < Source.size() && Source[Index] != '\n')
                {
                    ++Index;
                }

                continue;
            }

            if (Current == '/' && Next == '*')
            {
                Index += 2;
                bool bClosed = false;

                while (Index + 1 < Source.size())
                {
                    if (Source[Index] == '\n')
                    {
                        ++Line;
                        bAtLineStart = true;
                    }

                    if (Source[Index] == '*' && Source[Index + 1] == '/')
                    {
                        Index += 2;
                        bClosed = true;
                        break;
                    }

                    ++Index;
                }

                if (!bClosed)
                {
                    OutError = "Unterminated block comment.";
                    return false;
                }

                continue;
            }

            if (Current == '"' || Current == '\'')
            {
                const char Quote = Current;
                ++Index;
                bool bClosed = false;

                while (Index < Source.size())
                {
                    if (Source[Index] == '\\')
                    {
                        if (Index + 1 < Source.size())
                        {
                            if (Source[Index + 1] == '\n')
                            {
                                ++Line;
                            }

                            Index += 2;
                            continue;
                        }
                    }

                    if (Source[Index] == Quote)
                    {
                        ++Index;
                        bClosed = true;
                        break;
                    }

                    if (Source[Index] == '\n')
                    {
                        ++Line;
                    }

                    ++Index;
                }

                if (!bClosed)
                {
                    OutError = "Unterminated string or character literal.";
                    return false;
                }

                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(Current)) || Current == '_')
            {
                const size_t Start = Index;

                while (Index < Source.size())
                {
                    const unsigned char Character = static_cast<unsigned char>(Source[Index]);

                    if (!std::isalnum(Character) && Source[Index] != '_')
                    {
                        break;
                    }

                    ++Index;
                }

                OutTokens.push_back({Source.substr(Start, Index - Start), Line});
                continue;
            }

            OutTokens.push_back({std::string(1, Current), Line});
            ++Index;
        }

        return true;
    }

    bool SkipBalanced(const std::vector<FToken>& Tokens, size_t& Index, const std::string& Open, const std::string& Close)
    {
        if (Index >= Tokens.size() || Tokens[Index].Text != Open)
        {
            return false;
        }

        int Depth = 0;

        for (size_t Cursor = Index; Cursor < Tokens.size(); ++Cursor)
        {
            if (Tokens[Cursor].Text == Open)
            {
                ++Depth;
            }
            else if (Tokens[Cursor].Text == Close)
            {
                --Depth;
            }

            if (Depth == 0)
            {
                Index = Cursor + 1;
                return true;
            }
        }

        return false;
    }

    std::string MakeFileId(const std::string& RelativePath)
    {
        std::string Result = RelativePath;

        for (char& Character : Result)
        {
            const unsigned char Value = static_cast<unsigned char>(Character);

            if (!std::isalnum(Value) && Character != '_')
            {
                Character = '_';
            }
        }

        return Result;
    }
}

bool FHeaderParser::ParseFile(const std::filesystem::path& HeaderFile, const std::filesystem::path& InputRoot, std::vector<FReflectedClassInfo>& OutClasses, std::string& OutError) const
{
    std::ifstream Input(HeaderFile, std::ios::binary);

    if (!Input.is_open())
    {
        OutError = "Failed to open header: " + HeaderFile.string();
        return false;
    }

    std::string Source{std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};

    if (Source.size() >= 3 && static_cast<unsigned char>(Source[0]) == 0xEF && static_cast<unsigned char>(Source[1]) == 0xBB && static_cast<unsigned char>(Source[2]) == 0xBF)
    {
        Source.erase(0, 3);
    }

    std::vector<FToken> Tokens;

    if (!Tokenize(Source, Tokens, OutError))
    {
        OutError = HeaderFile.string() + ": " + OutError;
        return false;
    }

    for (size_t Index = 0; Index < Tokens.size(); ++Index)
    {
        if (Tokens[Index].Text != "UCLASS")
        {
            continue;
        }

        size_t Cursor = Index + 1;

        if (Cursor >= Tokens.size() || Tokens[Cursor].Text != "(" || !SkipBalanced(Tokens, Cursor, "(", ")"))
        {
            OutError = HeaderFile.string() + ":" + std::to_string(Tokens[Index].Line) + ": Invalid UCLASS declaration.";
            return false;
        }

        if (Cursor >= Tokens.size() || Tokens[Cursor].Text != "class" || Cursor + 1 >= Tokens.size())
        {
            OutError = HeaderFile.string() + ":" + std::to_string(Tokens[Index].Line) + ": Expected class after UCLASS.";
            return false;
        }

        FReflectedClassInfo ClassInfo;
        ClassInfo.ClassName = Tokens[Cursor + 1].Text;

        size_t OpenBraceIndex = Cursor + 2;
        bool bFoundSuperClass = false;

        for (; OpenBraceIndex < Tokens.size(); ++OpenBraceIndex)
        {
            if (Tokens[OpenBraceIndex].Text == ":" && OpenBraceIndex + 2 < Tokens.size() && Tokens[OpenBraceIndex + 1].Text == "public")
            {
                ClassInfo.SuperClassName = Tokens[OpenBraceIndex + 2].Text;
                bFoundSuperClass = true;
            }

            if (Tokens[OpenBraceIndex].Text == "{")
            {
                break;
            }

            if (Tokens[OpenBraceIndex].Text == ";")
            {
                break;
            }
        }

        if (!bFoundSuperClass || OpenBraceIndex >= Tokens.size() || Tokens[OpenBraceIndex].Text != "{")
        {
            OutError = HeaderFile.string() + ":" + std::to_string(Tokens[Index].Line) + ": Expected a public base class and class body.";
            return false;
        }

        int BraceDepth = 0;
        bool bFoundGeneratedBody = false;
        size_t ClassEndIndex = Tokens.size();

        for (size_t BodyIndex = OpenBraceIndex; BodyIndex < Tokens.size(); ++BodyIndex)
        {
            if (Tokens[BodyIndex].Text == "{")
            {
                ++BraceDepth;
            }
            else if (Tokens[BodyIndex].Text == "}")
            {
                --BraceDepth;

                if (BraceDepth == 0)
                {
                    ClassEndIndex = BodyIndex;
                    break;
                }
            }

            if (BraceDepth == 1 && Tokens[BodyIndex].Text == "GENERATED_BODY")
            {
                if (bFoundGeneratedBody)
                {
                    OutError = HeaderFile.string() + ":" + std::to_string(Tokens[BodyIndex].Line) + ": Multiple GENERATED_BODY declarations.";
                    return false;
                }

                if (BodyIndex + 1 >= Tokens.size() || Tokens[BodyIndex + 1].Text != "(")
                {
                    OutError = HeaderFile.string() + ":" + std::to_string(Tokens[BodyIndex].Line) + ": Invalid GENERATED_BODY declaration.";
                    return false;
                }

                size_t BodyCursor = BodyIndex + 1;

                if (!SkipBalanced(Tokens, BodyCursor, "(", ")"))
                {
                    OutError = HeaderFile.string() + ":" + std::to_string(Tokens[BodyIndex].Line) + ": Invalid GENERATED_BODY arguments.";
                    return false;
                }

                ClassInfo.GeneratedBodyLine = Tokens[BodyIndex].Line;
                bFoundGeneratedBody = true;
            }
        }

        if (ClassEndIndex >= Tokens.size() || !bFoundGeneratedBody)
        {
            OutError = HeaderFile.string() + ":" + std::to_string(Tokens[Index].Line) + ": Class body or GENERATED_BODY not found.";
            return false;
        }

        std::error_code ErrorCode;
        const std::filesystem::path RelativePath = std::filesystem::relative(HeaderFile, InputRoot, ErrorCode);

        if (ErrorCode)
        {
            OutError = "Failed to make relative header path: " + HeaderFile.string();
            return false;
        }

        ClassInfo.HeaderIncludePath = RelativePath.generic_string();

        std::filesystem::path GeneratedPath = RelativePath;
        GeneratedPath.replace_extension(".generated.h");

        ClassInfo.GeneratedIncludePath = GeneratedPath.generic_string();
        ClassInfo.FileId = MakeFileId(RelativePath.generic_string());

        OutClasses.push_back(ClassInfo);
        Index = ClassEndIndex;
    }

    return true;
}
