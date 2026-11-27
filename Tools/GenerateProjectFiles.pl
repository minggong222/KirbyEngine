#!/usr/bin/perl
# Generates the Visual Studio project files from the source folders.
#
#   KirbyRuntime  (Engine/Source/Runtime/KirbyRuntime.vcxproj)  static library
#       Everything under Engine/Source/Runtime + generated reflection code.
#       Only sees its own headers: it cannot include Editor or ImGui code.
#
#   KirbyEngine   (KirbyEngine.vcxproj)  editor executable
#       Engine/Source/Launch + Engine/Source/Editor + ImGui, links KirbyRuntime.
#
# Run from anywhere after adding, moving or deleting source files:
#   perl Tools/GenerateProjectFiles.pl
use strict;
use warnings;
use File::Find;
use File::Basename qw(dirname);
use File::Spec;
use Cwd qw(abs_path);
use Digest::MD5 qw(md5_hex);

my $Root = abs_path(File::Spec->catdir(dirname(abs_path($0)), '..'));
my $NL = "\r\n";

my $RuntimeGuid = '{3F6A2C1E-8B4D-4E7A-9C2F-5D1B7E9A4C30}';
my $EditorGuid  = '{de55a139-ef4d-4229-81ac-bb2fa7b24694}';
my $HeaderToolGuid = '{79B8E0E2-6B8E-4A0C-9D0E-74D3C7C34A0B}';

my @Configs = (['Debug', 'Win32'], ['Release', 'Win32'], ['Debug', 'x64'], ['Release', 'x64']);

# ImGui files the editor compiles (the rest of the ImGui folder is upstream examples/docs)
my @ImGuiSources = qw(imgui.cpp imgui_demo.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp imgui_impl_dx11.cpp imgui_impl_win32.cpp);
my @ImGuiHeaders = qw(imconfig.h imgui.h imgui_internal.h imgui_impl_dx11.h imgui_impl_win32.h imstb_rectpack.h imstb_textedit.h imstb_truetype.h);

sub ListFiles
{
    my ($Dir, $Pattern) = @_;
    my @Files;
    find({ no_chdir => 1, wanted => sub { push @Files, $_ if -f $_ && $_ =~ $Pattern; } }, $Dir);
    return sort @Files;
}

# Root-relative path -> path relative to $Base, with backslashes
sub RelPath
{
    my ($Path, $Base) = @_;
    my $Rel = File::Spec->abs2rel($Path, $Base);
    $Rel =~ s{/}{\\}g;
    return $Rel;
}

sub WriteFile
{
    my ($Path, $Text) = @_;
    $Text =~ s/\r?\n/$NL/g;
    open my $Out, '>:raw', $Path or die "Cannot write $Path: $!";
    print $Out "\xEF\xBB\xBF" if $Path =~ /\.filters$/;
    print $Out $Text;
    close $Out;
    print "Wrote $Path\n";
}

sub ConfigurationItems
{
    my $Text = qq{  <ItemGroup Label="ProjectConfigurations">\n};
    for my $C (@Configs)
    {
        $Text .= qq{    <ProjectConfiguration Include="$C->[0]|$C->[1]">\n      <Configuration>$C->[0]</Configuration>\n      <Platform>$C->[1]</Platform>\n    </ProjectConfiguration>\n};
    }
    return $Text . qq{  </ItemGroup>\n};
}

