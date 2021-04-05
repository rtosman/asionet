-- premake5.lua

newoption {
   trigger = "toolset",
   value = "clang/msc",
   description = "Choose a toolset to build with",
   allowed = {
      { "clang",    "Clang" },
      { "msc",  "Microsoft C/C++"} ,
      { "gcc", "GNU Compiler Collection" }
   }
}

if not _OPTIONS["toolset"] then
   _OPTIONS["toolset"] = 'clang'
end

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
      toolset(_OPTIONS["toolset"])
      defines { "_WIN32_WINNT=0x0601" }
      platforms { "Win32", "Win64" }
      filter { "toolset:clang" }
         buildoptions { "-Wno-undefined-internal", "-Wno-unused-private-field", "-Wno-unknown-attributes" }
      filter { "toolset:msc" }
         disablewarnings { "5051", "4267", "4146", "4244", "4996" }

   filter "system:linux"
      toolset(_OPTIONS["toolset"])
      platforms { "linux-x32", "linux-x64" }
      buildoptions { "-Wno-undefined-internal", "-Wno-unused-private-field", "-Wno-unknown-attributes" }

project "ServerOne"
   kind "ConsoleApp"
   targetdir "bin/%{cfg.buildcfg}"

   files { "one/**.cpp", "serverone/**.hpp", "serverone/**.cpp" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

project "ClientOne"
   kind "ConsoleApp"
   targetdir "bin/%{cfg.buildcfg}"

   files { "one/**.cpp", "clientone/**.hpp", "clientone/**.cpp" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      
