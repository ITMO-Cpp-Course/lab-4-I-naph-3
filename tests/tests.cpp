#include "file_handle.hpp"
#include "resource_error.hpp"
#include "resource_manager.hpp"
#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace lab4::resource;

// Вспомогательная функция для создания тестового файла
void create_test_file(const std::string& filename, const std::string& content)
{
    std::ofstream file(filename);
    file << content;
    file.close();
}

#pragma region FileHandleTests
TEST_CASE("FileHandle: Constructor and RAII", "[file][raii]")
{
    const std::string filename = "test_raii.txt";

    // Удаляем файл перед тестом, если существует
    if (fs::exists(filename))
    {
        fs::remove(filename);
    }

    {
        FileHandle fh(filename, "w");
        REQUIRE(fh.is_open());
        fh.write("RAII is working");
    } // Деструктор автоматически закроет файл

    REQUIRE(fs::exists(filename));

    // Проверяем содержимое
    std::ifstream ifs(filename);
    std::string content;
    std::getline(ifs, content);
    REQUIRE(content == "RAII is working");

    fs::remove(filename);
}

TEST_CASE("FileHandle: Move Semantics", "[file][move]")
{
    const std::string filename = "test_move.txt";

    create_test_file(filename, "Move test content");

    SECTION("Move constructor")
    {
        FileHandle fh1(filename, "r");
        REQUIRE(fh1.is_open());

        FileHandle fh2 = std::move(fh1);
        REQUIRE(fh2.is_open());
        REQUIRE_FALSE(fh1.is_open());

        std::string content = fh2.read(16);
        REQUIRE(content == "Move test content");
    }

    SECTION("Move assignment")
    {
        FileHandle fh1(filename, "r");
        FileHandle fh2("temp.txt", "w");

        fh2 = std::move(fh1);
        REQUIRE(fh2.is_open());
        REQUIRE_FALSE(fh1.is_open());

        fs::remove("temp.txt");
    }

    fs::remove(filename);
}
#pragma endregion

#pragma region ResourceErrorTests
TEST_CASE("ResourceError: Exception handling", "[error][exception]")
{
    const std::string invalid_path = "/non/existent/path/parmezan.txt";

    SECTION("Catching ResourceError on non-existent file")
    {
        REQUIRE_THROWS_AS(FileHandle(invalid_path, "r"), ResourceError);
    }

    SECTION("Checking error message content")
    {
        try
        {
            FileHandle fh(invalid_path, "r");
            REQUIRE(false); // Не должно дойти сюда
        }
        catch (const ResourceError& e)
        {
            std::string msg = e.what();
            REQUIRE(msg.find("Failed to open file") != std::string::npos);
            REQUIRE(msg.find("parmezan.txt") != std::string::npos);
        }
    }

    SECTION("Exception when reading from closed file")
    {
        const std::string filename = "test_closed.txt";
        create_test_file(filename, "test");

        FileHandle fh(filename, "r");
        fh.close();

        REQUIRE_THROWS_AS(fh.read(1), ResourceError);

        fs::remove(filename);
    }
}
#pragma endregion

#pragma region ResourceManagerTests
TEST_CASE("ResourceManager: Shared Ownership and Caching", "[manager][shared]")
{
    const std::string path = "shared_test.txt";
    create_test_file(path, "Shared content");
    ResourceManager manager;

    SECTION("Same file returns same object")
    {
        auto ptr1 = manager.get_file(path);
        auto ptr2 = manager.get_file(path);

        REQUIRE(ptr1 == ptr2);
        REQUIRE(ptr1.use_count() == 2);
        REQUIRE(manager.cache_size() == 1);
        REQUIRE(manager.is_cached(path));
    }

    SECTION("Automatic resource release from cache")
    {
        {
            auto ptr = manager.get_file(path);
            REQUIRE(ptr.use_count() == 1);
            REQUIRE(manager.cache_size() == 1);
        } // ptr уничтожается, weak_ptr истекает

        // Кеш всё ещё содержит weak_ptr, но is_cached вернёт false
        REQUIRE(manager.cache_size() == 1);
        REQUIRE_FALSE(manager.is_cached(path));

        // При следующем запросе создастся новый объект
        auto ptr_new = manager.get_file(path);
        REQUIRE(ptr_new.use_count() == 1);
        REQUIRE(manager.cache_size() == 1);
    }

    SECTION("Multiple files in cache")
    {
        const std::string path2 = "shared_test2.txt";
        create_test_file(path2, "Another content");

        auto ptr1 = manager.get_file(path);
        auto ptr2 = manager.get_file(path2);

        REQUIRE(ptr1 != ptr2);
        REQUIRE(manager.cache_size() == 2);
        REQUIRE(manager.is_cached(path));
        REQUIRE(manager.is_cached(path2));

        fs::remove(path2);
    }

    fs::remove(path);
}

TEST_CASE("ResourceManager: Cleanup logic", "[manager][cleanup]")
{
    const std::string path1 = "clean1.txt";
    const std::string path2 = "clean2.txt";

    create_test_file(path1, "Content 1");
    create_test_file(path2, "Content 2");

    ResourceManager manager;

    SECTION("cleanup_expired removes only expired entries")
    {
        auto f2 = manager.get_file(path2); // Оставляем живым

        {
            auto f1 = manager.get_file(path1);
            REQUIRE(manager.cache_size() == 2);
        } // f1 уничтожается

        // До очистки в кеше 2 записи
        REQUIRE(manager.cache_size() == 2);
        REQUIRE_FALSE(manager.is_cached(path1));
        REQUIRE(manager.is_cached(path2));

        manager.cleanup_expired();

        // После очистки остаётся только path2
        REQUIRE(manager.cache_size() == 1);
        REQUIRE_FALSE(manager.is_cached(path1));
        REQUIRE(manager.is_cached(path2));
    }

    SECTION("release_file explicitly removes from cache")
    {
        auto f1 = manager.get_file(path1);
        REQUIRE(manager.cache_size() == 1);

        manager.release_file(path1);
        REQUIRE(manager.cache_size() == 0);
        REQUIRE_FALSE(manager.is_cached(path1));
    }

    fs::remove(path1);
    fs::remove(path2);
}

TEST_CASE("ResourceManager: File operations through cached handles", "[manager][operations]")
{
    const std::string path = "cache_ops.txt";
    create_test_file(path, "Initial content");
    ResourceManager manager;

    SECTION("Write and read through cached handle")
    {
        auto fh1 = manager.get_file(path, "r+");
        fh1->write("Updated content");
        fh1->seek(0);

        auto fh2 = manager.get_file(path);
        std::string content = fh2->read(15);

        REQUIRE(content == "Updated content");
        REQUIRE(fh1 == fh2);
    }

    fs::remove(path);
}
#pragma endregion