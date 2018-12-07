#include "./Network.hpp"
#include <math.h>

ACTION Network::setenable(bool enable) {
    require_auth( _self );
    /* TODO - add more assertions here. */

    state_type state_table_inst(_self, _self.value);
    state_t s;
    s.is_enabled = enable;
    s.manager = _self;
    state_table_inst.set(s, _self);
}

ACTION Network::addreserve(name reserve, bool add) {
    require_auth( _self );
    /* TODO - add more assertions here. */

    reserves_type reserves_table_inst(_self, _self.value);
    auto itr = reserves_table_inst.find(reserve.value);
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

ACTION Network::listpairres(name reserve,
                          asset token,
                          name token_contract,
                          bool eos_to_token, /* unused */
                          bool token_to_eos, /* unused */
                          bool add
) {
    require_auth( _self );
    /* TODO - add more assertions here. */
    /* TODO - actually use eos_to_token and token_to_eos */

    reserves_type reserves_table_inst(_self, _self.value);
    auto reserve_exists = (reserves_table_inst.find(reserve.value) != reserves_table_inst.end());
    eosio_assert(reserve_exists, "reserve does not exist");

    reservespert_type reservespert_table_inst(_self, _self.value);
    auto itr = reservespert_table_inst.find(token.symbol.raw());
    auto token_exists = (itr != reservespert_table_inst.end());

    if (add) {
        if (!token_exists) {
            reservespert_table_inst.emplace(_self, [&]( auto& s ) {
               s.symbol = token.symbol.raw();
               s.token_contract = token_contract.value;
               s.reserve_contracts = std::vector<name>(MAX_RESERVES_PER_TOKEN, name());
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
                s.reserve_contracts[s.num_reserves-1] = name();
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

void Network::transfer(name from, name to, asset quantity, string memo) {

    if (to != _self) return; /* TODO: is this ok? */

    state_type state_table_inst(_self, _self.value);
    eosio_assert(state_table_inst.exists(), "trade not enabled");
    eosio_assert(memo.length() > 0, "needs a memo with transaction details");
    eosio_assert(quantity.is_valid(), "invalid transfer");

    auto memo_struct = parse_memo(memo);
    /*
     TODO: should we have such verification? how?
     eosio_assert(code == memo_struct.src_contract, "src token contract must match memo.");
     */
    eosio_assert(quantity.symbol == memo_struct.src.symbol,
                 "src token symbol must match memo.");

    reservespert_type reservespert_table_inst(_self, _self.value);
    auto itr = (quantity.symbol != EOS_SYMBOL) ?
            reservespert_table_inst.find(quantity.symbol.raw()) :
            reservespert_table_inst.find(memo_struct.dest.symbol.raw());
    eosio_assert( itr != reservespert_table_inst.end(), "unlisted token" );

    /* fill memo struct with amount to avoid passing the transfer object afterwards. */
    memo_struct.src.amount = quantity.amount;

    /* get rates from all reserves that hold the pair */
    auto reservespert_entry = (memo_struct.src.symbol != EOS_SYMBOL) ?
            reservespert_table_inst.get(memo_struct.src.symbol.raw()) :
            reservespert_table_inst.get(memo_struct.dest.symbol.raw());

    for (int i = 0; i < reservespert_entry.num_reserves; i++) {
        auto reserve = reservespert_entry.reserve_contracts[i];
        action {
            permission_level{_self, "active"_n},
            reserve,
            "getconvrate"_n,
            std::make_tuple(memo_struct.src)
        }.send();
    }

    SEND_INLINE_ACTION(*this, trade1, {_self, "active"_n}, {memo_struct});
}

ACTION Network::trade1(memo_trade_structure memo_struct) {

    /* TODO: is this correct? */
    eosio_assert( _code == _self, "current action can only be called internally" );

    /* read stored rates from all reserves that hold the pair */
    reservespert_type reservespert_table_inst(_self, _self.value);
    auto reservespert_entry = (memo_struct.src.symbol != EOS_SYMBOL) ?
            reservespert_table_inst.get(memo_struct.src.symbol.raw()) :
            reservespert_table_inst.get(memo_struct.dest.symbol.raw());

    double best_rate = 0;
    int best_reserve_index = 0;
    for (int i = 0; i < reservespert_entry.num_reserves; i++) {
        auto reserve = reservespert_entry.reserve_contracts[i];
        auto rate_entry = rate_type(reserve, reserve.value).get();

        if(rate_entry.stored_rate > best_rate) {
            best_reserve_index = i;
        }
    }
    auto best_reserve = reservespert_entry.reserve_contracts[best_reserve_index];
    auto best_rate_entry = rate_type(best_reserve, best_reserve.value).get();
    auto stored_rate = best_rate_entry.stored_rate;
    auto rate_result_dest_amount = best_rate_entry.dest_amount;

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
                       {_self, "active"_n},
                       {best_reserve, memo_struct, actual_src, actual_dest});
}

ACTION Network::trade2(name reserve,
                     memo_trade_structure memo_struct,
                     asset actual_src,
                     asset actual_dest) {

    /* TODO: is this correct? */
    eosio_assert( _code == _self, "current action can only be called internally" );

    /* store dest balance to help verify later that dest amount was received. */
    asset dest_before_trade = get_balance(memo_struct.dest_account,
                                          memo_struct.dest_contract,
                                          memo_struct.dest.symbol);

    /* since no suitable method to turn double into string we do not pass
     * conversion rate to reserve. Instead we assume that it's already stored there. */

    /* do reserve trade */
    action {
        permission_level{_self, "active"_n},
        memo_struct.src_contract,
        "transfer"_n,
        std::make_tuple(_self,
                        reserve,
                        actual_src,
                        (name{memo_struct.dest_account}).to_string())
    }.send();

    SEND_INLINE_ACTION(
        *this,
        trade3,
        {_self, "active"_n},
        {reserve, memo_struct, actual_src, actual_dest, dest_before_trade}
    );
}

ACTION Network::trade3(name reserve,
                     memo_trade_structure memo_struct,
                     asset actual_src,
                     asset actual_dest,
                     asset dest_before_trade) {

    /* TODO: is this correct? */
    eosio_assert( _code == _self, "current action can only be called internally" );

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

int Network::find_reserve(std::vector<name> reserve_list,
                          uint8_t num_reserves,
                          name reserve) {
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
    res.trader = name(parts[0].c_str());
    res.src_contract = name(parts[1].c_str());

    auto sym_parts = split(parts[2], " ");
    res.src = asset(0, symbol(sym_parts[1].c_str(), stoi(sym_parts[0].c_str())));

    res.dest_contract = name(parts[3].c_str());
    sym_parts = split(parts[4], " ");
    res.dest = asset(0, symbol(sym_parts[1].c_str(), stoi(sym_parts[0].c_str())));

    res.dest_account = name(parts[5].c_str());
    res.max_dest_amount = stoi(parts[6].c_str()); /* TODO: should we use std:stoul to turn to unsignd ints? */
    res.min_conversion_rate = stof(parts[7].c_str()); /* TODO: is it ok to use stdof to parse double? */
    res.walletId = name(parts[8].c_str());
    res.hint = parts[7];

    return res;
}

extern "C" {
    [[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {

        if (action == "transfer"_n.value && code != receiver) {
            eosio::execute_action( eosio::name(receiver), eosio::name(code), &Network::transfer );
        }
        if (code == receiver){
            switch( action ) {
                EOSIO_DISPATCH_HELPER( Network, (setenable)(addreserve)(listpairres)
                                                (trade1)(trade2)(trade3))
            }
        }
        eosio_exit(0);
    }
}
