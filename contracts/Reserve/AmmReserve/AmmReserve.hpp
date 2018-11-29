#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include "../../Common/common.hpp"

struct reserve_memo_trade_structure {
    account_name dest_address;
    double       conversion_rate;
};

class AmmReserve : public contract {

    public:
        using contract::contract;

        struct [[eosio::table]] state {
            account_name    manager;
            account_name    network_contract;
            asset           token_asset;
            account_name    token_contract;
            account_name    eos_contract;
            bool            trade_enabled;
            asset           collected_fees_in_tokens;
            uint64_t        primary_key() const { return manager; }
            EOSLIB_SERIALIZE(state, (manager)(network_contract)(token_asset)(token_contract)
                                     (eos_contract)(trade_enabled)(collected_fees_in_tokens))
        };

        struct [[eosio::table]] params {
            account_name    manager;
            double          r;
            double          p_min;
            asset           max_eos_cap_buy;
            asset           max_eos_cap_sell;
            double          fee_percent;
            double          max_buy_rate;
            double          min_buy_rate;
            double          max_sell_rate;
            double          min_sell_rate;
            uint64_t        primary_key() const { return manager; }
            EOSLIB_SERIALIZE(params, (manager)(r)(p_min)(max_eos_cap_buy)(max_eos_cap_sell)(fee_percent)
                                     (max_buy_rate)(min_buy_rate)(max_sell_rate)(min_sell_rate))
        };

        /* TODO: the following is duplicated with common.hpp, see if can remove from here. */
        struct [[eosio::table]] rate {
            account_name    manager;
            double          stored_rate; /* TODO - adding hash/id to make sure we read correct rate? */
            uint64_t        dest_amount; /* TODO: make it an asset to comply with standard that amounts are assets, rates are double? */
            uint64_t        primary_key() const { return manager; }
            EOSLIB_SERIALIZE(rate, (manager)(stored_rate)(dest_amount))
        };

        typedef eosio::multi_index<N(state), state> state_type;
        typedef eosio::multi_index<N(params), params> params_type;
        typedef eosio::multi_index<N(rate), rate> rate_type;

        AmmReserve(account_name self) : contract(self),
                                        state_instance(self, self),
                                        params_instance(self, self),
                                        rate_instance(self, self) { }

        [[eosio::action]]
        void init(account_name network_contract,
                  asset        token_asset,
                  account_name token_contract,
                  account_name eos_contract,
                  bool         enable_trade);

        [[eosio::action]]
        void setparams(double r,
                       double p_min,
                       asset  max_eos_cap_buy,
                       asset  max_eos_cap_sell,
                       double fee_percent,
                       double max_sell_rate,
                       double min_sell_rate);

        [[eosio::action]]
        void setnetwork(account_name network_contract);

        [[eosio::action]]
        void enabletrade();

        [[eosio::action]]
        void disabletrade();

        [[eosio::action]]
        void resetfee();

        [[eosio::action]]
        void getconvrate(asset src);

        void apply(const account_name contract, const account_name act);

        /* TODO: do we need approvedWithdrawAddresses and approveWithdrawAddress()? */

    private:
        params_type params_instance;
        rate_type   rate_instance;
        state_type  state_instance;

        void reserve_get_conv_rate(asset      src,
                                   double     &rate,
                                   uint64_t   &dst_amount);

        double liquidity_get_rate(const struct state &current_state,
                                  const struct params &current_params,
                                  bool is_buy,
                                  asset src_asset);


        double get_rate_with_e(const struct params &current_params,
                               bool is_buy,
                               asset src_asset,
                               double e);

        double rate_after_validation(const struct params &current_params,
                                     double rate,
                                     bool buy);

        double buy_rate(const struct params &current_params, double e, double delta_e);

        double buy_rate_zero_quantity(const struct params &current_params, double e);

        void sell_rate(const struct params &current_params,
                       double e,
                       double sell_input_qty,
                       double delta_t,
                       double &rate,
                       double &delta_e);

        double sell_rate_zero_quantity(const struct params &current_params, double e);

        double value_after_reducing_fee(const struct params &current_params,double val);

        double p_of_e(const struct params &current_params, double e);

        double delta_t_func(const struct params &current_params, double e, double delta_e);

        double delta_e_func(const struct params &current_params, double e, double delta_t);

        void reserve_trade(const struct transfer &transfer, const account_name code);

        void do_trade(const struct state &state,
                      const struct params &current_params,
                      asset src,
                      account_name dest_address,
                      double conversion_rate,
                      symbol_type dest_symbol,
                      account_name dest_contract);

        void record_imbalance(const struct params &current_params,
                              asset token,
                              bool buy);

        reserve_memo_trade_structure parse_memo(std::string memo);
};

