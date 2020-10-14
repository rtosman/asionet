-- premake5.lua
workspace "ASIO"
   configurations { "Debug", "Release" }
   includedirs { 
      "E:/Users/Rennie Allen/source/include/asio-1.18.0/include",
      "%{prj.location}/common",
      "%{prj.location}/one"
   }
   language "C++"
   cppdialect "C++17" 
   filter "system:windows"
      toolset('clang')
      defines { "_WIN32_WINNT=0x0601" }
      platforms { "Win32", "Win64" }
      
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
