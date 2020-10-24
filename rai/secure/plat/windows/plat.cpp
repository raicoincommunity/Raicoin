#include <rai/secure/plat.hpp>

#include <shlobj.h>
#include <windows.h>

boost::filesystem::path rai::AppPath()
{
    boost::filesystem::path result;
    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path)))
    {
        result = boost::filesystem::path(path);
    }
    return result;
}

void rai::SetStdinEcho(bool enable)
{
    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(handle, &mode);

    if (enable)
    {
        mode |= ENABLE_ECHO_INPUT;
    }
    else
    {
        mode &= ~ENABLE_ECHO_INPUT;
    }

    SetConsoleMode(handle, mode);
}

std::string rai::PemPath()
{
  std::string result = "cacert.pem";
  return result;
}