#pragma once

#include <boost/program_options.hpp>
#include <rai/common/errors.hpp>

namespace rai
{
void CliAddOptions(boost::program_options::options_description&);
rai::ErrorCode CliProcessOptions(const boost::program_options::variables_map&);
}  // namespace rai