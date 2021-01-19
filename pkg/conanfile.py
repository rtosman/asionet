from conans import ConanFile, tools

class ASIOnetConan(ConanFile):
    name = "ASIOnet"
    version = "1.0"
    description = "TCP networking framework built on top of asio"
    topics = ("network", "tcp", "async", "parallel")
    exports_sources = "../common/*"
    no_copy_source = True
    generators = "premake"
    requires = ("asio/1.18.1", "botan/2.17.3")

    def package(self):
        self.copy("*.hpp", dst="include")

