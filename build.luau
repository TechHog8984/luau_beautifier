local fs = require("@lune/fs")
local stdio = require("@lune/stdio")
local process = require("@lune/process")

local build_wasm = false
local silent = false

for _, arg in process.args do
    if arg == "build" then
        continue
    elseif arg == "wasm" then
        build_wasm = true
        continue
    elseif arg == "silent" or arg == "-s" then
        silent = true
        continue
    end

    stdio.ewrite("unexpected argument '" .. arg .. "'\n")
    process.exit(1)
end

local log = if silent then function(...) end else print

local function searchForCPPFiles(dir: string, parent_dir: { string })
    for _, name in fs.readDir(dir) do
        if name:sub(-4, -1) ~= ".cpp" then
            continue
        end
        parent_dir[#parent_dir + 1] = dir .. '/' .. name
    end
end

local LUAU_SOURCES = { "Luau/CLI/FileUtils.cpp" }
searchForCPPFiles("Luau/Analysis/src", LUAU_SOURCES)
searchForCPPFiles("Luau/Ast/src", LUAU_SOURCES)
searchForCPPFiles("Luau/Config/src", LUAU_SOURCES)

local BEAUTIFIER_SOURCES = {}
searchForCPPFiles("beautify", BEAUTIFIER_SOURCES)

local LUAU_INCLUDE = {"-ILuau/Analysis/include", "-ILuau/Ast/include", "-ILuau/CLI", "-ILuau/Common/include", "-ILuau/Config/include"}

local function replaceLuau(list: { string }): { string }
    local new : { string } = {}
    for index, value in list do
        new[index] = (value:gsub("Luau", "../Luau"))
    end
    return new
end
local LUAU_SOURCES_BUILD = replaceLuau(LUAU_SOURCES)
local LUAU_INCLUDE_BUILD = replaceLuau(LUAU_INCLUDE)

local function spawnProcess(command: string, option_list: { string | { string }}?)
    log("running command " .. command, option_list)
    local new_options: { string } = {}
    if option_list then
        for _, option_or_list in option_list do
            if type(option_or_list) == "string" then
                new_options[#new_options + 1] = option_or_list
            else
                for _, option in option_or_list do
                    new_options[#new_options + 1] = option
                end
            end
        end
    end

    local handle = process.spawn(command, new_options)
    if not handle.ok then
        stdio.ewrite(handle.stderr .. '\n')
        process.exit(1)
    end
end

if not fs.isDir("luau_build") then
    fs.writeDir("luau_build")
    spawnProcess("cd", {"luau_build"})
    log("building luau...")
    spawnProcess("g++", {
        "-std=c++17",
        "-c",
        LUAU_SOURCES_BUILD,
        LUAU_INCLUDE_BUILD
    })
    log("luau done building")
    spawnProcess("cd", {".."})
end

local LUAU_OUTPUT = {}
for index, name in fs.readDir("luau_build") do
    LUAU_OUTPUT[index] = "luau_build/" .. name
end

log("building beautifier")

if build_wasm then
    spawnProcess("emcc", {
        "-std=c++17",
        "-lembind",
        "handle.cpp",
        BEAUTIFIER_SOURCES,
        LUAU_SOURCES,
        "-o",
        "luau-beautifier.js",
        "-Ibeautify",
        LUAU_INCLUDE
    })
else
    spawnProcess("g++", {
        "-std=c++17",
        "main.cpp",
        "handle.cpp",
        BEAUTIFIER_SOURCES,
        LUAU_OUTPUT,
        "-o",
        "luau-beautifier",
        "-Ibeautify",
        LUAU_INCLUDE
    })
end