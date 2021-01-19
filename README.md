# asionet

Sample client server built around a small framework called asionet that is a thin layer
over asio specializing it for tcp.

Build notes:

Prequiresites are:
  - premake5 (if you are using msvc the latest binary will work fine otherwise you 
    need to build from master), link: https://premake.github.io/download.html
  - conan (https://conan.io/)

Written in C++17 Requires VS2019 (with or without clang toolchain)
  - To use with msvc rather than clang comment out the line "toolset('clang')"
    in premake5.lua

To configure the solution (assuming conand and premake5 are on the path):

> \> mkdir build && cd build
> \> conan install ..

Then

> \> cd ..
> \> premake5 --scripts=build vs2019

Once that is done you will have a solution named ASIO.sln, just open that up with VS2019
and build
