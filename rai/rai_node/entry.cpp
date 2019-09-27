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

#include <chrono>
#include <unordered_set>

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

int main(int argc, char* const* argv)
{
    std::string str = R"%%%({
    "type": "representative",
    "opcode": "receive",
    "credit": "1",
    "counter": "1",
    "timestamp": "1564905000",
    "height": "0",
    "account": "rai_1re6kae6c995p5mj7w99agqidqxybrik77q9jpb74zncxxx7647qbfwjazz3",
    "previous": "0000000000000000000000000000000000000000000000000000000000000000",
    "balance": "999999000000000",
    "link": "6999BC557825BC751FD7EF80FBF88766AFEEE3300623EC8C2A32863BB9879A5D",
    "signature": "6AF3097F33B3F5EF07590654DBC379186234CA59E70FE9135BB26DFFC87D3C78C86D96C6534EA9D923EE8248FCF1306790A732FC460B4A3760BECB4268A7F60C"
})%%%";
    rai::Ptree ptree;
    rai::ErrorCode error_code;
    std::stringstream stream = std::stringstream(str);
    boost::property_tree::read_json(stream, ptree);
    rai::RepBlock block(error_code, ptree);

    std::cout << block.Json() << std::endl;
    std::cout << block.Hash().StringHex() << std::endl;
    
    std::cin.get();
    return 0;
}
#endif


