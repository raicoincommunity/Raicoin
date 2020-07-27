#include <iostream>
#include <boost/program_options.hpp>

#include <rai/rai_node/cli.hpp>

#if 1
int main(int argc, char* const* argv)
{
    boost::program_options::options_description desc("Command line options");
    desc.add_options()("help", "Print out options");
    rai::CliAddOptions(desc);

    boost::program_options::variables_map vm;
    try
    {
        boost::program_options::store(
            boost::program_options::parse_command_line(argc, argv, desc), vm);
    }
    catch (const boost::program_options::error& err)
    {
        std::cerr << err.what() << std::endl;
        return 1;
    }
    boost::program_options::notify(vm);


    rai::ErrorCode error_code = rai::CliProcessOptions(vm);
    if (vm.count("help") || (rai::ErrorCode::UNKNOWN_COMMAND == error_code))
    {
        std::cout << desc << std::endl;
        return 0;
    }

    if (rai::ErrorCode::SUCCESS != error_code)
    {
        std::cerr << rai::ErrorString(error_code) << ": "
                  << static_cast<int>(error_code) << std::endl;
        return 1;
    }

    return 0;
}
#else
#include <iostream>
#include <rai/node/node.hpp>

int main(int argc, char* const* argv)
{
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(rai::LIVE_GENESIS_BLOCK);
    boost::property_tree::read_json(stream, ptree);
    rai::TxBlock block(error_code, ptree);
    std::cout << "error_code:" << rai::ErrorString(error_code) << std::endl;

    std::cout << block.Json() << std::endl;
    std::cout << block.Hash().StringHex() << std::endl;

    std::cin.get();
    return 0;
}
#endif


