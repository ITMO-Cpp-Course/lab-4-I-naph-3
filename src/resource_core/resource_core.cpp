#include "lab4/resource_core.hpp"
#include <cstring>
#include <iostream>
#include <sstream>

namespace lab4::resource
{

ResourceError::ResourceError(const std::string& msg) : message(msg) {}

const char* ResourceError::what() const noexcept
{
    return message.c_str();
}

FileHandle::FileHandle(const std::string& filename, const std::string& mode)
    : file(nullptr), filename(filename), isOpen(false)
{
    reopen(filename, mode);
}

FileHandle::~FileHandle()
{
    close();
}

void FileHandle::close() noexcept
{
    if (file && isOpen)
    {
        std::fclose(file);
        file = nullptr;
        isOpen = false;
    }
}

void FileHandle::reopen(const std::string& newFilename, const std::string& mode)
{
    close();
    filename = newFilename;
    file = std::fopen(filename.c_str(), mode.c_str());

    if (!file)
    {
        throw ResourceError("Failed to open file: " + filename + " (error: " + std::strerror(errno) + ")");
    }
    isOpen = true;
}

void FileHandle::closeHandle()
{
    close();
}

FileHandle::FileHandle(FileHandle&& other) noexcept
    : file(other.file), filename(std::move(other.filename)), isOpen(other.isOpen)
{
    other.file = nullptr;
    other.isOpen = false;
}

FileHandle& FileHandle::operator=(FileHandle&& other) noexcept
{
    if (this != &other)
    {
        close();
        file = other.file;
        filename = std::move(other.filename);
        isOpen = other.isOpen;
        other.file = nullptr;
        other.isOpen = false;
    }
    return *this;
}

std::string FileHandle::readLine()
{
    if (!isOpen || !file)
    {
        throw ResourceError("File not open: " + filename);
    }

    std::string line;
    char buffer[4096];
    if (std::fgets(buffer, sizeof(buffer), file))
    {
        line = buffer;
        if (!line.empty() && line.back() == '\n')
        {
            line.pop_back();
        }
    }
    return line;
}

void FileHandle::writeLine(const std::string& line)
{
    if (!isOpen || !file)
    {
        throw ResourceError("File not open: " + filename);
    }

    if (std::fprintf(file, "%s\n", line.c_str()) < 0)
    {
        throw ResourceError("Failed to write to file: " + filename);
    }
    std::fflush(file);
}

bool FileHandle::eof() const
{
    if (!isOpen || !file)
    {
        return true;
    }
    return std::feof(file) != 0;
}

std::shared_ptr<FileHandle> ResourceManager::getResource(const std::string& filename, const std::string& mode)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto it = cache.find(filename);
    if (it != cache.end())
    {
        auto shared = it->second.lock();
        if (shared)
        {
            return shared;
        }
        else
        {
            cache.erase(it);
        }
    }

    auto handle = std::make_shared<FileHandle>(filename, mode);
    cache[filename] = handle;
    return handle;
}

void ResourceManager::evict(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(mutex);
    cache.erase(filename);
}

void ResourceManager::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = cache.begin(); it != cache.end();)
    {
        if (it->second.expired())
        {
            it = cache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool ResourceManager::hasCached(const std::string& filename) const
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = cache.find(filename);
    return it != cache.end() && !it->second.expired();
}

size_t ResourceManager::activeCount() const
{
    std::lock_guard<std::mutex> lock(mutex);
    size_t count = 0;
    for (const auto& pair : cache)
    {
        if (!pair.second.expired())
        {
            ++count;
        }
    }
    return count;
}

} // namespace lab4::resource