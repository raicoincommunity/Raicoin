#include <rai/secure/plat.hpp>

#include <mach-o/dyld.h>
#include <limits.h>
#include <boost/filesystem.hpp>
#include <pwd.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

boost::filesystem::path rai::AppPath()
{
    auto entry(getpwuid(getuid()));
    boost::filesystem::path result(entry->pw_dir);
    return result;
}

void rai::SetStdinEcho(bool enable)
{
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if (enable)
    {
        tty.c_lflag |= ECHO;
    }
    else
    {
        tty.c_lflag &= ~ECHO;
    }
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

std::string rai::PemPath()
{
  std::string result = "cacert.pem";
  char buf[PATH_MAX];
  uint32_t size = PATH_MAX;
  if(_NSGetExecutablePath(buf, &size) != 0)
  {
    return result;
  }
  std::string str(buf, size);
  boost::filesystem::path path(str);
  path = path.parent_path() / result;
  result = path.string();
  return result;
}