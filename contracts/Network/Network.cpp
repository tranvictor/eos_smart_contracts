#include "./Network.hpp"
#include <math.h>

void Network::setenable(bool enable) {
    require_auth( _self );
    /* TODO - add more assertions here. */

    auto itr = state_table_inst.find( _self );
    if(itr != state_table_inst.end()) {
        state_table_inst.modify(itr, _self, [&]( auto& s ) {
            s.is_enabled = enable;
            s.manager = _self;
        });
    } else {
        state_table_inst.emplace( _self, [&]( auto& s ) {
            s.is_enabled = enable;
            s.manager = _self;
        });
    }
}

void Network::addreserve(account_name reserve, bool add) {
    require_auth( _self );
    /* TODO - add more assertions here. */

    auto itr = reserves_table_inst.find(reserve);
    bool exists = (itr != reserves_table_inst.end());
    eosio_assert(add != exists, "can only add a non existing reserve or delete an existing one");
    if (add) {
        reserves_table_inst.emplace(_self, [&]( auto& s ) {
            s.contract = reserve;
        });
    } else {
        reserves_table_inst.erase(itr);
    }
}

void Network::listpairres(account_name reserve,
                          asset token,
                          account_name token_contract,
                          bool eos_to_token, /* unused */
                          bool token_to_eos, /* unused */
                          bool add
) {
    require_auth( _self );
    /* TODO - add more assertions here. */
    /* TODO - actually use eos_to_token and token_to_eos */

    auto reserve_exists = (reserves_table_inst.find(reserve) != reserves_table_inst.end());
    eosio_assert(reserve_exists, "reserve does not exist");

    auto itr = reservespert_table_inst.find(token.symbol.value);
    auto token_exists = (itr != reservespert_table_inst.end());

    if (add) {
        if (!token_exists) {
            reservespert_table_inst.emplace(_self, [&]( auto& s ) {
               s.symbol = token.symbol.value;
               s.token_contract = token_contract;
               s.reserve_contracts = std::vector<account_name>(MAX_RESERVES_PER_TOKEN, 0);
               s.reserve_contracts[0] = reserve;
               s.num_reserves = 1;
            });
        } else {
            reservespert_table_inst.modify(itr, _self, [&]( auto& s ) {
                if(find_reserve(s.reserve_contracts, s.num_reserves, reserve) == -1) {
                    /* reserve does not exist */
                    eosio_assert(s.num_reserves < MAX_RESERVES_PER_TOKEN,
                                 "reached number of reserves limit for this token ");
                    s.reserve_contracts[s.num_reserves] = reserve;
                    s.num_reserves++;
                }
            });
        }
    } else if (token_exists) {
        bool erase = false;
        reservespert_table_inst.modify(itr, _self, [&]( auto& s ) {
            int reserve_index = find_reserve(s.reserve_contracts, s.num_reserves, reserve);
            if(reserve_index != -1) {
                /* reserve exist */
                s.reserve_contracts[reserve_index] = s.reserve_contracts[s.num_reserves-1];
                s.reserve_contracts[s.num_reserves-1] = 0;
                s.num_reserves--;
            }
            if(s.num_reserves == 0) {
                erase = true;
            }
        });
        if (erase) {
            reservespert_table_inst.erase(itr);
        }
    }
}

void Network::trade0(const struct transfer &transfer, const account_name code) {

    if (transfer.to != _self) return; /* TODO: is this ok? */

    eosio_assert(state_table_inst.find(_self) != state_table_inst.end(), "trade not enabled");
    eosio_assert(state_table_inst.get(_self).is_enabled, "trade not enabled");
    eosio_assert(transfer.memo.length() > 0, "needs a memo with transaction details");
    eosio_assert(transfer.quantity.is_valid(), "invalid transfer");

    auto memo_struct = parse_memo(transfer.memo);
    eosio_assert(code == memo_struct.src_contract, "src token contract must match memo.");
    eosio_assert(transfer.quantity.symbol.value == memo_struct.src.symbol.value,
                 "src token symbol must match memo.");

    auto itr = (transfer.quantity.symbol.value != EOS_SYMBOL) ?
            reservespert_table_inst.find(transfer.quantity.symbol.value) :
            reservespert_table_inst.find(memo_struct.dest.symbol.value);
    eosio_assert( itr != reservespert_table_inst.end(), "unlisted token" );

    /* fill memo struct with amount to avoid passing the transfer object afterwards. */
    memo_struct.src.amount = transfer.quantity.amount;

    /* TODO - add reserve choosing logic. currently we use the first reserve listed for the token. */
    auto reserve = (memo_struct.src.symbol.value != EOS_SYMBOL) ?
            reservespert_table_inst.get(memo_struct.src.symbol.value).reserve_contracts[0] :
            reservespert_table_inst.get(memo_struct.dest.symbol.value).reserve_contracts[0];

    action {
        permission_level{_self, N(active)},
        reserve,
        N(getconvrate),
        std::make_tuple(memo_struct.src)
    }.send();

    SEND_INLINE_ACTION(*this, trade1, {_self, N(active)}, {reserve, memo_struct});
}

