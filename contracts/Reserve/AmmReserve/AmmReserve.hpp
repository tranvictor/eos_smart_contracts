#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include "../../Common/common.hpp"

class AmmReserve : public contract {

    public:
        using contract::contract;

        struct [[eosio::table]] params {
            account_name    manager;
            asset           token_asset;
            account_name    token_contract;
            account_name    eos_contract;
            float           r;
            float           p_min;
            bool            trade_enabled;
            uint64_t        primary_key() const { return manager; }
            EOSLIB_SERIALIZE( params, (manager)(token_asset)(token_contract)(eos_contract)(r)(p_min)(trade_enabled) )
        };

        /* TODO: the following is duplicated with common.hpp, see if can remove from here. */
        struct [[eosio::table]] rate {
            account_name    manager;
            float           stored_rate; /* TODO - adding hash/id to make sure we read correct rate? */
            uint64_t        dest_amount;
            uint64_t        primary_key() const { return manager; }
            EOSLIB_SERIALIZE( rate, (manager)(stored_rate)(dest_amount) )
        };

        typedef eosio::multi_index<N(params), params> params_type;
        typedef eosio::multi_index<N(rate), rate> rate_type;

        AmmReserve(account_name self) : contract(self),
                                        params_instance( self, self ),
                                        rate_instance( self, self) {}

        [[eosio::action]]
        void setparams(float        r,
                       float        p_min,
                       asset        token_asset, /* TODO: align with names fron network, e.g _asset */
                       account_name token_contract,
                       account_name eos_contract);

        [[eosio::action]]
        void enabletrade();

        [[eosio::action]]
        void disabletrade();

        [[eosio::action]]
        void getconvrate(asset src, asset dest);

        void apply(const account_name contract, const account_name act);

        //TODO: add: address public kyberNetwork;
        //TODO: add: bool public tradeEnabled;
        //TODO: add: function enableTrade() public onlyAdmin returns(bool) {
        //TODO: add: function disableTrade() public onlyAlerter returns(bool) {
        //TODO: do we need approvedWithdrawAddresses and approveWithdrawAddress()?

    private:
        params_type params_instance;
        rate_type   rate_instance;

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

