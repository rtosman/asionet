-- premake5.lua
localIncludePrefix = "E:/Users/Rennie Allen/source/opensource"
localLibPrefix = localIncludePrefix -- for the author these are equivalent
localAsioPrefix = "E:/Users/Rennie Allen/source/include/asio-1.18.0"
workspace "ASIO"
   configurations { "Debug", "Release" }
   includedirs { 
      localAsioPrefix .. "/include",
      "%{prj.location}/common",
      "%{prj.location}/one",
   }
   links { "botan" }

   language "C++"
   cppdialect "C++17" 
   filter "system:windows"
      toolset('clang')
      defines { "_WIN32_WINNT=0x0601" }
      platforms { "Win32", "Win64" }
      buildoptions { "-Wno-undefined-internal", "-Wno-unused-private-field", "-Wno-unknown-attributes" }

   filter "platforms:Win32"
      includedirs {
         localIncludePrefix .. "/include/botan-2/x86"
      }
      libdirs {
         localLibPrefix .. "/lib/x86"
      }
      architecture "x32"
  
   filter "platforms:Win64"
      includedirs {
         localIncludePrefix .. "/include/botan-2/x64"
      }
      libdirs {
         localLibPrefix .. "/lib/x64"
      }
      architecture "x64"

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
      