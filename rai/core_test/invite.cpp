#include <unordered_set>
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <rai/common/util.hpp>
#include <rai/common/numbers.hpp>

TEST(invite, BETA)
{
    try
    {
        boost::filesystem::path file_path = "../invite/beta.json";
        ASSERT_TRUE(boost::filesystem::exists(file_path));

        std::fstream fs;
        fs.open(file_path.string(), std::ios_base::in);
        ASSERT_FALSE(fs.fail());

        rai::Ptree ptree;
        boost::property_tree::read_json(fs, ptree);

        std::unordered_set<rai::Account> reps;
        rai::Ptree reps_ptree = ptree.get_child("representatives");
        for (const auto& i : reps_ptree)
        {
            rai::Account rep;
            auto account = i.second.get<std::string>("account");
            bool error   = rep.DecodeAccount(account);
            if (error)
            {
                std::cout << "Invalid account field:" << account << std::endl;
                ASSERT_FALSE(true);
            }

            if (reps.find(rep) != reps.end())
            {
                std::cout << "Duplicate account field :" << account
                          << std::endl;
                ASSERT_FALSE(true);
            }

            reps.insert(rep);
        }
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
        ASSERT_TRUE(false);
    }
}

TEST(invite, LIVE)
{
    try
    {
        boost::filesystem::path file_path = "../invite/live.json";
        ASSERT_TRUE(boost::filesystem::exists(file_path));

        std::fstream fs;
        fs.open(file_path.string(), std::ios_base::in);
        ASSERT_FALSE(fs.fail());

        rai::Ptree ptree;
        boost::property_tree::read_json(fs, ptree);

        std::unordered_set<rai::Account> reps;
        rai::Ptree reps_ptree = ptree.get_child("representatives");
        for (const auto& i : reps_ptree)
        {
            rai::Account rep;
            auto account = i.second.get<std::string>("account");
            bool error   = rep.DecodeAccount(account);
            if (error)
            {
                std::cout << "Invalid account field:" << account << std::endl;
                ASSERT_FALSE(true);
            }

            if (reps.find(rep) != reps.end())
            {
                std::cout << "Duplicate account field :" << account
                          << std::endl;
                ASSERT_FALSE(true);
            }

            reps.insert(rep);
        }
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
        ASSERT_TRUE(false);
    }
}