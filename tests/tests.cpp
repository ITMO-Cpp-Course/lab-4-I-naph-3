#include "lab4/resource_core.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <thread>
#include <vector>

using namespace lab4::resource;
using namespace Catch::Matchers;

struct TempFile
{
    std::string path;
    TempFile(const std::string& name, const std::string& content = "") : path(name)
    {
        std::ofstream file(path);
        file << content;
    }
    ~TempFile()
    {
        std::remove(path.c_str());
    }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
};

TEST_CASE("FileHandle - RAII and basic operations", "[FileHandle]")
{
    TempFile temp("test.txt", "Hello\nWorld\n");

    SECTION("Constructor opens file, destructor closes it")
    {
        {
            FileHandle fh(temp.path, "r");
            REQUIRE(fh.isOpen());
        }
        FileHandle fh2(temp.path, "r");
        REQUIRE(fh2.isOpen());
    }

    SECTION("Read and write operations work correctly")
    {
        FileHandle fh(temp.path, "w");
        fh.writeLine("Line1");
        fh.writeLine("Line2");
        fh.closeHandle();

        FileHandle fh2(temp.path, "r");
        REQUIRE(fh2.readLine() == "Line1");
        REQUIRE(fh2.readLine() == "Line2");
        REQUIRE(fh2.eof());
    }

    SECTION("Error handling throws ResourceError")
    {
        REQUIRE_THROWS_AS(FileHandle("nonexistent.txt", "r"), ResourceError);

        FileHandle fh(temp.path, "r");
        fh.closeHandle();
        REQUIRE_THROWS_AS(fh.readLine(), ResourceError);
    }

    SECTION("Move semantics transfers ownership")
    {
        FileHandle fh1(temp.path, "r");
        FileHandle fh2 = std::move(fh1);
        REQUIRE_FALSE(fh1.isOpen());
        REQUIRE(fh2.isOpen());
        REQUIRE(fh2.getFilename() == temp.path);
    }
}

TEST_CASE("ResourceManager - Caching and shared ownership", "[ResourceManager]")
{
    TempFile temp("test.txt", "Content");
    ResourceManager rm;

    SECTION("Same resource returned from cache")
    {
        auto ptr1 = rm.getResource(temp.path, "r");
        auto ptr2 = rm.getResource(temp.path, "r");
        REQUIRE(ptr1.get() == ptr2.get());
        REQUIRE(rm.activeCount() == 1);
    }

    SECTION("Different modes create different handles")
    {
        auto ptr1 = rm.getResource(temp.path, "r");
        auto ptr2 = rm.getResource(temp.path, "w");
        REQUIRE(ptr1.get() != ptr2.get());
        REQUIRE(rm.activeCount() == 2);
    }

    SECTION("Shared file state between owners")
    {
        auto owner1 = rm.getResource(temp.path, "r");
        auto owner2 = rm.getResource(temp.path, "r");

        REQUIRE(owner1->readLine() == "Content");
        REQUIRE(owner2->eof());
    }

    SECTION("Cleanup removes expired resources")
    {
        auto ptr = rm.getResource(temp.path, "r");
        REQUIRE(rm.hasCached(temp.path));

        ptr.reset();
        rm.cleanup();

        REQUIRE_FALSE(rm.hasCached(temp.path));
        REQUIRE(rm.activeCount() == 0);
    }

    SECTION("Evict removes from cache but keeps alive if used")
    {
        auto ptr = rm.getResource(temp.path, "r");
        rm.evict(temp.path);
        REQUIRE_FALSE(rm.hasCached(temp.path));
        REQUIRE(ptr->isOpen());
    }
}

TEST_CASE("Integration - Complete workflow", "[integration]")
{
    TempFile temp("workflow.txt");
    ResourceManager rm;

    {
        auto writer = rm.getResource(temp.path, "w");
        writer->writeLine("Data1");
        writer->writeLine("Data2");
    }

    auto reader = rm.getResource(temp.path, "r");
    REQUIRE(reader->readLine() == "Data1");
    REQUIRE(reader->readLine() == "Data2");
    REQUIRE(reader->eof());

    REQUIRE(rm.activeCount() == 1);
    REQUIRE(rm.hasCached(temp.path));
}
TEST_CASE("ResourceManager - Thread safety", "[ResourceManager][!mayfail]")
{
    TempFile temp("concurrent.txt", "data");
    ResourceManager rm;
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<FileHandle>> handles(10);

    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([&rm, &handles, i, &temp]() { handles[i] = rm.getResource(temp.path, "r"); });
    }

    for (auto& t : threads)
        t.join();

    for (int i = 1; i < 10; ++i)
    {
        REQUIRE(handles[0].get() == handles[i].get());
    }
}