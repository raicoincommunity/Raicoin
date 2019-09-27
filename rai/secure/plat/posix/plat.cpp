#include <rai/secure/plat.hpp>

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
