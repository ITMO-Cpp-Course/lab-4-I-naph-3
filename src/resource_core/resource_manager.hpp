#pragma once
#include "file_handle.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace lab4::resource
{

class ResourceManager
{
  public:
    using FileHandlePtr = std::shared_ptr<FileHandle>;
    using WeakFileHandlePtr = std::weak_ptr<FileHandle>;

    FileHandlePtr get_file(const std::string& filename, const std::string& mode = "r");
    void release_file(const std::string& filename);
    void cleanup_expired();
    size_t cache_size() const;
    bool is_cached(const std::string& filename) const;

  private:
    std::unordered_map<std::string, WeakFileHandlePtr> cache_;
};

} // namespace lab4::resource