#pragma once

#include <boost/filesystem.hpp>

namespace rai
{
boost::filesystem::path AppPath();
void SetStdinEcho(bool);
std::string PemPath();
}