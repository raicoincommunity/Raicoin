#pragma once
#include <string>

void TestShowHex(const unsigned char* text, size_t size);
bool TestDecodeHex(const std::string& in, unsigned char out[], size_t max_out);
bool TestDecodeHex(const std::string& in, std::vector<uint8_t>& out);

