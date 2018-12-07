#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include "../Common/common.hpp"

using namespace eosio;

#define MAX_RESERVES_PER_TOKEN  5
#define NOT_FOUND              -1

struct memo_trade_structure {
    name        trader;
    name        src_contract; /* TODO: potentially read from storage since pair is listed. */
    asset       src;
    name        dest_contract; /* TODO: potentially read from storage since pair is listed. */
    asset       dest;
    name        dest_account;
    uint64_t    max_dest_amount;
    double      min_conversion_rate;
    name        walletId;
    string      hint; /* TODO: should hint be another type? */
};

CONTRACT Network : public contract {

    public:
        using contract::contract;

        TABLE state_t {
            bool        is_enabled;
            EOSLIB_SERIALIZE(state_t, (is_enabled))
        };

        TABLE reserve_t {
            name        contract;
            uint64_t    primary_key() const { return contract.value; }
            EOSLIB_SERIALIZE(reserve_t, (contract))
        };

        /* TODO: change to 2 lists of per_token_src and per_token_dest */
        TABLE reservespert_t {
            symbol          symbol;
            name            token_contract;
            vector<name>    reserve_contracts;
            uint8_t         num_reserves;
            uint64_t        primary_key() const { return symbol.raw(); }
            EOSLIB_SERIALIZE(reservespert_t, (symbol)(token_contract)(reserve_contracts)(num_reserves))
        };

        typedef eosio::singleton<"state"_n, state_t> state_type;
        typedef eosio::multi_index<"state"_n, state_t> dummy_state_for_abi; /* hack until abi generator generates correct name */
        typedef eosio::multi_index<"reserve"_n, reserve_t> reserves_type;
        typedef eosio::multi_index<"reservespert"_n, reservespert_t> reservespert_type;

        ACTION setenable(bool enable);

        ACTION addreserve(name reserve, bool add);

        ACTION listpairres(name reserve,
                           asset token,
                           name token_contract,
                           bool eos_to_token,
                           bool token_to_eos,
                           bool add);

        ACTION trade1(memo_trade_structure memo_struct);

        ACTION trade2(name reserve,
                      memo_trade_structure memo_struct,
                      asset actual_src,
                      asset actual_dest);

        ACTION trade3(name reserve,
                      memo_trade_structure memo_struct,
                      asset actual_src,
                      asset actual_dest,
                      asset dest_before_trade);

        void transfer(name from, name to, asset quantity, string memo);

    private:
        void trade0(const struct transfer &transfer, const name code);

        void calc_actuals(memo_trade_structure &memo_struct,
                          double rate_result,
                          uint64_t rate_result_dest_amount,
                          asset &actual_src,
                          asset &actual_dest);

        int find_reserve(vector<name> reserve_list,
                         uint8_t num_reserves,
                         name reserve);

        memo_trade_structure parse_memo(string memo);
};