sub ConfigurationProperties
{
    my ($Type) = @_;
    my $Text = '';
    for my $C (@Configs)
    {
        my $Debug = $C->[0] eq 'Debug';
        $Text .= qq{  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='$C->[0]|$C->[1]'" Label="Configuration">\n};
        $Text .= qq{    <ConfigurationType>$Type</ConfigurationType>\n};
        $Text .= qq{    <UseDebugLibraries>} . ($Debug ? 'true' : 'false') . qq{</UseDebugLibraries>\n};
        $Text .= qq{    <PlatformToolset>v145</PlatformToolset>\n};
        $Text .= qq{    <WholeProgramOptimization>true</WholeProgramOptimization>\n} unless $Debug;
        $Text .= qq{    <CharacterSet>Unicode</CharacterSet>\n  </PropertyGroup>\n};
    }
    return $Text;
}

sub PropertySheets
{
    my $Text = qq{  <ImportGroup Label="ExtensionSettings">\n  </ImportGroup>\n  <ImportGroup Label="Shared">\n  </ImportGroup>\n};
    for my $C (@Configs)
    {
        $Text .= qq{  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='$C->[0]|$C->[1]'">\n};
        $Text .= qq{    <Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />\n};
        $Text .= qq{  </ImportGroup>\n};
    }
    return $Text . qq{  <PropertyGroup Label="UserMacros" />\n};
}

# Per-configuration compiler/linker settings shared by both projects
sub CompileSettings
{
    my ($IncludeDirs, $WithLink) = @_;
    my $Text = '';
    for my $C (@Configs)
    {
        my $Debug = $C->[0] eq 'Debug';
        my $Defines = ($C->[1] eq 'Win32' ? 'WIN32;' : '') . ($Debug ? '_DEBUG' : 'NDEBUG') . ';_CONSOLE;%(PreprocessorDefinitions)';
        $Text .= qq{  <ItemDefinitionGroup Condition="'\$(Configuration)|\$(Platform)'=='$C->[0]|$C->[1]'">\n    <ClCompile>\n};
        $Text .= qq{      <WarningLevel>Level3</WarningLevel>\n};
        $Text .= qq{      <FunctionLevelLinking>true</FunctionLevelLinking>\n      <IntrinsicFunctions>true</IntrinsicFunctions>\n} unless $Debug;
        $Text .= qq{      <SDLCheck>true</SDLCheck>\n      <PreprocessorDefinitions>$Defines</PreprocessorDefinitions>\n};
        $Text .= qq{      <ConformanceMode>true</ConformanceMode>\n      <LanguageStandard>stdcpp20</LanguageStandard>\n};
        $Text .= qq{      <AdditionalIncludeDirectories>$IncludeDirs;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\n    </ClCompile>\n};
        if ($WithLink)
        {
            $Text .= qq{    <Link>\n      <SubSystem>Windows</SubSystem>\n      <GenerateDebugInformation>true</GenerateDebugInformation>\n    </Link>\n};
            $Text .= qq{    <Manifest>\n      <EnableSegmentHeap>true</EnableSegmentHeap>\n    </Manifest>\n};
        }
        $Text .= qq{  </ItemDefinitionGroup>\n};
    }
    return $Text;
}

sub Escape { my $S = shift; $S =~ s/&/&amp;/g; return $S; }

# items: [kind, path, extra-metadata-xml]
sub ItemGroupXml
{
    my (@Items) = @_;
    my $Text = qq{  <ItemGroup>\n};
    for my $I (@Items)
    {
        my ($Kind, $Path, $Meta) = @$I;
        if ($Meta)
        {
            $Text .= qq{    <$Kind Include="} . Escape($Path) . qq{">\n$Meta    </$Kind>\n};
        }
        else
        {
            $Text .= qq{    <$Kind Include="} . Escape($Path) . qq{" />\n};
        }
    }
    return $Text . qq{  </ItemGroup>\n};
}

sub FiltersXml
{
    my ($FilterPrefix, @Items) = @_;
    my %Filters;
    my $Body = '';
    for my $I (@Items)
    {
        my ($Kind, $Path) = @$I;
        next if $Path =~ /^\$\(/;          # generated files have no folder
        my $Dir = $Path =~ /\\/ ? ($Path =~ s/\\[^\\]+$//r) : '';
        $Dir =~ s/^(\.\.\\)+//;
        my $Filter = join '\\', grep { length } ($FilterPrefix, $Dir);
        $Filter = 'Source' if $Filter eq '';
        my @Parts = split /\\/, $Filter;
        $Filters{join '\\', @Parts[0..$_]} = 1 for 0..$#Parts;
        $Body .= qq{    <$Kind Include="} . Escape($Path) . qq{">\n      <Filter>$Filter</Filter>\n    </$Kind>\n};
    }
    my $Text = qq{<?xml version="1.0" encoding="utf-8"?>\n<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n  <ItemGroup>\n};
    for my $F (sort keys %Filters)
    {
        my $H = uc md5_hex("KirbyFilter:$F");
        my $Guid = sprintf '{%s-%s-%s-%s-%s}', substr($H,0,8), substr($H,8,4), substr($H,12,4), substr($H,16,4), substr($H,20,12);
        $Text .= qq{    <Filter Include="$F">\n      <UniqueIdentifier>$Guid</UniqueIdentifier>\n    </Filter>\n};
    }
    return $Text . qq{  </ItemGroup>\n  <ItemGroup>\n$Body  </ItemGroup>\n</Project>\n};
}

# ------------------------------------------------------------------
# KirbyRuntime.vcxproj
# ------------------------------------------------------------------
my $RuntimeDir = "$Root/Engine/Source/Runtime";
my @RuntimeItems;

for my $File (ListFiles($RuntimeDir, qr/\.(cpp|h|inl)$/))
{
    my $Rel = RelPath($File, $RuntimeDir);
    if ($Rel =~ /\.cpp$/)
    {
        my $Meta = $Rel eq 'Core\\Precompiled.cpp'
            ? qq{      <PrecompiledHeader>Create</PrecompiledHeader>\n      <ForcedIncludeFiles />\n      <MultiProcessorCompilation>false</MultiProcessorCompilation>\n}
            : '';
        push @RuntimeItems, ['ClCompile', $Rel, $Meta];
    }
    else
    {
        push @RuntimeItems, ['ClInclude', $Rel];
    }
}

my $RuntimeProject = qq{<?xml version="1.0" encoding="utf-8"?>
<!-- Generated by Tools/GenerateProjectFiles.pl. Re-run it instead of editing by hand. -->
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003" DefaultTargets="Build">
} . ConfigurationItems() . qq{  <PropertyGroup Label="Globals">
    <VCProjectVersion>18.0</VCProjectVersion>
    <Keyword>Win32Proj</Keyword>
    <ProjectGuid>$RuntimeGuid</ProjectGuid>
    <RootNamespace>KirbyRuntime</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
    <ProjectName>KirbyRuntime</ProjectName>
  </PropertyGroup>
  <PropertyGroup>
    <KirbyRoot>\$([System.IO.Path]::GetFullPath('\$(MSBuildThisFileDirectory)..\\..\\..\\'))</KirbyRoot>
    <KirbyGeneratedDir>\$(KirbyRoot)Build\\Intermediate\\KirbyRuntime\\\$(Platform)\\\$(Configuration)\\Generated\\</KirbyGeneratedDir>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <PrecompiledHeader>Use</PrecompiledHeader>
      <PrecompiledHeaderFile>Core/Precompiled.h</PrecompiledHeaderFile>
      <PrecompiledHeaderOutputFile>\$(IntDir)KirbyRuntime.pch</PrecompiledHeaderOutputFile>
      <ForcedIncludeFiles>Core/Precompiled.h;%(ForcedIncludeFiles)</ForcedIncludeFiles>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
    </ClCompile>
  </ItemDefinitionGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
} . ConfigurationProperties('StaticLibrary') . qq{  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <PropertyGroup>
    <OutDir>\$(KirbyRoot)Build\\Lib\\\$(Platform)\\\$(Configuration)\\</OutDir>
    <IntDir>\$(KirbyRoot)Build\\Intermediate\\KirbyRuntime\\\$(Platform)\\\$(Configuration)\\</IntDir>
  </PropertyGroup>
} . PropertySheets()
  . CompileSettings('$(KirbyGeneratedDir);$(MSBuildThisFileDirectory)', 0)
  . ItemGroupXml(@RuntimeItems)
  . ItemGroupXml(['ClCompile', '$(KirbyGeneratedDir)KirbyEngine.generated.cpp'])
  . qq{  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets">
  </ImportGroup>
  <!-- Reflection code generation (KirbyHeaderTool, the UnrealHeaderTool counterpart) -->
  <ItemGroup>
    <KirbyReflectionInput Include="\$(MSBuildThisFileDirectory)**\\*.h" />
    <KirbyReflectionInput Include="\$(KirbyRoot)Engine\\Programs\\KirbyHeaderTool\\*.cpp" />
    <KirbyReflectionInput Include="\$(KirbyRoot)Engine\\Programs\\KirbyHeaderTool\\*.h" />
    <KirbyReflectionInput Include="\$(KirbyRoot)Engine\\Programs\\KirbyHeaderTool\\KirbyHeaderTool.vcxproj" />
  </ItemGroup>
  <Target Name="GenerateKirbyReflection" BeforeTargets="ClCompile" Inputs="@(KirbyReflectionInput)" Outputs="\$(KirbyGeneratedDir)KirbyReflection.stamp;\$(KirbyGeneratedDir)KirbyEngine.generated.cpp">
    <Message Importance="High" Text="[KirbyHeaderTool] Generating reflection code..." />
    <MSBuild Projects="\$(KirbyRoot)Engine\\Programs\\KirbyHeaderTool\\KirbyHeaderTool.vcxproj" Targets="Build" Properties="Configuration=\$(Configuration);Platform=\$(Platform)" />
    <MakeDir Directories="\$(KirbyGeneratedDir)" />
    <Exec Command="&quot;\$(KirbyRoot)Build\\Bin\\Tools\\\$(Platform)\\\$(Configuration)\\KirbyHeaderTool.exe&quot; --input-dir &quot;\$(MSBuildThisFileDirectory.TrimEnd('\\'))&quot; --output-dir &quot;\$(KirbyGeneratedDir.TrimEnd('\\'))&quot;" />
    <Touch Files="\$(KirbyGeneratedDir)KirbyReflection.stamp" AlwaysCreate="true" />
  </Target>
  <Target Name="CleanKirbyReflection" AfterTargets="Clean">
    <RemoveDir Directories="\$(KirbyGeneratedDir)" />
  </Target>
</Project>
};

WriteFile("$RuntimeDir/KirbyRuntime.vcxproj", $RuntimeProject);
WriteFile("$RuntimeDir/KirbyRuntime.vcxproj.filters", FiltersXml('', @RuntimeItems));

# ------------------------------------------------------------------
# KirbyEngine.vcxproj (editor executable)
# ------------------------------------------------------------------
my @EditorItems;

for my $Module ('Launch', 'Editor')
{
    for my $File (ListFiles("$Root/Engine/Source/$Module", qr/\.(cpp|h|inl)$/))
    {
        my $Rel = RelPath($File, $Root);
        if ($Rel =~ /\.cpp$/)
        {
            my $Meta = $Rel eq 'Engine\\Source\\Editor\\EditorPrecompiled.cpp'
                ? qq{      <PrecompiledHeader>Create</PrecompiledHeader>\n      <ForcedIncludeFiles />\n      <MultiProcessorCompilation>false</MultiProcessorCompilation>\n}
                : '';
            push @EditorItems, ['ClCompile', $Rel, $Meta];
        }
        else
        {
            push @EditorItems, ['ClInclude', $Rel];
        }
    }
}

for my $Source (@ImGuiSources)
{
    push @EditorItems, ['ClCompile', "Engine\\Source\\ThirdParty\\ImGui\\$Source", qq{      <PrecompiledHeader>NotUsing</PrecompiledHeader>\n      <ForcedIncludeFiles />\n}];
}
push @EditorItems, ['ClInclude', "Engine\\Source\\ThirdParty\\ImGui\\$_"] for @ImGuiHeaders;

for my $Shader (ListFiles("$Root/Engine/Resources/Shaders", qr/\.hlsl$/))
{
    push @EditorItems, ['FxCompile', RelPath($Shader, $Root), qq{      <ExcludedFromBuild>true</ExcludedFromBuild>\n}];
}

my $EditorProject = qq{<?xml version="1.0" encoding="utf-8"?>
<!-- Generated by Tools/GenerateProjectFiles.pl. Re-run it instead of editing by hand. -->
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003" DefaultTargets="Build">
} . ConfigurationItems() . qq{  <PropertyGroup Label="Globals">
    <VCProjectVersion>18.0</VCProjectVersion>
    <Keyword>Win32Proj</Keyword>
    <ProjectGuid>$EditorGuid</ProjectGuid>
    <RootNamespace>KirbyEngine</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
    <ProjectName>KirbyEngine</ProjectName>
  </PropertyGroup>
  <PropertyGroup>
    <KirbyRoot>\$(MSBuildThisFileDirectory)</KirbyRoot>
    <KirbyGeneratedDir>\$(KirbyRoot)Build\\Intermediate\\KirbyRuntime\\\$(Platform)\\\$(Configuration)\\Generated\\</KirbyGeneratedDir>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <PrecompiledHeader>Use</PrecompiledHeader>
      <PrecompiledHeaderFile>Core/Precompiled.h</PrecompiledHeaderFile>
      <PrecompiledHeaderOutputFile>\$(IntDir)KirbyEngine.pch</PrecompiledHeaderOutputFile>
      <ForcedIncludeFiles>Core/Precompiled.h;%(ForcedIncludeFiles)</ForcedIncludeFiles>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
    </ClCompile>
  </ItemDefinitionGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
} . ConfigurationProperties('Application') . qq{  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <PropertyGroup>
    <OutDir>\$(KirbyRoot)Build\\Bin\\\$(Platform)\\\$(Configuration)\\</OutDir>
    <IntDir>\$(KirbyRoot)Build\\Intermediate\\KirbyEngine\\\$(Platform)\\\$(Configuration)\\</IntDir>
  </PropertyGroup>
} . PropertySheets()
  . CompileSettings('$(KirbyGeneratedDir);$(KirbyRoot)Engine\\Source\\Launch;$(KirbyRoot)Engine\\Source\\Editor;$(KirbyRoot)Engine\\Source\\Runtime;$(KirbyRoot)Engine\\Source\\ThirdParty;$(KirbyRoot)Engine\\Source\\ThirdParty\\ImGui', 1)
  . ItemGroupXml(@EditorItems)
  . qq{  <ItemGroup>
    <!-- Builds KirbyRuntime first and links KirbyRuntime.lib -->
    <ProjectReference Include="Engine\\Source\\Runtime\\KirbyRuntime.vcxproj">
      <Project>$RuntimeGuid</Project>
    </ProjectReference>
  </ItemGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets">
  </ImportGroup>
</Project>
};

WriteFile("$Root/KirbyEngine.vcxproj", $EditorProject);
WriteFile("$Root/KirbyEngine.vcxproj.filters", FiltersXml('', @EditorItems));

# ------------------------------------------------------------------
# Solution
# ------------------------------------------------------------------
WriteFile("$Root/KirbyEngine.slnx", qq{<Solution>
  <Configurations>
    <Platform Name="x64" />
    <Platform Name="x86" />
  </Configurations>
  <Folder Name="/Engine/">
    <Project Path="Engine/Source/Runtime/KirbyRuntime.vcxproj" Id="} . lc(substr($RuntimeGuid, 1, 36)) . qq{" />
  </Folder>
  <Folder Name="/Programs/">
    <Project Path="Engine/Programs/KirbyHeaderTool/KirbyHeaderTool.vcxproj" Id="} . lc(substr($HeaderToolGuid, 1, 36)) . qq{" />
  </Folder>
  <Project Path="KirbyEngine.vcxproj" Id="} . substr($EditorGuid, 1, 36) . qq{" />
</Solution>
});
