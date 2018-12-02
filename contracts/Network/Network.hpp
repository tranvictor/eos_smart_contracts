#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include "../Common/common.hpp"

using namespace eosio;
using namespace std; /* TODO: can remove it and replace string with std::string in some places. */

#define MAX_RESERVES_PER_TOKEN  5
#define NOT_FOUND              -1

struct memo_trade_structure {
    account_name    trader;
    account_name    src_contract; /* TODO: potentially read from storage since pair is listed. */
    asset           src;
    account_name    dest_contract; /* TODO: potentially read from storage since pair is listed. */
    asset           dest;
    account_name    dest_account;
    uint64_t        max_dest_amount;
    double          min_conversion_rate;
    account_name    walletId;
    std::string     hint; /* TODO: should hint be another type? */
};

class Network : public contract {

    public:
        using contract::contract;

        Network(account_name self): contract(self),
                                    state_table_inst(self,self),
                                    reserves_table_inst(self,self),
                                    reservespert_table_inst(self,self) {}

        struct [[eosio::table]] state {
            account_name    manager;
            bool            is_enabled;
            uint64_t        primary_key() const { return manager; }
            EOSLIB_SERIALIZE( state, (manager)(is_enabled) )
        };

        struct [[eosio::table]] reserve {
            account_name    contract;
            uint64_t        primary_key() const { return contract; }
            EOSLIB_SERIALIZE( reserve, (contract) )
        };

        /* TODO: change to 2 lists of per_token_src and per_token_dest */
        struct [[eosio::table]] reservespert {
            uint64_t                    symbol;
            uint64_t                    token_contract;
            std::vector<account_name>   reserve_contracts;
            uint8_t                     num_reserves;
            uint64_t                    primary_key() const { return symbol; }
            EOSLIB_SERIALIZE(reservespert, (symbol)(token_contract)(reserve_contracts)(num_reserves))
        };

        typedef eosio::multi_index<N(state), state> state_table;
        typedef eosio::multi_index<N(reserve), reserve> reserves_table;
        typedef eosio::multi_index<N(reservespert), reservespert> reservespert_table;

        [[eosio::action]]
         void setenable(bool enable);

        [[eosio::action]]
        void addreserve(account_name reserve, bool add);

        [[eosio::action]]
        void listpairres(account_name reserve,
                         asset token,
                         account_name token_contract,
                         bool eos_to_token,
                         bool token_to_eos,
                         bool add);

        [[eosio::action]]
         void trade1(account_name reserve, memo_trade_structure memo_struct);

        [[eosio::action]]
         void trade2(account_name reserve,
                     memo_trade_structure memo_struct,
                     asset actual_src,
                     asset actual_dest);

        [[eosio::action]]
         void trade3(account_name reserve,
                     memo_trade_structure memo_struct,
                     asset actual_src,
                     asset actual_dest,
                     asset dest_before_trade);

        void apply(const account_name code, const account_name act);

    private:
        state_table         state_table_inst;
        reserves_table      reserves_table_inst;
        reservespert_table  reservespert_table_inst;

        void trade0(const struct transfer &transfer, const account_name code);

        void calc_actuals(memo_trade_structure &memo_struct,
                          double rate_result,
                          uint64_t rate_result_dest_amount,
                          asset &actual_src,
                          asset &actual_dest);

        int find_reserve(std::vector<account_name> reserve_list,
                         uint8_t num_reserves,
                         account_name reserve);

        memo_trade_structure parse_memo(std::string memo);
};
