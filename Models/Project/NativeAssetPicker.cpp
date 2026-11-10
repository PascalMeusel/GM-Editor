#include "Models/Project/NativeAssetPicker.hpp"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <commdlg.h>
#endif
namespace gm::assets
{
std::optional<std::filesystem::path> chooseImage(std::string &error, bool documents)
{
    error.clear();
#if defined(_WIN32)
    wchar_t file[32768] = {};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = documents ? L"Images and documents\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.pdf;*.txt\0\0"
                                   : L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = 32768;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog))
        return std::filesystem::path(file);
    if (const auto code = CommDlgExtendedError())
        error = "Image picker failed: " + std::to_string(code);
#else
    error = "Image browsing is unavailable on this platform.";
#endif
    return std::nullopt;
}
} // namespace gm::assets
