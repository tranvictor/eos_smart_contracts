#include "./AmmReserve.hpp"

using namespace eosio;

void AmmReserve::init(account_name network_contract,
                      asset        token_asset,
                      account_name token_contract,
                      account_name eos_contract,
                      bool         trade_enabled,
                      asset        collected_fees_in_tokens) {

    require_auth( _self );

    auto itr = state_instance.find(_self);
    eosio_assert(itr == state_instance.end(), "init already called");

    state_instance.emplace(_self, [&](auto& s) {
        s.network_contract = network_contract;
        s.token_asset = token_asset;
        s.token_contract = token_contract;
        s.eos_contract = eos_contract;
        s.trade_enabled = trade_enabled;
        s.collected_fees_in_tokens = collected_fees_in_tokens;
    });
}

void AmmReserve::setparams(double r,
                           double p_min,
                           asset  max_eos_cap_buy,
                           asset  max_eos_cap_sell,
                           double fee_percent,
                           double max_sell_rate,
                           double min_sell_rate) {

    require_auth( _self );

    eosio_assert(fee_percent < 100, "illegal fee_percent");
    eosio_assert(min_sell_rate < max_sell_rate, "min_sell_rate not smaller than max_sell_rate");

    auto itr = params_instance.find(_self);
    if (itr != params_instance.end()) {
        params_instance.modify(itr, _self, [&](auto& s) {
            s.r = r;
            s.p_min = p_min
            s.max_eos_cap_buy = max_eos_cap_buy;
            s.max_eos_cap_sell = max_eos_cap_sell
            s.fee_percent = fee_percent;
            s.max_buy_rate = 1.0 / min_sell_rate;
            s.min_buy_rate = 1.0 / max_sell_rate;
            s.max_sell_rate = max_sell_rate;
            s.min_sell_rate = min_sell_rate;
        });
    } else {
        params_instance.emplace(_self, [&](auto& s) {
            s.r = r;
            s.p_min = p_min
            s.max_eos_cap_buy = max_eos_cap_buy;
            s.max_eos_cap_sell = max_eos_cap_sell
            s.fee_percent = fee_percent;
            s.max_buy_rate = 1.0 / min_sell_rate;
            s.min_buy_rate = 1.0 / max_sell_rate;
            s.max_sell_rate = max_sell_rate;
            s.min_sell_rate = min_sell_rate;
        });
    }
}

void AmmReserve::setnetwork(account_name network_contract) {
    auto itr = state_instance.find(_self);
    eosio_assert(itr != state_instance.end(), "init not called yet");

    state_instance.modify(itr, _self, [&](auto& s) {
        s.network_contract = network_contract;
    });
}

void AmmReserve::enabletrade() {
    require_auth( _self );

    auto itr = state_instance.find(_self);
    eosio_assert(itr != state_instance.end(), "init not called yet");

    state_instance.modify(itr, _self, [&](auto& s) {
        s.trade_enabled = true;
    });
}

void AmmReserve::disabletrade() {
    require_auth( _self );

    auto itr = state_instance.find(_self);
    eosio_assert(itr != state_instance.end(), "init not called yet");

    state_instance.modify(itr, _self, [&](auto& s) {
        s.trade_enabled = false;
    });
}

void AmmReserve::getconvrate(asset src, asset dest) {

    require_auth(state_ptr->network_contract);

    uint64_t dst_amount;
    reserve_get_conv_rate(src, dest, rate, dst_amount);
    if(rate == 0) dst_amount = 0;

    auto itr = rate_instance.find(_self);
    if(itr != rate_instance.end()) {
        rate_instance.modify(itr, _self, [&](auto& s) {
            s.stored_rate = rate;
            s.dest_amount = dst_ext_asset.quantity.amount;
        });
    } else {
        rate_instance.emplace( _self, [&](auto& s) {
            s.manager = _self;
            s.stored_rate = rate;
            s.dest_amount = dst_ext_asset.quantity.amount;
        });
    }
}

double AmmReserve::reserve_get_conv_rate(asset      src,
                                         asset      dest,
                                         double     &rate,
                                         uint64_t   &dst_amount) {
    asset token;
    bool is_buy;

    /* verify contract was init */
    auto state_ptr = state_instance.find(_self);
    if (state_ptr == state_instance.end()) return 0;

    /* verify params were set */
    auto current_params_ptr = params_instance.find(_self);
    if (current_params_ptr == param_instance.end()) return 0;

    if (!current_params_ptr.trade_enabled) return 0;

    if (EOS_SYMBOL == src.symbol) {
        is_buy = true;
        token = dest;
    } else if (EOS_SYMBOL == dest.symbol) {
        is_buy = false;
        token = src;
    } else {
        return 0;
    }

    auto rate = liquidity_get_rate(current_params_ptr, token, is_buy, src);
    dst_amount = get_dest_qty(src, dest, rate);

    asset this_dest_balance = get_balance(_self,
                                          state_ptr.token_contract,
                                          state_ptr.token.token_asset.symbol);
    if (this_dest_balance.amount < dst_amount) return 0;

    return rate;
}

