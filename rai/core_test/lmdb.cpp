#include <chrono>
#include <iostream>
#include <string>
#include <gtest/gtest.h>
#include <lmdb/libraries/liblmdb/lmdb.h>
#include <rai/core_test/config.hpp>
#include <rai/core_test/test_util.hpp>

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::cout;
using std::endl;
using std::string;

TEST(lmdb, put_get_del)
{
    int ret;
    MDB_env *env;
    MDB_dbi dbi;
    MDB_val val_key;
    MDB_val val_data;
    MDB_txn *txn;
    MDB_stat stat;
    string file = "./data.ldb";
    string db = "raicoin";
    unsigned char key[256] = "raicoin";
    unsigned char data[256] = "raicoin";
    unsigned char query[256] = "";
    unsigned char key2[511] = "raicoin";
    unsigned char data2[511] = "raicoin";
    unsigned char query2[511] = "";

    ret = mdb_env_create(&env);
    ASSERT_EQ(0, ret);
    ret = mdb_env_set_maxdbs(env, 128);
    ASSERT_EQ(0, ret);
    ret = mdb_env_open(env, file.c_str(), MDB_NOSUBDIR|MDB_NOTLS, 0600);
    ASSERT_EQ(0, ret);
    ret = mdb_txn_begin(env, nullptr, 0, &txn);
    ASSERT_EQ(0, ret);
    ret = mdb_dbi_open(txn, db.c_str(), MDB_CREATE, &dbi);
    ASSERT_EQ(0, ret);

    val_key.mv_size = sizeof(key);
    val_key.mv_data = key;
    val_data.mv_size = sizeof(data);
    val_data.mv_data = data;
    ret = mdb_put(txn, dbi, &val_key, &val_data, 0);
    ASSERT_EQ(0, ret);

    val_key.mv_size = sizeof(key2);
    val_key.mv_data = key2;
    val_data.mv_size = sizeof(data2);
    val_data.mv_data = data2;
    ret = mdb_put(txn, dbi, &val_key, &val_data, 0);
    ASSERT_EQ(0, ret);

    ret = mdb_txn_commit(txn);
    ASSERT_EQ(0, ret);

    ret = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
    ASSERT_EQ(0, ret);

    val_key.mv_size = sizeof(key);
    val_key.mv_data = key;
    ret = mdb_get(txn, dbi, &val_key, &val_data);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(sizeof(data), val_data.mv_size);

    unsigned char *mv_data = reinterpret_cast<unsigned char *>(val_data.mv_data);
    std::copy(mv_data, mv_data + sizeof(query), query);
    std::vector<unsigned char> v1(data, data + sizeof(data));
    std::vector<unsigned char> v2(query, query + sizeof(query));
    ASSERT_EQ(v1, v2);

    val_key.mv_size = sizeof(key2);
    val_key.mv_data = key2;
    ret = mdb_get(txn, dbi, &val_key, &val_data);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(sizeof(data2), val_data.mv_size);

    unsigned char *mv_data2 = reinterpret_cast<unsigned char *>(val_data.mv_data);
    std::copy(mv_data2, mv_data2 + sizeof(query2), query2);
    std::vector<unsigned char> v3(data2, data2 + sizeof(data2));
    std::vector<unsigned char> v4(query2, query2 + sizeof(query2));
    ASSERT_EQ(v3, v4);

    ret = mdb_stat(txn, dbi, &stat);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(2, stat.ms_entries);

    ret = mdb_txn_commit(txn);
    ASSERT_EQ(0, ret);

    ret = mdb_txn_begin(env, nullptr, 0, &txn);
    ASSERT_EQ(0, ret);

    ret = mdb_del(txn, dbi, &val_key, nullptr);
    ASSERT_EQ(0, ret);

    ret = mdb_stat(txn, dbi, &stat);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(1, stat.ms_entries);

    val_key.mv_size = sizeof(key);
    val_key.mv_data = key;
    ret = mdb_del(txn, dbi, &val_key, nullptr);
    ASSERT_EQ(0, ret);

    ret = mdb_stat(txn, dbi, &stat);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(0, stat.ms_entries);

    ret = mdb_drop(txn, dbi, 1);
    ASSERT_EQ(0, ret);

    ret = mdb_txn_commit(txn);
    ASSERT_EQ(0, ret);

    mdb_env_close(env);
}

