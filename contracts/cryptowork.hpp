#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <string.h>

using namespace eosio;
using namespace std;

class [[eosio::contract("cryptobook")]] cryptobook: public eosio::contract {
    public:
        using contract::contract;
        cryptobook(name receiver, name code, datastream<const char*> ds):
        contract(receiver, code, ds){}

        [[eosio::action]]
        void create(name issuer, asset max_supply);
        [[eosio::action]]
        void issue(name to, asset quantity, string memo);
        [[eosio::action]]
        void transfer(name from, name to, asset quantity, string memo);
        [[eosio::action]]
        void stake(name owner, asset quantity);
        [[eosio::action]]
        void unstake(name owner, asset quantity);

    private:
        struct [[eosio::table]] account 
        {
            asset balance;
            uint64_t primary_key() const { return balance.symbol.code().raw(); }
            // EOSLIB_SERIALIZE(account, (balance))

        };
        struct [[eosio::table]] currency_stats
        {
            asset supply;
            asset max_supply;
            name issuer;
            uint64_t primary_key() const { return supply.symbol.code().raw(); }
            // EOSLIB_SERIALIZE(currency_stats, (supply)(max_supply)(issuer))

        };
        struct [[eosio::table]] stake_tokens
        {
            asset locked_balance;
            uint64_t primary_key() const { return locked_balance.symbol.code().raw(); }
            // EOSLIB_SERIALIZE(stake_tokens, (locked_balance))
        };

        typedef multi_index< "account"_n, account > accounts_t;
        typedef multi_index< "stat"_n, currency_stats > stats_t;
        typedef multi_index<"stake"_n, stake_tokens> stakes_t;
        
        void sub_balance(name owner, asset value);
        void add_balance(name owner, asset value, name ram_payer);

};