uint64_t AmmReserve::get_dest_qty(asset  src,
                                  asset  dest,
                                  double rate) {

#if 0
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
#endif
}

double AmmReserve::liquidity_get_rate(params *current_params_ptr,
                                      asset token,
                                      bool is_buy,
                                      asset src_asset) {


    /* require(qtyInSrcWei <= MAX_QTY); */

    const auto& current_params = *current_params_ptr;

    asset eos_balance = get_balance(_self, current_params.eos_contract, EOS_SYMBOL);
    double e = asset_to_double_amount(eos_balance);
    double rate = get_rate_with_e(current_params, token, is_buy, src_asset, e);

    /* TODO: following also appears in rate_after_validation, can remove from one of them */
    if (rate < MAX_RATE) return 0;
    return rate;
}

double AmmReserve::get_rate_with_e(params *current_params_ptr,
                                   asset token,
                                   bool is_buy,
                                   asset src_asset,
                                   double e) {

    const auto& current_params = *current_params_ptr;

    double delta_e;
    double sell_input_qty;
    double delta_t;

    double qty_in_src = asset_to_double_amount(src_asset);
    double rate;

    if (is_buy) {
        /* EOS goes in, token goes out */

        delta_e = qty_in_src;
        if (src_asset > current_params.max_eos_cap_buy) return 0;

        if (delta_e == 0) {
            rate = buy_rate_zero_quantity(current_params, e);
        } else {
            rate = buy_rate(current_params, e, delta_e);
        }
    } else {
        sell_input_qty = qty_in_src;
        auto delta_t = value_after_reducing_fee(current_params, sell_input_qty);
        if (delta_t == 0) {
            rate = sell_rate_zero_quantity(current_params, e);
            delta_e = 0;
        } else {
            /* rate and delta_e are passed by reference below */
            sell_rate(current_params, e, sell_input_qty, delta_t, rate, delta_e);
        }

        if (delta_e > current_params.max_eos_cap_sell) return 0;
    }

    return rate_after_validation(current_params, rate, is_buy);
}

double AmmReserve::rate_after_validation(const struct params &current_params,
                                         double rate,
                                         bool buy) {
    double min_allowed_rate;
    double max_allowed_rate;

    if (buy) {
        min_allowed_rate = current_params.min_buy_rate;
        max_allowed_rate = current_params.max_buy_rate;
    } else {
        min_allowed_rate = current_params.min_sell_rate;
        max_allowed_rate = current_params.max_sell_rate;
    }

    if ((rate > max_allowed_rate) || (rate < min_allowed_rate)) {
        return 0;
    } else if (rate > MAX_RATE) {
        return 0;
    } else {
        return rate;
    }
}

double AmmReserve::buy_rate(const struct params &current_params, double e, double delta_e) {
    double delta_t = delta_t_func(current_params, e, delta_e);
    /* require(deltaTInFp <= maxQtyInFp); */
    delta_t = value_after_reducing_fee(current_params, delta_t);
    return delta_t / delta_e;
}

double AmmReserve::buy_rate_zero_quantity(const struct params &current_params, double e) {
    double rate_pre_reduction = 1 / p_of_e(current_params, e);
    return value_after_reducing_fee(current_params, rate_pre_reduction);
}

double AmmReserve::sell_rate(const struct params &current_params,
                             double e,
                             double sell_input_qty,
                             double delta_t,
                             double &rate,
                             double &delta_e) {

    delta_e = delta_e_func(current_params, e, delta_t);
    /* require(deltaEInFp <= maxQtyInFp); */
    rate = delta_e / sell_input_qty;
}

double AmmReserve::sell_rate_zero_quantity(const struct params &current_params, double e) {
    double rate_pre_reduction = p_of_e(current_params, e);
    return value_after_reducing_fee(current_params, rate_pre_reduction);
}

double AmmReserve::value_after_reducing_fee(const struct params &current_params, double val) {
    /* require(val <= BIG_NUMBER); */
    return ((100.0 - current_params.fee_percent) * val) / 100.0;
}

float AmmReserve::p_of_e(const struct params &current_params, float e) {
    return current_params.p_min * exp(current_params.r * e);
}

float AmmReserve::delta_t_func(const struct params &current_params, float e, float delta_e) {
    return (exp(-current_params.r * delta_e) - 1.0) / (current_params.r * p_of_e(current_params, e));
}

float AmmReserve::delta_e_func(const struct params &current_params, float e, float delta_t) {
    return (-1) *
           (log(1 + current_params.r * p_of_e(current_params, e) * delta_t)) /
           current_params.r;
}

float AmmReserve::asset_to_double_amount(asset quantity) {
    return quantity.amount / pow(10,quantity.symbol.precision());
}

int64_t AmmReserve::float_amount_to_asset_amount(float base_amount, asset quantity) {
    return (base_amount * pow(10,quantity.symbol.precision()));
}


/////////// from here transfer related stuff I did not touch yet ////////////////

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
