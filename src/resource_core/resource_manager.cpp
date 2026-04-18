#include "resource_manager.hpp"
#include "resource_error.hpp"

namespace lab4::resource
{

ResourceManager::FileHandlePtr ResourceManager::get_file(const std::string& filename, const std::string& mode)
{
    auto it = cache_.find(filename);
    if (it != cache_.end())
    {
        auto shared = it->second.lock();
        if (shared)
        {
            return shared;
        }
        cache_.erase(it);
    }

    try
    {
        auto file = std::make_shared<FileHandle>(filename, mode);
        cache_[filename] = file;
        return file;
    }
    catch (const ResourceError&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw ResourceError(std::string("Failed to create file handle: ") + e.what());
    }
}

void ResourceManager::release_file(const std::string& filename)
{
    cache_.erase(filename);
}

void ResourceManager::cleanup_expired()
{
    for (auto it = cache_.begin(); it != cache_.end();)
    {
        if (it->second.expired())
        {
            it = cache_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

size_t ResourceManager::cache_size() const
{
    return cache_.size();
}

bool ResourceManager::is_cached(const std::string& filename) const
{
    auto it = cache_.find(filename);
    return it != cache_.end() && !it->second.expired();
}

} // namespace lab4::resource