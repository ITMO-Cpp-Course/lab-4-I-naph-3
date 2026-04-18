#pragma once
#include <cstdio>
#include <string>

namespace lab4::resource
{

class FileHandle
{
  public:
    explicit FileHandle(const std::string& filename, const std::string& mode = "r");
    ~FileHandle();

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    FileHandle(FileHandle&& other) noexcept;
    FileHandle& operator=(FileHandle&& other) noexcept;

    std::string read(size_t size);
    void write(const std::string& data);
    bool is_open() const;
    void close() noexcept;

    long tell() const;
    bool seek(long offset, int origin = SEEK_SET);
    bool eof() const;

    const std::string& filename() const noexcept;

  private:
    std::FILE* file_;
    std::string filename_;

    void check_file() const;
};

} // namespace lab4::resource