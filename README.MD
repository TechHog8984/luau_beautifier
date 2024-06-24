# Luau Beautifier

A beautifier for Luau written in C++

## Replit
You can use luau_beautifier without compiling with [this replit](https://replit.com/@TechHog/luaubeautifier-site).

## Build
> ```sh
> $ make
> ```

## Usage
> ```sh
> $ luau-beautifier filepath
> ```

If there are errors parsing, you will see those in stderr.<br>
Otherwise, the beautified code will appear in stdout.

## Other notes
1. The [Luau version used](https://github.com/luau-lang/luau/releases/tag/0.630) is 0.630
2. Currently there is no type support