#include "file_handle.hpp"
#include "resource_error.hpp"
#include "resource_manager.hpp"
#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>

using namespace lab4::resource;

// Вспомогательная функция для создания тестового файла
void create_test_file(const std::string& filename, const std::string& content)
{
    std::ofstream file(filename);
    file << content;
    file.close();
}

TEST_CASE("ResourceError construction and inheritance", "[resource_error]")
{
    SECTION("Exception message")
    {
        const std::string msg = "Test error message";
        ResourceError error(msg);
        REQUIRE(std::string(error.what()) == msg);
    }

    SECTION("Exception inheritance")
    {
        ResourceError error("test");
        REQUIRE_THROWS_AS(throw error, std::exception);
    }
}

TEST_CASE("FileHandle RAII and basic operations", "[file_handle]")
{
    const std::string test_file = "test_raii.txt";
    const std::string test_content = "Hello, RAII!";

    // Очищаем перед тестами
    std::filesystem::remove(test_file);

    SECTION("Constructor throws on non-existent file in read mode")
    {
        REQUIRE_THROWS_AS(FileHandle("nonexistent_file.txt", "r"), ResourceError);
    }

    SECTION("File opens and closes automatically")
    {
        create_test_file(test_file, test_content);

        {
            FileHandle file(test_file, "r");
            REQUIRE(file.is_open());
            REQUIRE(file.filename() == test_file);
        } // Деструктор должен закрыть файл

        // Проверяем, что файл можно открыть снова (значит, был закрыт)
        FileHandle file2(test_file, "r");
        REQUIRE(file2.is_open());
    }

    SECTION("Read operation")
    {
        create_test_file(test_file, test_content);
        FileHandle file(test_file, "r");

        std::string content = file.read(test_content.size());
        REQUIRE(content == test_content);
    }

    SECTION("Write operation")
    {
        {
            FileHandle file(test_file, "w");
            file.write(test_content);
        } // Файл закроется здесь

        // Проверяем, что данные записались
        std::ifstream ifs(test_file);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        REQUIRE(content == test_content);
    }

    // Очищаем после тестов
    std::filesystem::remove(test_file);
}

TEST_CASE("FileHandle move semantics", "[file_handle]")
{
    const std::string test_file = "test_move.txt";
    create_test_file(test_file, "Move test content");

    SECTION("Move constructor transfers ownership")
    {
        FileHandle file1(test_file, "r");
        REQUIRE(file1.is_open());

        FileHandle file2(std::move(file1));
        REQUIRE(file2.is_open());
        REQUIRE_FALSE(file1.is_open()); // file1 больше не владеет ресурсом

        // Проверяем, что file2 может читать
        std::string content = file2.read(17);
        REQUIRE(content == "Move test content");
    }

    SECTION("Move assignment transfers ownership")
    {
        FileHandle file1(test_file, "r");
        FileHandle file2("temp.txt", "w");

        file2 = std::move(file1);
        REQUIRE(file2.is_open());
        REQUIRE_FALSE(file1.is_open());

        std::filesystem::remove("temp.txt");
    }

    std::filesystem::remove(test_file);
}

TEST_CASE("FileHandle advanced operations", "[file_handle]")
{
    const std::string test_file = "test_advanced.txt";
    const std::string content = "0123456789";

    create_test_file(test_file, content);

    SECTION("Seek and tell operations")
    {
        FileHandle file(test_file, "r");

        REQUIRE(file.tell() == 0);
        REQUIRE(file.seek(5));
        REQUIRE(file.tell() == 5);

        std::string part = file.read(3);
        REQUIRE(part == "567");

        REQUIRE(file.seek(0, SEEK_END));
        REQUIRE(file.eof() == false);
    }

    SECTION("EOF detection")
    {
        FileHandle file(test_file, "r");
        file.read(content.size());
        REQUIRE(file.eof() == true);
    }

    SECTION("Close operation")
    {
        FileHandle file(test_file, "r");
        REQUIRE(file.is_open());

        file.close();
        REQUIRE_FALSE(file.is_open());

        // Проверяем, что операции с закрытым файлом вызывают исключение
        REQUIRE_THROWS_AS(file.read(1), ResourceError);
    }

    std::filesystem::remove(test_file);
}