void Network::trade1(account_name reserve,
                     memo_trade_structure memo_struct
) {
    auto rate_entry = rate_type(reserve, reserve).get(reserve);
    auto stored_rate = rate_entry.stored_rate;
    auto rate_result_dest_amount = rate_entry.dest_amount;

    eosio_assert(stored_rate >= memo_struct.min_conversion_rate,
                 "recieved rate is smaller than min conversion rate.");

    asset actual_src;
    asset actual_dest;

    calc_actuals(memo_struct,
                 stored_rate,
                 rate_result_dest_amount,
                 actual_src,
                 actual_dest);

    if(actual_src < memo_struct.src) {
        /* if there is "change" send back to trader */
        auto change = memo_struct.src - actual_src;
        send(_self, memo_struct.trader, change, memo_struct.src_contract);
    }

    SEND_INLINE_ACTION(*this,
                       trade2,
                       {_self, N(active)},
                       {reserve, memo_struct, actual_src, actual_dest});
}

void Network::trade2(account_name reserve,
                     memo_trade_structure memo_struct,
                     asset actual_src,
                     asset actual_dest) {

    /* store dest balance to help verify later that dest amount was received. */
    asset dest_before_trade = get_balance(memo_struct.dest_account,
                                          memo_struct.dest_contract,
                                          memo_struct.dest.symbol);

    /* since no suitable method to turn double into string we do not pass
     * conversion rate to reserve. Instead we assume that it's already stored there. */

    /* do reserve trade */
    action {
        permission_level{_self, N(active)},
        memo_struct.src_contract,
        N(transfer),
        std::make_tuple(_self,
                        reserve,
                        actual_src,
                        (name{memo_struct.dest_account}).to_string())
    }.send();

    SEND_INLINE_ACTION(
        *this,
        trade3,
        {_self, N(active)},
        {reserve, memo_struct, actual_src, actual_dest, dest_before_trade}
    );
}

void Network::trade3(account_name reserve,
                     memo_trade_structure memo_struct,
                     asset actual_src,
                     asset actual_dest,
                     asset dest_before_trade) {

    /* verify dest balance was indeed added to dest account */
    auto dest_after_trade = get_balance(memo_struct.dest_account,
                                        memo_struct.dest_contract,
                                        memo_struct.dest.symbol);
    asset dest_difference = dest_after_trade - dest_before_trade;

    eosio_assert(dest_difference == actual_dest, "trade amount not added to dest");
}

void Network::calc_actuals(memo_trade_structure &memo_struct,
                           double rate_result,
                           uint64_t rate_result_dest_amount,
                           asset &actual_src,
                           asset &actual_dest) {
    uint64_t actual_dest_amount;
    uint64_t actual_src_amount;

    if (rate_result_dest_amount > memo_struct.max_dest_amount) {
        actual_dest_amount = memo_struct.max_dest_amount;
        actual_src_amount = calc_src_amount(rate_result,
                                            memo_struct.src.symbol.precision(),
                                            actual_dest_amount,
                                            memo_struct.dest.symbol.precision());
        eosio_assert(actual_src_amount <= memo_struct.src.amount,
                     "actual src amount can not be bigger than memo src amount");
    } else {
        actual_dest_amount = rate_result_dest_amount;
        actual_src_amount = memo_struct.src.amount;
    }

    actual_src.amount = actual_src_amount;
    actual_src.symbol = memo_struct.src.symbol;
    actual_dest.amount = actual_dest_amount;
    actual_dest.symbol = memo_struct.dest.symbol;
}

int Network::find_reserve(std::vector<account_name> reserve_list,
                          uint8_t num_reserves,
                          account_name reserve) {
    for(int index = 0; index < num_reserves; index++) {
        if (reserve_list[index] == reserve) {
            return index;
        }
    }
    return NOT_FOUND;
}

memo_trade_structure Network::parse_memo(std::string memo) {
    auto res = memo_trade_structure();
    auto parts = split(memo, ",");

    res.trader = string_to_name(parts[0].c_str());

    res.src_contract = string_to_name(parts[1].c_str());
    auto sym_parts = split(parts[2], " ");
    res.src = asset(0, string_to_symbol(stoi(sym_parts[0].c_str()), sym_parts[1].c_str()));

    res.dest_contract = string_to_name(parts[3].c_str());
    sym_parts = split(parts[4], " ");
    res.dest = asset(0, string_to_symbol(stoi(sym_parts[0].c_str()),sym_parts[1].c_str()));

    res.dest_account = string_to_name(parts[5].c_str());
    res.max_dest_amount = stoi(parts[6].c_str()); /* TODO: should we use std:stoul to turn to unsignd ints? */
    res.min_conversion_rate = stof(parts[7].c_str()); /* TODO: is it ok to use stdof to parse double? */
    res.walletId = string_to_name(parts[8].c_str());
    res.hint = parts[7];

    return res;
}

void Network::apply(const account_name code, const account_name act) {

    if (act == N(transfer)) {
        trade0(unpack_action_data<struct transfer>(), code);
        return;
    }

    if (act == N(trade1) ||
        act == N(trade2) ||
        act == N(trade3)) {
        eosio_assert( code == _self, "current antion can only be called internally" );
    }

    auto &thiscontract = *this;

    switch (act) {
        EOSIO_API( Network, (setenable)(addreserve)(listpairres)(trade1)(trade2)(trade3))
    };
}

extern "C" {

    using namespace eosio;

    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        auto self = receiver;
        Network contract(self);
        contract.apply(code, action);
        eosio_exit(0);
    }
}

