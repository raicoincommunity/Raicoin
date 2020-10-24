#include <rai/secure/plat.hpp>

#include <mach-o/dyld.h>
#include <limits.h>
#include <boost/filesystem.hpp>

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