TEST_CASE("ResourceManager caching mechanism", "[resource_manager]")
{
    ResourceManager manager;
    const std::string test_file1 = "test_cache1.txt";
    const std::string test_file2 = "test_cache2.txt";

    create_test_file(test_file1, "Content 1");
    create_test_file(test_file2, "Content 2");

    SECTION("Basic caching")
    {
        REQUIRE(manager.cache_size() == 0);

        auto file1 = manager.get_file(test_file1);
        REQUIRE(manager.cache_size() == 1);
        REQUIRE(manager.is_cached(test_file1));

        // Получаем тот же файл - должен вернуться из кеша
        auto file1_again = manager.get_file(test_file1);
        REQUIRE(manager.cache_size() == 1);
        REQUIRE(file1 == file1_again);

        // Другой файл
        auto file2 = manager.get_file(test_file2);
        REQUIRE(manager.cache_size() == 2);
        REQUIRE(manager.is_cached(test_file2));
    }

    SECTION("Weak pointer expiration and cleanup")
    {
        {
            auto file1 = manager.get_file(test_file1);
            REQUIRE(manager.cache_size() == 1);
        } // file1 уничтожается, weak_ptr должен истечь

        REQUIRE(manager.cache_size() == 1);           // Кеш всё ещё содержит запись
        REQUIRE_FALSE(manager.is_cached(test_file1)); // Но файл уже не в кеше

        manager.cleanup_expired();
        REQUIRE(manager.cache_size() == 0); // После очистки запись удалена
    }

    SECTION("Release file")
    {
        auto file = manager.get_file(test_file1);
        REQUIRE(manager.cache_size() == 1);

        manager.release_file(test_file1);
        REQUIRE(manager.cache_size() == 0);
        REQUIRE_FALSE(manager.is_cached(test_file1));
    }

    SECTION("Multiple references to same file")
    {
        auto file1 = manager.get_file(test_file1);
        auto file2 = manager.get_file(test_file1);
        auto file3 = manager.get_file(test_file1);

        REQUIRE(manager.cache_size() == 1);
        REQUIRE(file1 == file2);
        REQUIRE(file2 == file3);

        // Все указывают на один файл
        file1->write("Shared content");

        file2->seek(0);
        std::string content = file3->read(14);
        REQUIRE(content == "Shared content");
    }

    std::filesystem::remove(test_file1);
    std::filesystem::remove(test_file2);
}

TEST_CASE("ResourceManager thread safety", "[resource_manager]")
{
    ResourceManager manager;
    const std::string test_file = "test_thread.txt";
    create_test_file(test_file, "Thread test");

    SECTION("Concurrent access to same file")
    {
        std::vector<std::thread> threads;
        std::vector<std::shared_ptr<FileHandle>> handles;
        std::mutex handles_mutex;

        for (int i = 0; i < 10; ++i)
        {
            threads.emplace_back([&]() {
                auto handle = manager.get_file(test_file);
                std::lock_guard<std::mutex> lock(handles_mutex);
                handles.push_back(handle);
            });
        }

        for (auto& t : threads)
        {
            t.join();
        }

        // Все должны получить один и тот же объект
        REQUIRE(manager.cache_size() == 1);
        for (size_t i = 1; i < handles.size(); ++i)
        {
            REQUIRE(handles[0] == handles[i]);
        }
    }

    std::filesystem::remove(test_file);
}

TEST_CASE("Error handling in ResourceManager", "[resource_manager]")
{
    ResourceManager manager;

    SECTION("Getting non-existent file in read mode")
    {
        REQUIRE_THROWS_AS(manager.get_file("nonexistent.txt", "r"), ResourceError);
    }

    SECTION("Getting file in invalid directory")
    {
        REQUIRE_THROWS_AS(manager.get_file("/invalid/path/file.txt", "w"), ResourceError);
    }
}