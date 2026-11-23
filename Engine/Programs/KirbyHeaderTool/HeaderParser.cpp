
#include "HeaderParser.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <unordered_set>

namespace
{
    enum class ETokenKind
    {
        Identifier,
        Number,
        StringLiteral,
        Symbol
    };

    struct FToken
    {
        std::string Text;
        unsigned int Line = 1;
        ETokenKind Kind = ETokenKind::Symbol;
    };

    bool IsIdentifierStart(char Character)
    {
        const unsigned char Value = static_cast<unsigned char>(Character);
        return std::isalpha(Value) || Character == '_';
    }

    bool IsIdentifierCharacter(char Character)
    {
        const unsigned char Value = static_cast<unsigned char>(Character);
        return std::isalnum(Value) || Character == '_';
    }

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
                const unsigned int TokenLine = Line;
                std::string Value;

                ++Index;
                bool bClosed = false;

                while (Index < Source.size())
                {
                    const char Character = Source[Index];

                    if (Character == '\\' && Index + 1 < Source.size())
                    {
                        const char Escaped = Source[Index + 1];

                        switch (Escaped)
                        {
                        case 'n': Value.push_back('\n'); break;
                        case 'r': Value.push_back('\r'); break;
                        case 't': Value.push_back('\t'); break;
                        case '\\': Value.push_back('\\'); break;
                        case '"': Value.push_back('"'); break;
                        case '\'': Value.push_back('\''); break;
                        default: Value.push_back(Escaped); break;
                        }

                        Index += 2;
                        continue;
                    }

                    if (Character == Quote)
                    {
                        ++Index;
                        bClosed = true;
                        break;
                    }

                    if (Character == '\n')
                    {
                        ++Line;
                    }

                    Value.push_back(Character);
                    ++Index;
                }

                if (!bClosed)
                {
                    OutError = "Unterminated string or character literal.";
                    return false;
                }

                if (Quote == '"')
                {
                    OutTokens.push_back({Value, TokenLine, ETokenKind::StringLiteral});
                }

