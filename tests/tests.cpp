#include "file_handle.hpp"
#include "resource_error.hpp"
#include "resource_manager.hpp"
#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>

using namespace lab4::resource;
namespace fs = std::filesystem;

void create_file(const std::string& name, const std::string& content)
{
    std::ofstream f(name);
    f << content;
}

TEST_CASE("FileHandle: basic operations", "[FileHandle]")
{
    std::string name = "test_basic.txt";

    SECTION("Write and read")
    {
        FileHandle f(name, "w");
        REQUIRE(f.is_open());
        f.write("Hello world");
        f.close();

        FileHandle f2(name, "r");
        std::string content = f2.read(100);
        REQUIRE(content == "Hello world");
    }

    SECTION("Constructor throws on bad file")
    {
        REQUIRE_THROWS_AS(FileHandle("/bad/path.txt", "r"), ResourceError);
    }

    fs::remove(name);
}

TEST_CASE("FileHandle: move semantics", "[FileHandle]")
{
    std::string name = "test_move.txt";
    create_file(name, "Move test");

    SECTION("Move constructor")
    {
        FileHandle f1(name, "r");
        FileHandle f2 = std::move(f1);

        REQUIRE(f2.is_open());
        REQUIRE_FALSE(f1.is_open());
        REQUIRE(f2.read(100) == "Move test");
    }

    SECTION("Move assignment")
    {
        FileHandle f1(name, "r");
        FileHandle f2("temp.txt", "w");
        f2 = std::move(f1);

        REQUIRE(f2.is_open());
        REQUIRE_FALSE(f1.is_open());
        fs::remove("temp.txt");
    }

    fs::remove(name);
}

TEST_CASE("ResourceError", "[ResourceError]")
{
    SECTION("Throws on bad file")
    {
        REQUIRE_THROWS_AS(FileHandle("no_such_file.txt", "r"), ResourceError);
    }

    SECTION("Throws on read from closed file")
    {
        create_file("test.txt", "data");
        FileHandle f("test.txt", "r");
        f.close();
        REQUIRE_THROWS_AS(f.read(1), ResourceError);
        fs::remove("test.txt");
    }
}

TEST_CASE("ResourceManager: caching", "[ResourceManager]")
{
    std::string name = "cache_test.txt";
    create_file(name, "Cached content");

    ResourceManager mgr;

    SECTION("Same file returns same object")
    {
        auto p1 = mgr.get_file(name);
        auto p2 = mgr.get_file(name);

        REQUIRE(p1 == p2);
        REQUIRE(p1.use_count() == 2);
        REQUIRE(mgr.cache_size() == 1);
        REQUIRE(mgr.is_cached(name));
    }

    SECTION("Cache cleanup")
    {
        {
            auto p = mgr.get_file(name);
            REQUIRE(mgr.cache_size() == 1);
        }

        REQUIRE_FALSE(mgr.is_cached(name));
        mgr.cleanup_expired();
        REQUIRE(mgr.cache_size() == 0);
    }

    SECTION("Release file")
    {
        auto p = mgr.get_file(name);
        mgr.release_file(name);
        REQUIRE(mgr.cache_size() == 0);
        REQUIRE_FALSE(mgr.is_cached(name));
    }

    fs::remove(name);
}

TEST_CASE("ResourceManager: multiple files", "[ResourceManager]")
{
    create_file("f1.txt", "one");
    create_file("f2.txt", "two");

    ResourceManager mgr;

    auto p1 = mgr.get_file("f1.txt");
    auto p2 = mgr.get_file("f2.txt");

    REQUIRE(p1 != p2);
    REQUIRE(mgr.cache_size() == 2);
    REQUIRE(mgr.is_cached("f1.txt"));
    REQUIRE(mgr.is_cached("f2.txt"));

    fs::remove("f1.txt");
    fs::remove("f2.txt");
}

TEST_CASE("Empty ResourceManager", "[ResourceManager]")
{
    ResourceManager mgr;

    REQUIRE(mgr.cache_size() == 0);
    REQUIRE_FALSE(mgr.is_cached("any.txt"));
}
