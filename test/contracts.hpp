#pragma once
#include <eosio/testing/tester.hpp>

namespace eosio { namespace testing {

struct contracts {
   static std::vector<uint8_t> token_wasm() { return read_wasm("~/crypto_book/test/../contracts/cryptobook.wasm"); }
   static std::vector<char>    token_abi() { return read_abi("~/crypto_book/test/../contracts/cryptobook.abi"); }
};
}} //ns eosio::testing
