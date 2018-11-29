#include "./AmmReserveNew.hpp"

using namespace eosio;

void AmmReserve::init(account_name network_contract,
                      asset        token_asset,
                      account_name token_contract,
                      account_name eos_contract,
                      bool         trade_enabled) {

    require_auth( _self );

    auto itr = state_instance.find(_self);
    eosio_assert(itr == state_instance.end(), "init already called");

    state_instance.emplace(_self, [&](auto& s) {
        s.network_contract = network_contract;
        s.token_asset = token_asset;
        s.token_contract = token_contract;
        s.eos_contract = eos_contract;
        s.trade_enabled = trade_enabled;
        s.collected_fees_in_tokens = token_asset;
        s.collected_fees_in_tokens.amount = 0;
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
    require_auth( _self );

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

void AmmReserve::resetfee() {
    require_auth( _self );

    auto itr = state_instance.find(_self);
    eosio_assert(itr != state_instance.end(), "init not called yet");

    state_instance.modify(itr, _self, [&](auto& s) {
        s.collected_fees_in_tokens.amount = 0;
    });
}

void AmmReserve::getconvrate(asset src, asset dest) {

    /* we only allow network to get conv rate since it actually writes to ram */
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
    if (rate == 0) return 0;

    dst_amount = calc_dst_amount(rate,
                                 src.symbol.precision(),
                                 src.amount,
                                 dst.symbol.precision());

    asset this_dest_balance = get_balance(_self,
                                          state_ptr.token_contract,
                                          state_ptr.token.token_asset.symbol);
    if (this_dest_balance.amount < dst_amount) return 0;

    return rate;
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

/* TODO: not sure this is correct, also consider moving to common place */
double AmmReserve::calc_dst_amount(double rate,
                                   uint64_t src_precision,
                                   uint64_t src_amount,
                                   uint64_t dest_precision) {

    return double(src_amount * pow(10,dest_precision)) /
           (rate * pow(10,src_amount));
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

double AmmReserve::p_of_e(const struct params &current_params, double e) {
    return current_params.p_min * exp(current_params.r * e);
}

double AmmReserve::delta_t_func(const struct params &current_params, double e, double delta_e) {
    return (exp(-current_params.r * delta_e) - 1.0) / (current_params.r * p_of_e(current_params, e));
}

double AmmReserve::delta_e_func(const struct params &current_params, double e, double delta_t) {
    return (-1) *
           (log(1 + current_params.r * p_of_e(current_params, e) * delta_t)) /
           current_params.r;
}

double AmmReserve::asset_to_double_amount(asset quantity) {
    return quantity.amount / pow(10,quantity.symbol.precision());
}

void AmmReserve::reserve_trade(const struct transfer &transfer, const account_name code) {

    if (transfer.to != _self) return; /* TODO: is this ok? */

    require_auth(state_ptr->network_contract);

    auto state_ptr = state_instance.find(_self);
    eosio_assert(state_ptr != state_instance.end(), "init was not called");
    const auto& state = *state_ptr;

    auto current_params_ptr = params_instance.find(_self);
    eosio_assert(current_params_ptr != param_instance.end(), "params were not set");
    const auto& current_params = *current_params_ptr;

    eosio_assert(code == current_params.token_contract || current_params.eos_contract , "needs to come from token contract or eos contract");
    eosio_assert(transfer.quantity.is_valid(), "invalid transfer");
    eosio_assert(current_params.trade_enabled, "trade is not enabled");

    eosio_assert(transfer.memo.length() == 2, "needs a memo");
    auto parsed_memo = parse_memo(transfer.memo);

    eosio_assert(transfer.quantity.symbol == EOS_SYMBOL ||
                 transfer.quantity.symbol == state.token_asset.symbol,
                 "unrecognized transfer asset symbol");

    symbol_type dest_symbol;
    account_name dest_contract;

    if (transfer.quantity.symbol == EOS_SYMBOL) {
        dest_symbol = state.token_asset.symbol;
        dest_contract = state.token_contract;
    } else {
        dest_symbol = EOS_SYMBOL;
        dest_contract = state.eos_contract;
    }

    do_trade(state,
             current_params,
             transfer.quantity,
             parsed_memo.dest_address
             parsed_memo.conversion_rate,
             dest_symbol,
             dest_contract);
}

void AmmReserve::do_trade(const struct state &state,
                          const struct params &current_params,
                          asset src,
                          account_name dest_address,
                          double conversion_rate,
                          symbol_type dest_symbol,
                          account_name dest_contract) {
    eosio_assert(conversion_rate > 0, "conversion rate must be bigger than 0");

    uint64_t dest_amount = calc_dst_amount(conversion_rate,
                                           src.symbol.precision(),
                                           src.amount,
                                           dest_symbol.precision());
    eosio_assert(dest_amount > 0, "internal error. calculated dest amount must be > 0");

    asset dest_asset;
    dest_asset.symbol = dest_symbol;
    dest_asset.amount = dest_amount;

    // add to imbalance (fees)
    bool buy;
    buy = (src.symbol == EOS_SYMBOL) ? true : false;
    asset token = buy ? dest_asset : src;
    record_imbalance(buy, current_params, token);

    send(_self, dest_address, dest_asset, dest_contract);
}

void AmmReserve::record_imbalance(const struct state &state, // TODO: Should a pointer be passed?
                                  const struct params &current_params,
                                  asset token,
                                  bool buy) { /* TODO: implement in calling function */

    /* require(val <= MAX_QTY); */

    // TODO: not sure below is correct
    uint64_t current_fees_in_tokens;

    if (buy) {
        current_fees_in_tokens = uint64_t((token.amount) * current_params.fee_percent /
                                          (100.0 - current_params.fee_percent));
        } else {
            current_fees_in_tokens = uint64_t((token.amount) * current_params.fee_percent / 100.0);
        }
    }

    state_instance.modify(state, _self, [&](auto& s) {
        s.collected_fees_in_tokens = current_fees_in_tokens;
    });
}


reserve_memo_trade_structure AmmReserve::parse_memo(std::string memo) {
    auto res = reserve_memo_trade_structure();
    auto parts = split(memo, ",");

    res.dest_address = string_to_name(parts[0].c_str());
    res.conversion_rate = stod(parts[1].c_str());

    return res;
}

void AmmReserve::apply(const account_name contract, const account_name act) {

    if (act == N(transfer)) {
        reserve_trade(unpack_action_data<struct transfer>(), contract);
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
