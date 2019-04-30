#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <fc/variant_object.hpp>
#include <Runtime/Runtime.h>
#include "contracts.hpp"

using namespace eosio::testing;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace eosio;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;

class cryptobook_tester : public tester
{
public:
    cryptobook_tester()
    {
        produce_blocks(2);

        create_accounts({N(alice), N(bob), N(carol), N(cryptobook)});
        produce_blocks(2);

        set_code(N(cryptobook), contracts::token_wasm());
        set_abi(N(cryptobook), contracts::token_abi().data());

        produce_blocks();

        const auto &accnt = control->db().get<account_object, by_name>(N(cryptobook));
        abi_def abi;
        BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
        abi_ser.set_abi(abi, abi_serializer_max_time);
    }

    action_result push_action(const account_name &signer, const account_name &name, const variant_object &data)
    {
        string action_type_name = abi_ser.get_action_type(name);

        action act;
        act.account = N(cryptobook);
        act.name = name;
        act.data = abi_ser.variant_to_binary(action_type_name, data, abi_serializer_max_time);

        return base_tester::push_action(std::move(act), uint64_t(signer));
    }

    fc::variant get_stats(const string &symbolname)
    {
        auto symb = eosio::chain::symbol::from_string(symbolname);
        auto symbol_code = symb.to_symbol_code().value;
        vector<char> data = get_row_by_account(N(cryptobook), symbol_code, N(stat_t), symbol_code);

        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("current_stats", data, abi_serializer_max_time);

    }

    fc::variant get_account(account_name acc, const string &symbolname)
    {
        auto symb = eosio::chain::symbol::from_string(symbolname);
        auto symbol_code = symb.to_symbol_code().value;
        vector<char> data = get_row_by_account(N(cryptobook), acc, N(accounts_t), symbol_code);

        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("account", data, abi_serializer_max_time);
    }

    fc::variant get_stake(account_name acc)
    {
        vector<char> data = get_row_by_account(N(cryptobook), acc, N(stakes_t), acc);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("stake", data, abi_serializer_max_time);
    }

    //define action result 

    action_result create(account_name issuer, asset maximum_supply)
    {
        return push_action(N(cryptobook), N(create), mvo()("issuer", issuer)("maximum_supply", maximum_supply));
    }

    action_result issue(account_name issuer, account_name to, asset quantity, string memo)
    {
        return push_action(issuer, N(issue), mvo()("to", to)("quantity", quantity)("memo", memo));
    }

    action_result transfer(account_name from,
                          account_name to,
                          asset quantity,
                          string memo)
    {
        return push_action(from, N(transfer), mvo("from", from)("to", to)("quantity", quantity)("memo", memo));
    }

    action_result stake(name owner, asset quantity)
    {
        return push_action(owner, N(satke), mvo("owner", owner)("quantity", quantity));
    }

    action_result unstake(name owner, asset quantity)
    {
        return push_action(owner, N(unstake), mvo("owner", owner)("quantity",quantity));
    }

    abi_serializer abi_ser;
};

//Write test case

BOOST_AUTO_TEST_SUITE(eosio_token_tests)

//1) Testing create a new tokens
BOOST_FIXTURE_TEST_CASE(create_tests, cryptobook_tester)
try
{
    auto token = create(N(cryptobook), asset::from_string("10000.0000 CRW"));
    auto stats = get_stats("4, CRW");
    REQUIRE_MATCHING_OBJECT(stats, mvo()
        ("suppply", "0.0000 CRW")
        ("max_suplly","10000.0000 CRW")
        ("issuer", "cryptobook")
    );
    produce_blocks(1);
} 
FC_LOG_AND_RETHROW()

//2) Test creating a token with a negative supply
BOOST_FIXTURE_TEST_CASE(create_negative_max_supply, cryptobook_tester)
try
{
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("max-supply must be possitive"), create(N(cryptobook), asset::from_string("-10000.00 CRW"))
    );
}
FC_LOG_AND_RETHROW()

