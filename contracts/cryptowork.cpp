#include "cryptobook.hpp"

void cryptobook::create(name issuer, asset max_supply)
{
    require_auth(get_self());

    auto sym = max_supply.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(max_supply.is_valid(), "invalid supply");
    check(max_supply.amount > 0, "max_supply must be positive");

    stats_t stats_table(get_self(), sym.code().raw());
    auto existing = stats_table.find(sym.code().raw());
    check(existing == stats_table.end(), "token with symbol already exists");

    stats_table.emplace(get_self(), [&](auto &s) {
        s.supply.symbol = max_supply.symbol;
        s.max_supply    = max_supply;
        s.issuer        = issuer;
    });
}
void cryptobook::issue(name to, asset quantity, string memo)
{
    auto sym = quantity.symbol;
    check(sym.is_valid(), "invalid symbok name");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    stats_t stats_table(get_self(), sym.code().raw());
    auto existing = stats_table.find(sym.code().raw());
    check(existing != stats_table.end(), "token with symbol does not exist, create token before issue");
    const auto &st = *existing;

    require_auth(st.issuer);
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must issue positive quantity");

    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    check(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    stats_table.modify(st, same_payer, [&](auto &s) {
        s.supply += quantity;
    });

    add_balance(st.issuer, quantity, st.issuer);

    if(to != st.issuer)
    {
        SEND_INLINE_ACTION(*this, transfer, {{st.issuer, "active"_n}},
                           {st.issuer, to, quantity, memo});
    }
}
void cryptobook::transfer(name from, name to, asset quantity, string memo)
{
    check(from != to, "cannot transfer to self");
    require_auth(from);
    check(is_account(to), "to account does not exist");
    
    auto sym = quantity.symbol.code();
    stats_t stats_table(get_self(), sym.raw());
    const auto &st = stats_table.get(sym.raw());

    require_recipient(from);
    require_recipient(to);

    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must transfer positive quantity");
    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    auto payer = has_auth(to) ? to: from;

    sub_balance(from, quantity);
    add_balance(to, quantity, payer);
}
void cryptobook::stake(name owner, asset quantity)
{
    require_auth(owner);
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must stake positive quantity");

    accounts_t from_acnts(get_self(), owner.value);
    const auto &from = from_acnts.get(quantity.symbol.code().raw(), "no balance object found");

    stakes_t stake_table(get_self(), owner.value);
    const auto &stake_from = stake_table.get(quantity.symbol.code().raw());

    check(from.balance >= (stake_from.locked_balance + quantity), "overdrawn balance for stake");

    stake_table.modify(stake_from, owner, [&](auto &a) {
        a.locked_balance += quantity;
    });

    // sub_balance(owner, quantity);
}
void cryptobook::unstake(name owner, asset quantity)
{
    require_auth(owner);
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must unstake positive quantity");

    stakes_t stake_table(get_self(), owner.value);
    const auto &stake_from = stake_table.get(quantity.symbol.code().raw(), "no balance object found");
    check(stake_from.locked_balance >= quantity, "overdrawn locked balance");

    if(stake_from.locked_balance == quantity)
    {
        stake_table.erase(stake_from);
    }
    else
    {
        stake_table.modify(stake_from, owner, [&](auto &a) {
            a.locked_balance -= quantity;
        });
    }

    // add_balance(owner, quantity, owner);

}
void cryptobook::sub_balance(name owner, asset value)
{
    accounts_t from_acnts(get_self(), owner.value);
    const auto &from = from_acnts.get(value.symbol.code().raw(), "no blance object found");
    check(from.balance.amount >= value.amount, "overdrawn balance");

    from_acnts.modify(from, owner, [&](auto &a) {
        a.balance -= value;
    });
}
void cryptobook::add_balance(name owner, asset value, name ram_payer)
{
    accounts_t to_acnts(get_self(), owner.value);
    auto to = to_acnts.find(value.symbol.code().raw());
    if(to == to_acnts.end())
    {
        to_acnts.emplace(ram_payer, [&](auto &a) {
            a.balance = value;
        });
    }
    else
    {
        to_acnts.modify(to, same_payer, [&](auto &a) {
            a.balance += value;
        });
    }
}

EOSIO_DISPATCH(cryptobook::cryptobook, (create)(issue)(transfer)(stake)(unstake))