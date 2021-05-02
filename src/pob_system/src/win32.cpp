#include <pob_system/user_path_helper.h>

#define WIN32_LEAN_AND_MEAN
#include <Shlobj.h>

#include <string>
#include <string_view>

std::string utf8_encode(const std::wstring_view &wstr)
{
    if (wstr.empty())
        return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::string get_user_home_directory()
{
    PWSTR wstr;
    SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &wstr);

    return utf8_encode(wstr);
}