//3) Test you cannot create two tokens with the same symbol
BOOST_FIXTURE_TEST_CASE(symbol_already_exist, cryptobook_tester)
try
{
    auto token = create(N(cryptobook), asset::from_string("100 CRW"));
    auto stats = get_stats("0, CRW");
    REQUIRE_EQUAL_OBJECTS(stats, mvo()("supply", "0 CRW")("max_supply", "100 CRW")("issuer", "cryptobook"));
    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("token with symbol already exits"), create(N(cryptobook), asset::from_string("100 CRW")));
}
FC_LOG_AND_RETHROW()

//4) Test max supply token
BOOST_FIXTURE_TEST_CASE(create_max_supply, cryptobook_tester)
try
{
    auto token = create(N(cryptobook), asset::from_string("4611686018427387903 CRW"));
    auto stats = get_stats("0, CRW");
    REQUIRE_EQUAL_OBJECTS(stats, mvo()("supply", "0 CRW")("max_supply", "4611686018427387903 CRW")("issuer", "cryptobook"));
    produce_blocks(1);

    asset max(10, symbol(SY(0, NKT)));
    share_type amount = 4611686018427387904;
    static_assert(sizeof(share_type) <= sizeof(asset), "asset changed so test is no longer valid");
    static_assert(std::is_trivially_copyable<asset>::value, "asset is not trivially copyable");
    memcpy(&max, &amount, sizeof(share_type)); // hack in an invalid amount

    BOOST_CHECK_EXCEPTION(create(N(horustokenio), max), asset_type_exception, [](const asset_type_exception &e) {
        return expect_assert_message(e, "magnitude of asset amount must be less than 2^62");
    });
}
FC_LOG_AND_RETHROW()

/*************************************************************************
* 5) Test creating token with max decimal places
**************************************************************************/
BOOST_FIXTURE_TEST_CASE(create_max_decimals, cryptobook_tester)
try
{

    auto token = create(N(horustokenio), asset::from_string("1.000000000000000000 CRW"));
    auto stats = get_stats("18,CRW");
    REQUIRE_MATCHING_OBJECT(stats, mvo()("supply", "0.000000000000000000 CRW")("max_supply", "1.000000000000000000 CRW")("issuer", "cryptobook"));
    produce_blocks(1);

    asset max(10, symbol(SY(0, NKT)));
    //1.0000000000000000000 => 0x8ac7230489e80000L
    share_type amount = 0x8ac7230489e80000L;
    static_assert(sizeof(share_type) <= sizeof(asset), "asset changed so test is no longer valid");
    static_assert(std::is_trivially_copyable<asset>::value, "asset is not trivially copyable");
    memcpy(&max, &amount, sizeof(share_type)); // hack in an invalid amount

    BOOST_CHECK_EXCEPTION(create(N(horustokenio), max), asset_type_exception, [](const asset_type_exception &e) {
        return expect_assert_message(e, "magnitude of asset amount must be less than 2^62");
    });
}
FC_LOG_AND_RETHROW()

