# Luau Beautifier

A beautifier for Luau written in C++

## Replit
You can use luau_beautifier without compiling with [this replit](https://replit.com/@TechHog/luaubeautifier-site).

![](assets/replit1.png)
![](assets/replit2.png)
![](assets/replit3.png)

## Build
> ```sh
> $ make
> ```

## Usage
> Usage: ./luau-beautifier [options] [file]
> <br></br>
> options:<br>
> &nbsp;&nbsp;--minify: switches output mode from beautify to minify<br>
> &nbsp;&nbsp;--nosolve: doesn't solve simple expressions

If there are errors parsing (both CLI options or the input code), you will see those in stderr.<br>
Otherwise, the beautified code will appear in stdout.

## Other notes
1. The [Luau version used](https://github.com/luau-lang/luau/releases/tag/0.630) is 0.630
2. Currently there is no type support