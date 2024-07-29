ANALYSIS_SOURCES=Luau/Analysis/src/*.cpp
AST_SOURCES=Luau/Ast/src/*.cpp
CLI_SOURCES=Luau/CLI/FileUtils.cpp
CONFIG_SOURCES=Luau/Config/src/*.cpp

LUAU_SOURCES=$(ANALYSIS_SOURCES) $(AST_SOURCES) $(CLI_SOURCES) $(CONFIG_SOURCES)
LUAU_INCLUDE=-ILuau/Analysis/include -ILuau/Ast/include -ILuau/CLI -ILuau/Common/include -ILuau/Config/include

LUAU_SOURCES_BUILD=$(shell echo "$(LUAU_SOURCES)" | sed 's/Luau\//..\/Luau\//g' -)
LUAU_INCLUDE_BUILD=$(shell echo "$(LUAU_INCLUDE)" | sed 's/Luau\//..\/Luau\//g' -)

build:
	if [ ! -d "luau_build" ]; then \
		mkdir luau_build && cd luau_build; \
		echo "building luau..."; \
		g++ -c $(LUAU_SOURCES_BUILD) $(LUAU_INCLUDE_BUILD); \
		echo "luau built!"; \
		cd ..; \
	fi
	echo "building beautifier...";
	g++ main.cpp handle.cpp beautify/*.cpp luau_build/*.o -o luau-beautifier -Ibeautify $(LUAU_INCLUDE);

wasm:
	emcc -lembind handle.cpp beautify/*.cpp $(LUAU_SOURCES) -o luau-beautifier.js -Ibeautify $(LUAU_INCLUDE);