#include "./AmmReserve.hpp"
#include <math.h>

using namespace eosio;

void AmmReserve::setparams(float        r,
                           float        p_min,
                           asset        token_asset,
                           account_name token_contract,
                           account_name eos_contract)
{
    require_auth( _self );
    // TODO - add more assertions here.

    auto itr = params_instance.find(_self);
    if (itr != params_instance.end()) {
        params_instance.modify(itr, _self, [&](auto& s) {
            s.r  = r;
            s.p_min = p_min;
            s.token_asset = token_asset;
            s.token_contract = token_contract;
            s.eos_contract = eos_contract;
            s.manager = _self;
        });
    } else {
        params_instance.emplace(_self, [&](auto& s) {
            s.r  = r;
            s.p_min = p_min;
            s.token_asset = token_asset;
            s.token_contract = token_contract;
            s.eos_contract = eos_contract;
            s.trade_enabled = true;
            s.manager = _self;
        });
    }
}

void AmmReserve::enabletrade() {
    require_auth( _self );
    set_enable_trade(true);
}

void AmmReserve::disabletrade() {
    require_auth( _self );
    set_enable_trade(false);
}

void AmmReserve::set_enable_trade(bool enable) {
    auto itr = params_instance.find(_self);
    if (itr != params_instance.end()) {
        params_instance.modify(itr, _self, [&](auto& s) {
            s.trade_enabled = enable;
        });
    } else {
        params_instance.emplace(_self, [&](auto& s) {
            s.trade_enabled = enable;
        });
    }
}

void AmmReserve::getconvrate(asset src, asset dest) {
    extended_asset dst_ext_asset;
    bool return_zero_rate = false;
    float rate;
    auto current_params_ptr = params_instance.find(_self);

    if ((!src.is_valid()) || (current_params_ptr == params_instance.end())) {
        return_zero_rate = true;
    }

    const auto& current_params = *current_params_ptr;
    if (!current_params.trade_enabled) return_zero_rate = true;

    if( return_zero_rate) {
        rate = 0;
        dst_ext_asset.quantity.amount = 0;
    } else {
        calc_dst_ext_asset(current_params, src, dst_ext_asset);
        rate = calc_rate(src.amount,
                         src.symbol.precision(),
                         dst_ext_asset.quantity.amount,
                         dst_ext_asset.quantity.symbol.precision());
    }

    auto itr = rate_instance.find(_self);
    if(itr != rate_instance.end()) {
        rate_instance.modify(itr, _self, [&](auto& s) {
            s.stored_rate = rate;
            s.dest_amount = dst_ext_asset.quantity.amount;
            s.manager = _self;
        });
    } else {
        rate_instance.emplace( _self, [&](auto& s) {
            s.stored_rate = rate;
            s.dest_amount = dst_ext_asset.quantity.amount;
            s.manager = _self;
        });
    }
}

float AmmReserve::p_of_e(const struct params &current_params, float e) {
    return current_params.p_min * exp(current_params.r * e);
}

float AmmReserve::delta_t(const struct params &current_params, float e, float delta_e) {
    return (exp(-current_params.r * delta_e) - 1.0) / (current_params.r * p_of_e(current_params, e));
}

float AmmReserve::delta_e(const struct params &current_params, float e, float delta_t) {
    return (-1) *
           (log(1 + current_params.r * p_of_e(current_params, e) * delta_t)) /
           current_params.r;
}

float AmmReserve::asset_to_float_amount(asset quantity) {
    return quantity.amount / pow(10,quantity.symbol.precision());
}

int64_t AmmReserve::float_amount_to_asset_amount(float base_amount, asset quantity) {
    return (base_amount * pow(10,quantity.symbol.precision()));
}

void AmmReserve::calc_dst_ext_asset(const struct params &current_params,
                                    const asset &src_asset,
                                    extended_asset &dst_ext_asset) {
    asset eos_balance = get_balance(_self, current_params.eos_contract, EOS_SYMBOL);
    float e = asset_to_float_amount(eos_balance);
    float delta_in = asset_to_float_amount(src_asset);
    float delta_out_negative;

    if (src_asset.symbol == EOS_SYMBOL) {
        delta_out_negative = delta_t(current_params, e, delta_in);
        dst_ext_asset.quantity.symbol = current_params.token_asset.symbol;
        dst_ext_asset.contract = name{current_params.token_contract};
    } else if (src_asset.symbol == current_params.token_asset.symbol) {
        delta_out_negative = delta_e(current_params, e, delta_in);
        dst_ext_asset.quantity.symbol = EOS_SYMBOL;
        dst_ext_asset.contract = name{current_params.eos_contract};
    }

    float delta_out = abs(delta_out_negative);
    dst_ext_asset.quantity.amount = float_amount_to_asset_amount(delta_out, dst_ext_asset.quantity);
}

float AmmReserve::calc_rate(uint64_t src_amount,
                            uint64_t src_precision,
                            uint64_t dest_amount,
                            uint64_t dest_precision) {
    /* example:
    X=2.01 (amount=201, precision=2)
    Y=3.0010 (amount=30010, precision=4)
    Y/X = 201 * 10^4 / (30010 * 10^2) = (201/30010) * 10^2 = 20100 / 30010 ~= 2/3.
    */
    return float(dest_amount * pow(10,src_precision)) /
           float(src_amount * pow(10,dest_precision));
}

void AmmReserve::transfer_recieved(const struct transfer &transfer, const account_name code) {
    if (transfer.to != _self) return; /* TODO: is this ok? */

    auto current_params_ptr = params_instance.find(_self);
    eosio_assert( current_params_ptr != params_instance.end(), "params not created yet" );
    const auto& current_params = *current_params_ptr;

    eosio_assert(code == current_params.token_contract || current_params.eos_contract , "needs to come from token contract or eos contract");
    eosio_assert(transfer.memo.length() > 0, "needs a memo with the name");
    eosio_assert(transfer.quantity.is_valid(), "invalid transfer");
    eosio_assert(current_params.trade_enabled, "trade is not enabled");

    account_name dest_address = string_to_name(transfer.memo.c_str());
    extended_asset dst_ext_asset;
    calc_dst_ext_asset(current_params, transfer.quantity, dst_ext_asset);

    send(_self, dest_address, dst_ext_asset.quantity, dst_ext_asset.contract);
}

void AmmReserve::apply(const account_name contract, const account_name act) {

    if (act == N(transfer)) {
        transfer_recieved(unpack_action_data<struct transfer>(), contract);
        return;
    }

    auto &thiscontract = *this;

    switch (act) {
        EOSIO_API(AmmReserve, (setparams)(getconvrate))
    };
}

extern "C" {

    using namespace eosio;

    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        auto self = receiver;
        AmmReserve contract(self);
        contract.apply(code, action);
        eosio_exit(0);
    }
}
