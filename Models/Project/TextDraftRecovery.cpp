#include "Models/Project/TextDraftRecovery.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include <fstream>
#include <array>
#include <limits>
#include <sstream>

namespace TextDraftRecovery
{
namespace
{
constexpr std::size_t Limit = 8U * 1024U * 1024U;
std::filesystem::path pathFor(const Campaign &campaign, const std::string &key)
{
    if (key.empty() || key.size() > 160 || campaign.metadata.id.empty())
        throw std::runtime_error("Invalid recovery identity.");
    // Only simple caller-owned keys become file names. Campaign paths use the
    // same sanitized stable identity as the rest of package storage.
    for (const unsigned char c : key)
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '-' && c != '_')
            throw std::runtime_error("Invalid recovery key.");
    return ProjectPackageLayout::layoutForCampaign(campaign).recoveryDirectory / "Drafts" / (key + ".draft");
}
void field(std::string &out, const std::string &value)
{
    const auto prefix = std::to_string(value.size()) + ":";
    if (value.size() > Limit || out.size() + value.size() + prefix.size() > Limit)
        throw std::runtime_error("Text draft exceeds the recovery size limit.");
    out += prefix + value;
}
std::string take(const std::string &bytes, std::size_t &offset)
{
    const auto end = bytes.find(':', offset);
    if (end == std::string::npos || end == offset || end - offset > 8)
        throw std::runtime_error("Invalid text recovery data.");
    std::size_t size = 0;
    for (auto i = offset; i < end; ++i)
    {
        if (bytes[i] < '0' || bytes[i] > '9') throw std::runtime_error("Invalid recovery length.");
        size = size * 10 + static_cast<unsigned>(bytes[i] - '0');
    }
    offset = end + 1;
    if (size > Limit || size > bytes.size() - offset) throw std::runtime_error("Truncated text recovery data.");
    auto value = bytes.substr(offset, size);
    offset += size;
    return value;
}
bool safePath(const std::filesystem::path &path)
{
    const auto absolute=std::filesystem::absolute(path).lexically_normal();
    const auto inspect=[&](const std::filesystem::path &part,bool leaf) {
        std::error_code error;
        const auto status=std::filesystem::symlink_status(mapFileIoPath(part),error);
        if(error&&error!=std::errc::no_such_file_or_directory)
            throw std::filesystem::filesystem_error("Inspect recovery path",part,error);
        if(std::filesystem::is_symlink(status))
            throw std::runtime_error("Recovery paths cannot contain symbolic links.");
        if(!std::filesystem::exists(status))return false;
#ifdef _WIN32
        const auto flags = GetFileAttributesW(mapFileIoPath(part).c_str());
        if(flags==INVALID_FILE_ATTRIBUTES)
            throw std::system_error(GetLastError(),std::system_category(),"Inspect recovery attributes");
        if (flags & FILE_ATTRIBUTE_REPARSE_POINT)
            throw std::runtime_error("Recovery paths cannot contain reparse points.");
#endif
        if(leaf?!std::filesystem::is_regular_file(status):!std::filesystem::is_directory(status))
            throw std::runtime_error(leaf?"The recovery checkpoint is not a regular file.":"A recovery path ancestor is not a directory.");
        return true;
    };
    // Inspect parents before descendants: Windows may report a descendant of
    // a regular file as missing. That is an obstruction, not a clean absence.
    auto part=absolute.root_path();inspect(part,false);
    bool present=false;
    for(const auto &component:absolute.relative_path()) {
        part/=component;present=inspect(part,part==absolute);
    }
    return present;
}
}
ReadResult read(const Campaign &campaign, const std::string &key, bool retainRawBytes)
{
    ReadResult result;
    try
    {
        const auto path = pathFor(campaign, key);
        if (!safePath(path)) { result.success = true; return result; }
        result.found = true;
        if (std::filesystem::file_size(path) > Limit) throw std::runtime_error("Recovery file is too large.");
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot read text recovery file.");
        std::string bytes;
        std::array<char, 65536> buffer{};
        while (input)
        {
            input.read(buffer.data(), buffer.size());
            const auto count = static_cast<std::size_t>(input.gcount());
            if (count > Limit - bytes.size()) throw std::runtime_error("Recovery file is too large.");
            bytes.append(buffer.data(), count);
        }
        if (input.bad() || (input.fail() && !input.eof())) throw std::runtime_error("Could not finish reading text recovery file.");
        if (retainRawBytes) { result.rawBytes = bytes; result.rawAvailable = true; }
        std::size_t offset = 0;
        if (take(bytes, offset) != "GM_TEXT_DRAFT_1" || take(bytes, offset) != campaign.metadata.id || take(bytes, offset) != key)
            throw std::runtime_error("Recovery version or identity does not match.");
        while (offset < bytes.size())
        {
            if (result.fields.size() >= 128) throw std::runtime_error("Too many text recovery fields.");
            const auto name = take(bytes, offset);
            const auto value = take(bytes, offset);
            if (name.empty() || !result.fields.emplace(name, value).second)
                throw std::runtime_error("Duplicate text recovery field.");
        }
        result.success = true;
    }
    catch (const std::exception &error) { result.error = error.what(); result.fields.clear(); }
    return result;
}
std::string save(const Campaign &campaign, const std::string &key, const Fields &fields)
{
    try
    {
        if (fields.size() > 128) throw std::runtime_error("Too many text recovery fields.");
        std::string bytes;
        field(bytes, "GM_TEXT_DRAFT_1"); field(bytes, campaign.metadata.id); field(bytes, key);
        for (const auto &[name, value] : fields)
        {
            if (name.empty()) throw std::runtime_error("Empty recovery field name.");
            field(bytes, name); field(bytes, value);
        }
        const auto path = pathFor(campaign, key);
        safePath(path);
        std::filesystem::create_directories(path.parent_path());
        safePath(path);
        AtomicProjectFile::write(path, bytes);
        return {};
    }
    catch (const std::exception &error) { return error.what(); }
}
std::string remove(const Campaign &campaign, const std::string &key)
{
    try {
        const auto path = pathFor(campaign, key);
        if(!safePath(path))return {};
        if(!std::filesystem::remove(mapFileIoPath(path))&&safePath(path))
            throw std::runtime_error("Recovery checkpoint was not removed.");
        return {};
    }
    catch (const std::exception &error) { return error.what(); }
}
}
