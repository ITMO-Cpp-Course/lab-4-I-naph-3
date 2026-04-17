#include "file_handle.hpp"
#include "resource_error.hpp"
#include <cerrno>
#include <cstring>

namespace lab4::resource
{

FileHandle::FileHandle(const std::string& filename, const std::string& mode) : file_(nullptr), filename_(filename)
{

    file_ = std::fopen(filename.c_str(), mode.c_str());
    if (!file_)
    {
        throw ResourceError("Failed to open file: " + filename + " - " + std::strerror(errno));
    }
}

FileHandle::~FileHandle()
{
    close();
}

FileHandle::FileHandle(FileHandle&& other) noexcept : file_(other.file_), filename_(std::move(other.filename_))
{
    other.file_ = nullptr;
}

FileHandle& FileHandle::operator=(FileHandle&& other) noexcept
{
    if (this != &other)
    {
        close();
        file_ = other.file_;
        filename_ = std::move(other.filename_);
        other.file_ = nullptr;
    }
    return *this;
}

std::string FileHandle::read(size_t size)
{
    check_file();

    std::string buffer(size, '\0');
    size_t bytes_read = std::fread(buffer.data(), 1, size, file_);

    if (bytes_read == 0 && std::ferror(file_))
    {
        throw ResourceError("Failed to read from file: " + filename_);
    }

    buffer.resize(bytes_read);
    return buffer;
}

void FileHandle::write(const std::string& data)
{
    check_file();

    size_t bytes_written = std::fwrite(data.c_str(), 1, data.size(), file_);

    if (bytes_written != data.size())
    {
        throw ResourceError("Failed to write to file: " + filename_);
    }
}

bool FileHandle::is_open() const
{
    return file_ != nullptr;
}

void FileHandle::close() noexcept
{
    if (file_)
    {
        std::fclose(file_);
        file_ = nullptr;
    }
}

long FileHandle::tell() const
{
    check_file();

    long pos = std::ftell(file_);
    if (pos == -1L)
    {
        throw ResourceError("Failed to get position in file: " + filename_);
    }
    return pos;
}

bool FileHandle::seek(long offset, int origin)
{
    check_file();
    return std::fseek(file_, offset, origin) == 0;
}

const std::string& FileHandle::filename() const noexcept
{
    return filename_;
}

bool FileHandle::eof() const
{
    check_file();
    return std::feof(file_) != 0;
}

void FileHandle::check_file() const
{
    if (!file_)
    {
        throw ResourceError("File is not open: " + filename_);
    }
}

} // namespace lab4::resource