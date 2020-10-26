# asionet

Build notes:

Prequiresites are:
  - premake5 (if you are using msvc the latest binary will work fine otherwise you 
    need to build from master), link: https://premake.github.io/download.html
  - asio-1.18.0 standalone (i.e. no "boost" prefix), link: http://think-async.com/Asio/Download.html
  - Botan (latest version), link: https://github.com/randombit/botan 

Written in C++17 Requires VS2019 (with or without clang toolchain)
  - To use with msvc rather than clang comment out the line "toolset('clang')"
    in premake5.lua

You'll need to edit premake5.lua to set the paths to asio and Botan appropriately. Once
the paths are set correctly at the command line (in the folder that contains premake5.lua) 
type (assuming that premake5 is on the path)

> \> premake5 vs2019

Once that is done you will have a solution named ASIO.sln, just open that up with VS2019
and build solution
