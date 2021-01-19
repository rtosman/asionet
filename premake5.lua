-- premake5.lua
include("conanbuildinfo.premake.lua")

workspace "ASIO"
   configurations { "Debug", "Release" }
   includedirs { conan_includedirs, "common", "one" }
   linkoptions { conan_exelinkflags }
   libdirs { conan_libdirs }
   links { conan_libs, "crypt32" }

   language "C++"
   cppdialect "C++17" 
   filter "system:windows"
      toolset('clang')
      defines { "_WIN32_WINNT=0x0601" }
      platforms { "Win32", "Win64" }
      buildoptions { "-Wno-undefined-internal", "-Wno-unused-private-field", "-Wno-unknown-attributes" }

   filter "system:linux"
      toolset('clang')
      platforms { "linux-x32", "linux-x64" }
      buildoptions { "-Wno-undefined-internal", "-Wno-unused-private-field", "-Wno-unknown-attributes" }

project "ServerOne"
   kind "ConsoleApp"
   targetdir "bin/%{cfg.buildcfg}"

   files { "serverone/**.hpp", "serverone/**.cpp" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"


project "ClientOne"
   kind "ConsoleApp"
   targetdir "bin/%{cfg.buildcfg}"

   files { "clientone/**.hpp", "clientone/**.cpp" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      
