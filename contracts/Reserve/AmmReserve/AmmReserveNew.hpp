#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include "../../Common/common.hpp"

class AmmReserve : public contract {

    public:
        using contract::contract;

        /* TODO: add equivalent to recordImbalance() */
        /* TODO: add to resetCollectedFees() */

        struct [[eosio::table]] state {
            account_name    manager;
            account_name    network_contract;
            asset           token_asset;
            account_name    token_contract;
            account_name    eos_contract;
            bool            trade_enabled;
            uint64_t        primary_key() const { return manager; }
            EOSLIB_SERIALIZE(params, (manager)(network)(token_asset)(token_contract)(eos_contract)(trade_enabled))
        };

        struct [[eosio::table]] params {
            account_name    manager;
            double          r;
            double          p_min;
            asset           max_eos_cap_buy;
            asset           max_eos_cap_sell
            double          fee_in_bps;
            asset           collected_fees_in_tokens;
            double          max_buy_rate;
            double          min_but_rate;
            double          max_sell_rate;
            double          min_sell_rate;
            uint64_t        primary_key() const { return manager; }
            EOSLIB_SERIALIZE(params, (manager)(r)(p_min)(max_eos_cap_buy)(max_eos_cap_sell)(fee_in_bps)
                                     (collected_fees_in_tokens)(max_buy_rate)(min_but_rate)(max_sell_rate)
                                     (min_sell_rate))
        };

        /* TODO: the following is duplicated with common.hpp, see if can remove from here. */
        struct [[eosio::table]] rate {
            account_name    manager;
            double          stored_rate; /* TODO - adding hash/id to make sure we read correct rate? */
            uint64_t        dest_amount; /* TODO: make it an asset to comply with standard that amounts are assets, rates are double? */
            uint64_t        primary_key() const { return manager; }
            EOSLIB_SERIALIZE( rate, (manager)(stored_rate)(dest_amount) )
        };

        typedef eosio::multi_index<N(state), params> state_type;
        typedef eosio::multi_index<N(params), params> params_type;
        typedef eosio::multi_index<N(rate), rate> rate_type;

        AmmReserve(account_name self) : contract(self),
                                        state_instance( self, self),
                                        params_instance( self, self ),
                                        rate_instance( self, self) { }

        [[eosio::action]]
        void init(account_name network_contract,
                  asset        token_asset,
                  account_name token_contract,
                  account_name eos_contract,
                  bool         trade_enabled,
                  asset        collected_fees_in_tokens);

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
        void getconvrate(asset src, asset dest);

        void apply(const account_name contract, const account_name act);

        /* TODO: do we need approvedWithdrawAddresses and approveWithdrawAddress()? */

    private:
        params_type params_instance;
        rate_type   rate_instance;
        state_type  state_instance;

        void set_enable_trade(bool enable);
        float p_of_e(const struct params &current_params, float e);
        float delta_t(const struct params &current_params, float e, float delta_e);
        float delta_e(const struct params &current_params, float e, float delta_t);
        float asset_to_float_amount(asset quantity);
        int64_t float_amount_to_asset_amount(float base_amount, asset quantity);
        void calc_dst_ext_asset(const struct params &current_params,
                                const asset &src_asset,
                                extended_asset &dst_asset);
        float calc_rate(uint64_t src_amount,
                        uint64_t src_precision,
                        uint64_t dest_amount,
                        uint64_t dest_precision);
        void transfer_recieved(const struct transfer &transfer,
                               const account_name code);
};