                continue;
            }

            if (IsIdentifierStart(Current))
            {
                const size_t Start = Index;

                while (Index < Source.size() && IsIdentifierCharacter(Source[Index]))
                {
                    ++Index;
                }

                OutTokens.push_back({Source.substr(Start, Index - Start), Line, ETokenKind::Identifier});
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(Current)))
            {
                const size_t Start = Index;

                while (Index < Source.size())
                {
                    const unsigned char Character = static_cast<unsigned char>(Source[Index]);

                    if (!std::isalnum(Character) && Source[Index] != '.')
                    {
                        break;
                    }

                    ++Index;
                }

                OutTokens.push_back({Source.substr(Start, Index - Start), Line, ETokenKind::Number});
                continue;
            }

            if (Current == ':' && Next == ':')
            {
                OutTokens.push_back({"::", Line, ETokenKind::Symbol});
                Index += 2;
                continue;
            }

            if (Current == '&' && Next == '&')
            {
                OutTokens.push_back({"&&", Line, ETokenKind::Symbol});
                Index += 2;
                continue;
            }

            OutTokens.push_back({std::string(1, Current), Line, ETokenKind::Symbol});
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

    bool ReadMacroArguments(const std::vector<FToken>& Tokens, size_t MacroIndex, size_t& OutAfterIndex, std::vector<FToken>& OutArguments)
    {
        const size_t OpenIndex = MacroIndex + 1;

        if (OpenIndex >= Tokens.size() || Tokens[OpenIndex].Text != "(")
        {
            return false;
        }

        size_t AfterIndex = OpenIndex;

        if (!SkipBalanced(Tokens, AfterIndex, "(", ")"))
        {
            return false;
        }

        OutArguments.assign(Tokens.begin() + OpenIndex + 1, Tokens.begin() + AfterIndex - 1);
        OutAfterIndex = AfterIndex;
        return true;
    }

    std::string JoinTypeTokens(const std::vector<FToken>& Tokens, size_t Start, size_t End)
    {
        std::string Result;

        for (size_t Index = Start; Index < End; ++Index)
        {
            if (!Result.empty())
            {
                Result += " ";
            }

            Result += Tokens[Index].Text;
        }

        return Result;
    }

    std::string JoinTypeTokens(const std::vector<FToken>& Tokens)
    {
        return JoinTypeTokens(Tokens, 0, Tokens.size());
    }

    bool IsSupportedPropertyType(const std::string& TypeName)
    {
        static const std::unordered_set<std::string> SupportedTypes =
        {
            "bool",
            "int32",
            "uint32",
            "float",
            "double",
            "FString",
            "FVector",
            "FRotator",
            "FTransform"
        };

        return SupportedTypes.contains(TypeName);
    }

    bool IsSupportedFunctionType(const std::string& TypeName)
    {
        return TypeName == "void" || IsSupportedPropertyType(TypeName);
    }

    std::string FindStringSpecifier(const std::vector<FToken>& Tokens, const std::string& Name)
    {
        for (size_t Index = 0; Index + 2 < Tokens.size(); ++Index)
        {
            if (Tokens[Index].Text == Name && Tokens[Index + 1].Text == "=" && Tokens[Index + 2].Kind == ETokenKind::StringLiteral)
            {
                return Tokens[Index + 2].Text;
            }
        }

        return {};
    }

    bool ReadStatementEnd(const std::vector<FToken>& Tokens, size_t StartIndex, size_t& OutEndIndex)
    {
        int ParenthesisDepth = 0;
        int BracketDepth = 0;
        int BraceDepth = 0;

        for (size_t Index = StartIndex; Index < Tokens.size(); ++Index)
        {
            const std::string& Token = Tokens[Index].Text;

            if (Token == "(") ++ParenthesisDepth;
            else if (Token == ")") --ParenthesisDepth;
            else if (Token == "[") ++BracketDepth;
            else if (Token == "]") --BracketDepth;
            else if (Token == "{") ++BraceDepth;
            else if (Token == "}") --BraceDepth;

            if (Token == ";" && ParenthesisDepth == 0 && BracketDepth == 0 && BraceDepth == 0)
            {
                OutEndIndex = Index;
                return true;
            }

            if (ParenthesisDepth < 0 || BracketDepth < 0 || BraceDepth < 0)
            {
                return false;
            }
        }

        return false;
    }

    bool ParseProperty(const std::vector<FToken>& Tokens, size_t StartIndex, const std::vector<FToken>& Specifiers, FReflectedPropertyInfo& OutProperty, size_t& OutEndIndex, std::string& OutError)
    {
        if (!ReadStatementEnd(Tokens, StartIndex, OutEndIndex))
        {
            OutError = "UPROPERTY declaration must end with a semicolon.";
            return false;
        }

        size_t DeclarationEnd = OutEndIndex;

        for (size_t Index = StartIndex; Index < OutEndIndex; ++Index)
        {
            if (Tokens[Index].Text == "=")
            {
                DeclarationEnd = Index;
                break;
            }
        }

        size_t NameIndex = DeclarationEnd;

        while (NameIndex > StartIndex)
        {
            --NameIndex;

            if (Tokens[NameIndex].Kind == ETokenKind::Identifier)
            {
                break;
            }
        }

        if (NameIndex <= StartIndex || Tokens[NameIndex].Kind != ETokenKind::Identifier)
        {
            OutError = "Failed to parse UPROPERTY member name.";
            return false;
        }

        OutProperty.PropertyName = Tokens[NameIndex].Text;
        OutProperty.TypeName = JoinTypeTokens(Tokens, StartIndex, NameIndex);

        if (!IsSupportedPropertyType(OutProperty.TypeName))
        {
            OutError = "Unsupported UPROPERTY type: " + OutProperty.TypeName;
            return false;
        }

        OutProperty.DisplayName = FindStringSpecifier(Specifiers, "DisplayName");
        OutProperty.Category = FindStringSpecifier(Specifiers, "Category");

        for (const FToken& Token : Specifiers)
        {
            if (Token.Text == "EditAnywhere")
            {
                OutProperty.bVisible = true;
                OutProperty.bEditable = true;
            }
            else if (Token.Text == "VisibleAnywhere")
            {
                OutProperty.bVisible = true;
                OutProperty.bEditable = false;
            }
        }

        return true;
    }

    bool SplitFunctionParameters(const std::vector<FToken>& Tokens, size_t StartIndex, size_t EndIndex, std::vector<FReflectedFunctionParameterInfo>& OutParameters, std::string& OutError)
    {
        if (StartIndex >= EndIndex)
        {
            return true;
        }

        size_t SegmentStart = StartIndex;
        int ParenthesisDepth = 0;

        for (size_t Index = StartIndex; Index <= EndIndex; ++Index)
        {
            const bool bAtEnd = Index == EndIndex;
            const std::string Token = bAtEnd ? "," : Tokens[Index].Text;

            if (Token == "(") ++ParenthesisDepth;
            else if (Token == ")") --ParenthesisDepth;

            if (Token == "," && ParenthesisDepth == 0)
            {
                size_t SegmentEnd = Index;

                for (size_t Cursor = SegmentStart; Cursor < SegmentEnd; ++Cursor)
                {
                    if (Tokens[Cursor].Text == "=")
                    {
                        SegmentEnd = Cursor;
                        break;
                    }
                }

                if (SegmentEnd == SegmentStart + 1 && Tokens[SegmentStart].Text == "void")
                {
                    SegmentStart = Index + 1;
                    continue;
                }

                size_t NameIndex = SegmentEnd;

                while (NameIndex > SegmentStart)
                {
                    --NameIndex;

                    if (Tokens[NameIndex].Kind == ETokenKind::Identifier)
                    {
                        break;
                    }
                }

                if (NameIndex <= SegmentStart || Tokens[NameIndex].Kind != ETokenKind::Identifier)
                {
                    OutError = "Failed to parse UFUNCTION parameter name.";
                    return false;
                }

                FReflectedFunctionParameterInfo Parameter;
                Parameter.ParameterName = Tokens[NameIndex].Text;
                Parameter.TypeName = JoinTypeTokens(Tokens, SegmentStart, NameIndex);

                if (!IsSupportedFunctionType(Parameter.TypeName) || Parameter.TypeName == "void")
                {
                    OutError = "Unsupported UFUNCTION parameter type: " + Parameter.TypeName;
                    return false;
                }

                OutParameters.push_back(Parameter);
                SegmentStart = Index + 1;
            }
        }

        return true;
    }

    bool ParseFunction(const std::vector<FToken>& Tokens, size_t StartIndex, const std::vector<FToken>& Specifiers, FReflectedFunctionInfo& OutFunction, size_t& OutEndIndex, std::string& OutError)
    {
        if (!ReadStatementEnd(Tokens, StartIndex, OutEndIndex))
        {
            OutError = "UFUNCTION declaration must end with a semicolon.";
            return false;
        }

        size_t OpenParenIndex = OutEndIndex;

        for (size_t Index = StartIndex; Index < OutEndIndex; ++Index)
        {
            if (Tokens[Index].Text == "(")
            {
                OpenParenIndex = Index;
                break;
            }
        }

        if (OpenParenIndex == OutEndIndex || OpenParenIndex == StartIndex)
        {
            OutError = "Failed to parse UFUNCTION declaration.";
            return false;
        }

        if (Tokens[OpenParenIndex - 1].Kind != ETokenKind::Identifier)
        {
            OutError = "Failed to parse UFUNCTION name.";
            return false;
        }

        OutFunction.FunctionName = Tokens[OpenParenIndex - 1].Text;
        OutFunction.ReturnType = JoinTypeTokens(Tokens, StartIndex, OpenParenIndex - 1);
        OutFunction.Category = FindStringSpecifier(Specifiers, "Category");

        if (!IsSupportedFunctionType(OutFunction.ReturnType))
        {
            OutError = "Unsupported UFUNCTION return type: " + OutFunction.ReturnType;
            return false;
        }

        size_t AfterParameters = OpenParenIndex;

        if (!SkipBalanced(Tokens, AfterParameters, "(", ")"))
        {
            OutError = "Invalid UFUNCTION parameter list.";
            return false;
        }

        if (!SplitFunctionParameters(Tokens, OpenParenIndex + 1, AfterParameters - 1, OutFunction.Parameters, OutError))
        {
            return false;
        }

        for (size_t Index = AfterParameters; Index < OutEndIndex; ++Index)
        {
            if (Tokens[Index].Text == "const")
            {
                OutFunction.bIsConst = true;
            }
        }

        return true;
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
        std::vector<FToken> ClassSpecifiers;

        if (!ReadMacroArguments(Tokens, Index, Cursor, ClassSpecifiers))
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
            const std::string& Token = Tokens[BodyIndex].Text;

            if (Token == "{")
            {
                ++BraceDepth;
                continue;
            }

            if (Token == "}")
            {
                --BraceDepth;

                if (BraceDepth == 0)
                {
                    ClassEndIndex = BodyIndex;
                    break;
                }

                continue;
            }

            if (BraceDepth != 1)
            {
                continue;
            }

            if (Token == "GENERATED_BODY")
            {
                size_t AfterBody = BodyIndex + 1;
                std::vector<FToken> BodyArguments;

                if (!ReadMacroArguments(Tokens, BodyIndex, AfterBody, BodyArguments) || !BodyArguments.empty())
                {
                    OutError = HeaderFile.string() + ":" + std::to_string(Tokens[BodyIndex].Line) + ": Invalid GENERATED_BODY declaration.";
                    return false;
                }

                if (bFoundGeneratedBody)
                {
                    OutError = HeaderFile.string() + ":" + std::to_string(Tokens[BodyIndex].Line) + ": Multiple GENERATED_BODY declarations.";
                    return false;
                }

                ClassInfo.GeneratedBodyLine = Tokens[BodyIndex].Line;
                bFoundGeneratedBody = true;
                BodyIndex = AfterBody - 1;
                continue;
            }

            if (Token == "UPROPERTY")
            {
                size_t AfterPropertyMacro = BodyIndex + 1;
                std::vector<FToken> PropertySpecifiers;

                if (!ReadMacroArguments(Tokens, BodyIndex, AfterPropertyMacro, PropertySpecifiers))
                {
                    OutError = HeaderFile.string() + ":" + std::to_string(Tokens[BodyIndex].Line) + ": Invalid UPROPERTY declaration.";
                    return false;
                }

                FReflectedPropertyInfo Property;
                size_t StatementEnd = 0;

                if (!ParseProperty(Tokens, AfterPropertyMacro, PropertySpecifiers, Property, StatementEnd, OutError))
                {
                    OutError = HeaderFile.string() + ":" + std::to_string(Tokens[BodyIndex].Line) + ": " + OutError;
                    return false;
                }

                ClassInfo.Properties.push_back(Property);
                BodyIndex = StatementEnd;
                continue;
            }

            if (Token == "UFUNCTION")
            {
                size_t AfterFunctionMacro = BodyIndex + 1;
                std::vector<FToken> FunctionSpecifiers;

                if (!ReadMacroArguments(Tokens, BodyIndex, AfterFunctionMacro, FunctionSpecifiers))
                {
                    OutError = HeaderFile.string() + ":" + std::to_string(Tokens[BodyIndex].Line) + ": Invalid UFUNCTION declaration.";
                    return false;
                }

                FReflectedFunctionInfo Function;
                size_t StatementEnd = 0;

                if (!ParseFunction(Tokens, AfterFunctionMacro, FunctionSpecifiers, Function, StatementEnd, OutError))
                {
                    OutError = HeaderFile.string() + ":" + std::to_string(Tokens[BodyIndex].Line) + ": " + OutError;
                    return false;
                }

                ClassInfo.Functions.push_back(Function);
                BodyIndex = StatementEnd;
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

        OutClasses.push_back(std::move(ClassInfo));
        Index = ClassEndIndex;
    }

    return true;
}
