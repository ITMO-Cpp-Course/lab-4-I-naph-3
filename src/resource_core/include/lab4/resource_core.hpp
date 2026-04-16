#pragma once

#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace lab4::resource
{

class ResourceError : public std::exception
{
  private:
    std::string message;

  public:
    explicit ResourceError(const std::string& msg);
    const char* what() const noexcept override;
};

class FileHandle
{
  private:
    FILE* file;
    std::string filename;
    bool isOpen;

    void close() noexcept;

  public:
    explicit FileHandle(const std::string& filename, const std::string& mode = "r");

    ~FileHandle();
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    FileHandle(FileHandle&& other) noexcept;
    FileHandle& operator=(FileHandle&& other) noexcept;

    void reopen(const std::string& filename, const std::string& mode = "r");
    void closeHandle();
    bool isOpen() const noexcept
    {
        return isOpen;
    }
    const std::string& getFilename() const noexcept
    {
        return filename;
    }

    FILE* get() noexcept
    {
        return file;
    }
    const FILE* get() const noexcept
    {
        return file;
    }

    std::string readLine();
    void writeLine(const std::string& line);
    bool eof() const;
};

class ResourceManager
{
  private:
    std::unordered_map<std::string, std::weak_ptr<FileHandle>> cache;
    mutable std::mutex mutex;

  public:
    ResourceManager() = default;
    ~ResourceManager() = default;

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    std::shared_ptr<FileHandle> getResource(const std::string& filename, const std::string& mode = "r");

    void evict(const std::string& filename);

    void cleanup();

    bool hasCached(const std::string& filename) const;

    size_t activeCount() const;
};

} // namespace lab4::resource