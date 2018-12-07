#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>
#include "../../Common/common.hpp"

CONTRACT AmmReserve : public contract {

    public:
        using contract::contract;

        TABLE state_t {
            name        network_contract;
            asset       token;
            name        token_contract;
            name        eos_contract;
            bool        trade_enabled;
            asset       collected_fees_in_tokens;
            EOSLIB_SERIALIZE(state_t, (network_contract)(token)(token_contract)(eos_contract)
                                      (trade_enabled)(collected_fees_in_tokens))
        };

        TABLE params_t {
            double      r;
            double      p_min;
            asset       max_eos_cap_buy;
            asset       max_eos_cap_sell;
            double      fee_percent;
            double      max_buy_rate;
            double      min_buy_rate;
            double      max_sell_rate;
            double      min_sell_rate;
            EOSLIB_SERIALIZE(params_t, (r)(p_min)(max_eos_cap_buy)(max_eos_cap_sell)(fee_percent)
                                       (max_buy_rate)(min_buy_rate)(max_sell_rate)(min_sell_rate))
        };

        /* TODO: the following is duplicated with common.hpp, see if can remove from here. */
        TABLE rate_t {
            double      stored_rate; /* TODO - adding hash/id to make sure we read correct rate? */
            uint64_t    dest_amount; /* TODO: make it an asset to comply with standard that amounts are assets, rates are double? */
            EOSLIB_SERIALIZE(rate_t, (stored_rate)(dest_amount))
        };

        typedef eosio::singleton<"state"_n, state_t> state_type;
        typedef eosio::multi_index<"state"_n, state_t> dummy_state_for_abi; /* hack until abi generator generates correct name */
        typedef eosio::singleton<"params"_n, params_t> params_type;
        typedef eosio::multi_index<"params"_n, params_t> dummy_params_for_abi; /* hack until abi generator generates correct name */
        typedef eosio::singleton<"rate"_n, rate_t> rate_type;
        typedef eosio::multi_index<"rate"_n, rate_t> dummy_rate_for_abi; /* hack until abi generator generates correct name */

        ACTION init(name    network_contract,
                    asset   token,
                    name    token_contract,
                    name    eos_contract,
                    bool    enable_trade);

         ACTION setparams(double r,
                          double p_min,
                          asset  max_eos_cap_buy,
                          asset  max_eos_cap_sell,
                          double fee_percent,
                          double max_sell_rate,
                          double min_sell_rate);

        ACTION setnetwork(name network_contract);

        ACTION enabletrade();

        ACTION disabletrade();

        ACTION resetfee();

        ACTION getconvrate(asset src);

        void transfer(name from, name to, asset quantity, string memo);

    private:

        double reserve_get_conv_rate(asset      src,
                                     uint64_t   &dest_amount);

        double liquidity_get_rate(const struct state_t &current_state,
                                  const struct params_t &current_params,
                                  bool is_buy,
                                  asset src);


        double get_rate_with_e(const struct params_t &current_params,
                               bool is_buy,
                               asset src,
                               double e);

        double rate_after_validation(const struct params_t &current_params,
                                     double rate,
                                     bool buy);

        double buy_rate(const struct params_t &current_params, double e, double delta_e);

        double buy_rate_zero_quantity(const struct params_t &current_params, double e);

        double sell_rate(const struct params_t &current_params,
                         double e,
                         double sell_input_qty,
                         double delta_t,
                         double &delta_e);

        double sell_rate_zero_quantity(const struct params_t &current_params, double e);

        double value_after_reducing_fee(const struct params_t &current_params,double val);

        double p_of_e(const struct params_t &current_params, double e);

        double delta_t_func(const struct params_t &current_params, double e, double delta_e);

        double delta_e_func(const struct params_t &current_params, double e, double delta_t);

        void reserve_trade(name from, asset quantity, string memo, name code);

        void do_trade(const struct params_t &current_params,
                      asset src,
                      name dest_address,
                      double conversion_rate,
                      symbol dest_symbol,
                      name dest_contract);

        void record_fees(const struct params_t &current_params,
                         asset token,
                         bool buy);
};

