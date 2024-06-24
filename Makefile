ANALYSIS_SOURCES=Luau/Analysis/src/*.cpp
AST_SOURCES=Luau/Ast/src/*.cpp
CLI_SOURCES=Luau/CLI/FileUtils.cpp
CONFIG_SOURCES=Luau/Config/src/*.cpp

build:
	g++ main.cpp $(ANALYSIS_SOURCES) $(AST_SOURCES) $(CLI_SOURCES) $(CONFIG_SOURCES) beautify/*.cpp -o luau-beautifier -Ibeautify -ILuau/Analysis/include -ILuau/Ast/include -ILuau/CLI -ILuau/Common/include -ILuau/Config/include