#if EXECUTE_LONG_TIME_CASE
TEST(lmdb, perfmance)
{
    int ret;
    MDB_env *env;
    MDB_dbi dbi;
    MDB_val val_key;
    MDB_val val_data;
    MDB_txn *txn;
    MDB_cursor *cursor;
    MDB_stat stat;
    string file = "./data.ldb";
    string db = "raicoin";
    long long int num = 10000;
    unsigned char key[256] = "";
    unsigned char data[256] = "";
    unsigned char query[256] = "";

    ret = mdb_env_create(&env);
    ASSERT_EQ(0, ret);
    ret = mdb_env_set_maxdbs(env, 128);
    ASSERT_EQ(0, ret);
    ret = mdb_env_set_mapsize (env, 1ULL * 1024 * 1024 * 1024 * 128);
    ASSERT_EQ(0, ret);
    ret = mdb_env_open(env, file.c_str(), MDB_NOSUBDIR|MDB_NOTLS, 0600);
    ASSERT_EQ(0, ret);
    ret = mdb_txn_begin(env, nullptr, 0, &txn);
    ASSERT_EQ(0, ret);
    ret = mdb_dbi_open(txn, db.c_str(), MDB_CREATE, &dbi);
    ASSERT_EQ(0, ret);
    ret = mdb_txn_commit(txn);
    ASSERT_EQ(0, ret);

    val_key.mv_size = sizeof(key);
    val_key.mv_data = key;
    val_data.mv_size = sizeof(data);
    val_data.mv_data = data;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    for (uint32_t i = 0; i < num; ++i)
    {
        ret = mdb_txn_begin(env, nullptr, 0, &txn);
        ASSERT_EQ(0, ret);

        unsigned char *pi = reinterpret_cast<unsigned char *>(&i);
        std::copy(pi, pi + sizeof(i), key);
        ret = mdb_put(txn, dbi, &val_key, &val_data, 0);
        ASSERT_EQ(0, ret);

        ret = mdb_txn_commit(txn);
        ASSERT_EQ(0, ret);
    }
    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(t2 - t1).count();
    auto put_per_second = num * 1000 / duration;
    cout << put_per_second << " put/second." << endl;

    t1 = high_resolution_clock::now();
    for (uint32_t i = 0; i < num; ++i)
    {
        ret = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
        ASSERT_EQ(0, ret);

        unsigned char *pi = reinterpret_cast<unsigned char *>(&i);
        std::copy(pi, pi + sizeof(i), key);
        ret = mdb_get(txn, dbi, &val_key, &val_data);
        ASSERT_EQ(0, ret);

        ret = mdb_txn_commit(txn);
        ASSERT_EQ(0, ret);
    }
    t2 = high_resolution_clock::now();

    duration = duration_cast<milliseconds>(t2 - t1).count();
    auto get_per_second = num * 1000 / duration;
    cout << get_per_second << " get/second." << endl;

    t1 = high_resolution_clock::now();
    for (uint32_t i = 0; i < num; ++i)
    {
        ret = mdb_txn_begin(env, nullptr, 0, &txn);
        ASSERT_EQ(0, ret);

        unsigned char *pi = reinterpret_cast<unsigned char *>(&i);
        std::copy(pi, pi + sizeof(i), key);
        ret = mdb_del(txn, dbi, &val_key, nullptr);
        ASSERT_EQ(0, ret);

        ret = mdb_txn_commit(txn);
        ASSERT_EQ(0, ret);
    }
    t2 = high_resolution_clock::now();

    duration = duration_cast<milliseconds>(t2 - t1).count();
    auto del_per_second = num * 1000 / duration;
    cout << del_per_second << " del/second." << endl;
    duration = duration_cast<milliseconds>(t2 - t1).count();
}
#endif