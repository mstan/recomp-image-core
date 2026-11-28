#include "recomp_image/byte_source.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#endif
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        const auto root = std::filesystem::u8path(argv[1]);
        std::filesystem::create_directories(root);
        const auto path = root / std::filesystem::u8path("physical-\xc3\xa9.img");
        struct Cleanup { std::filesystem::path path; ~Cleanup() {std::error_code ec;std::filesystem::remove(path,ec);} } cleanup{path};
        constexpr uint64_t high = (uint64_t(2) << 30) + 17;
#ifdef _WIN32
        const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(file != INVALID_HANDLE_VALUE);
        DWORD ignored = 0;
        const bool sparse = DeviceIoControl(file, FSCTL_SET_SPARSE, nullptr, 0,
                                            nullptr, 0, &ignored, nullptr) != 0;
        CloseHandle(file);
        CHECK(sparse);
#endif
        { std::ofstream file(path, std::ios::binary); file.seekp(high); file.write("abcd", 4); CHECK(file.good()); }
        recomp_image::ByteSource source(path);
        CHECK(source.size() == high + 4);
        std::array<char, 4> out{};
        source.read_into(high, out.data(), out.size());
        CHECK((out == std::array<char,4>{'a','b','c','d'}));
        source.read_into(source.size(), nullptr, 0);
        const auto rejected = [&](uint64_t at, void *p, uint64_t size) {
            try { source.read_into(at, p, size); return false; }
            catch (const std::runtime_error&) { return true; }
        };
        CHECK(rejected(high + 3, out.data(), 2));
        CHECK(rejected(UINT64_MAX, out.data(), 1));
        CHECK(rejected(0, nullptr, 1));
        source.read_into(high + 1, out.data(), 2);
        CHECK(out[0] == 'b' && out[1] == 'c');
        const auto bytes = source.read(high, 4, 4);
        CHECK(std::string(bytes.begin(), bytes.end()) == "abcd");
        bool budget = false;
        try { source.read(high, 4, 3); } catch (const std::runtime_error&) { budget = true; }
        CHECK(budget);
        std::filesystem::resize_file(path,high+1);
        CHECK(rejected(high,out.data(),4));
        source.read_into(high,out.data(),1); CHECK(out[0]=='a');
        std::puts("bounded physical reads, UTF-8, large offsets and recovery passed");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