/*************************************************************************
* 6) Test issuing token
**************************************************************************/
BOOST_FIXTURE_TEST_CASE(issue_tests, cryptobook_tester)
try
{

    auto token = create(N(cryptobook), asset::from_string("10000.0000 CRW"));
    produce_blocks(1);

    issue(N(cryptobook), N(alice), asset::from_string("500.0000 CRW"), "cryptobook.io");

    auto stats = get_stats("4,CRW");
    REQUIRE_MATCHING_OBJECT(stats, mvo()("supply", "500.0000 CRW")("max_supply", "10000.0000 CRW")("issuer", "cryptobook"));

    auto alice_balance = get_account(N(alice), "4,CRW");
    REQUIRE_MATCHING_OBJECT(alice_balance, mvo()("balance", "500.0000 CRW"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("quantity exceeds available supply"),
                        issue(N(horustokenio), N(alice), asset::from_string("500.0001 CRW"), "cryptobook.io"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("must issue positive quantity"),
                        issue(N(horustokenio), N(alice), asset::from_string("-1.0000 CRW"), "cryptobook.io"));

    BOOST_REQUIRE_EQUAL(success(),
                        issue(N(horustokenio), N(alice), asset::from_string("1.0000 CRW"), "cryptobook.io"));
}
FC_LOG_AND_RETHROW()

/*************************************************************************
* 7) Test basic token transfering
**************************************************************************/
BOOST_FIXTURE_TEST_CASE(transfer_tests, cryptobook_tester)
try
{
    auto token = create(N(cryptobook), asset::from_string("10000.0000 CRW"));
    produce_blocks(1);

    issue(N(cryptobook), N(alice), asset::from_string("500.0000 CRW"), "cryptobook.io");

    auto stats = get_stats("4,CRW");
    REQUIRE_MATCHING_OBJECT(stats, mvo()("supply", "500.0000 CRW")("max_supply", "10000.0000 CRW")("issuer", "cryptobook"));

    auto alice_balance = get_account(N(alice), "4,CRW");
    REQUIRE_MATCHING_OBJECT(alice_balance, mvo()("balance", "500.0000 CRW"));

    transfer(N(alice), N(bob), asset::from_string("300.00 CRW"), "cryptobook.io");

    alice_balance = get_account(N(alice), "4,CRW");
    REQUIRE_MATCHING_OBJECT(alice_balance, mvo()("balance", "200.0000 CRW"));

    auto bob_balance = get_account(N(bob), "4,CRW");
    REQUIRE_MATCHING_OBJECT(bob_balance, mvo()("balance", "300.0000 CRW"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("overdrawn balance"),
                        transfer(N(alice), N(bob), asset::from_string("201.0000 CRW"), "cryptobook.io"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("must transfer positive quantity"),
                        transfer(N(alice), N(bob), asset::from_string("-1000.0000 CRW"), "cryptobook.io"));
}
FC_LOG_AND_RETHROW()

// 8) Testing stake CRW tokens
BOOST_FIXTURE_TEST_CASE(stake_tests, cryptobook_tester)
try
{
    auto token = create(N(cryptobook), asset::from_string("100000.0000 CRW"));
    produce_blocks(1);

    issue(N(cryptobook), N(alice), asset::from_string("2000.00000 CRW"), "cryptobook.io");
    
    //stake must be positive amount
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("must stake a positive amount"),
                        stake(N(alice), asset::from_string("-200.000 CRW")));
    
    //stake over range of amount token user has
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("you cannot stake overdrawn balance"),
                        stake(N(alice), asset::from_string("2001.0000 CRW")));
    
    BOOST_REQUIRE_EQUAL(success(),
                        stake( N(alice), asset::from_string("1000.0000 CRW")));

    auto alice_stake = get_stake("alice");
    REQUIRE_MATCHING_OBJECT(alice_stake,mvo()("owner", "alice")("locked_balance","1000.000 CRW"));
}
FC_LOG_AND_RETHROW()

//9) Testing unstake CRW tokens
BOOST_FIXTURE_TEST_CASE(unstake_tests, cryptobook_tester)
try
{
    auto token = create(N(cryptobook), asset::from_string("100000.0000 CRW"));
    produce_blocks(1);

    issue(N(cryptobook), N(alice), asset::from_string("2000.00000 CRW"), "cryptobook.io");

    stake(N(alice), asset::from_string("1000.0000 CRW"));
    produce_blocks(1);

    //unstake must be positive amount
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("must unstake a positive amount"),
                        unstake(N(alice), asset::from_string("-2000.0000 CRW")));
    
    //Cannot unstake with amount larger than staked amount
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("you cannot unstake overdrawn balancee"),
                        unstake(N(alice), asset::from_string("1001.0000 CRW")));
    

    BOOST_REQUIRE_EQUAL(success(),
                        unstake( N(alice), asset::from_string("900.0000 CRW")));

    auto alice_stake = get_stake("alice");
    REQUIRE_MATCHING_OBJECT(alice_stake,mvo()("owner", "alice")("locked_balance","100.000 CRW"));

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(success(),
                        unstake( N(alice), asset::from_string("100.0000 CRW")));
}
FC_LOG_AND_RETHROW()
} //end unittest
