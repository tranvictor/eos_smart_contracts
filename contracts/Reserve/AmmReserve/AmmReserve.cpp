#include "./AmmReserve.hpp"

using namespace eosio;

void AmmReserve::init(account_name network_contract,
                      asset        token_asset,
                      account_name token_contract,
                      account_name eos_contract,
                      bool         enable_trade) {

    require_auth( _self );

    auto itr = state_instance.find(_self);
    eosio_assert(itr == state_instance.end(), "init already called");

    state_instance.emplace(_self, [&](auto& s) {
        s.manager = _self;
        s.network_contract = network_contract;
        s.token_asset = token_asset;
        s.token_contract = token_contract;
        s.eos_contract = eos_contract;
        s.trade_enabled = enable_trade;
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
            s.p_min = p_min;
            s.max_eos_cap_buy = max_eos_cap_buy;
            s.max_eos_cap_sell = max_eos_cap_sell;
            s.fee_percent = fee_percent;
            s.max_buy_rate = 1.0 / min_sell_rate;
            s.min_buy_rate = 1.0 / max_sell_rate;
            s.max_sell_rate = max_sell_rate;
            s.min_sell_rate = min_sell_rate;
        });
    } else {
        params_instance.emplace(_self, [&](auto& s) {
            s.manager = _self;
            s.r = r;
            s.p_min = p_min;
            s.max_eos_cap_buy = max_eos_cap_buy;
            s.max_eos_cap_sell = max_eos_cap_sell;
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

void AmmReserve::getconvrate(asset src) {

    double rate;
    uint64_t dst_amount;

    reserve_get_conv_rate(src, rate, dst_amount);
    if(rate == 0) dst_amount = 0;

    auto itr = rate_instance.find(_self);
    if(itr != rate_instance.end()) {
        rate_instance.modify(itr, _self, [&](auto& s) {
            s.stored_rate = rate;
            s.dest_amount = dst_amount;
        });
    } else {
        rate_instance.emplace( _self, [&](auto& s) {
            s.manager = _self;
            s.stored_rate = rate;
            s.dest_amount = dst_amount;
        });
    }
}

double AmmReserve::reserve_get_conv_rate(asset      src,
                                         double     &rate,
                                         uint64_t   &dst_amount) {

    /* verify contract was init */
    auto state_ptr = state_instance.find(_self);
    if (state_ptr == state_instance.end()) return 0;
    const auto& current_state = *state_ptr;

    /* verify params were set */
    auto current_params_ptr = params_instance.find(_self);
    if (current_params_ptr == params_instance.end()) return 0;
    const auto& current_params = *current_params_ptr;

    /* we only allow network to get conv rate since it actually writes to ram */
    require_auth(state_ptr->network_contract);

    if (!current_state.trade_enabled) return 0;

    bool is_buy = (EOS_SYMBOL == src.symbol) ? true : false;
    uint64_t dest_precision = is_buy ? EOS_PRECISION : current_state.token_asset.symbol.precision();

    rate = liquidity_get_rate(current_state, current_params, is_buy, src);
    if (rate == 0) return 0;

    dst_amount = calc_dst_amount(rate,
                                 src.symbol.precision(),
                                 src.amount,
                                 dest_precision);

    asset this_dest_balance = get_balance(_self,
                                          state_ptr->token_contract,
                                          state_ptr->token_asset.symbol);
    if (this_dest_balance.amount < dst_amount) return 0;

    return rate;
}

double AmmReserve::liquidity_get_rate(const struct state &current_state,
                                      const struct params &current_params,
                                      bool is_buy,
                                      asset src_asset) {

    /* require(qtyInSrcWei <= MAX_QTY); */

    asset eos_balance = get_balance(_self, current_state.eos_contract, EOS_SYMBOL);
    double e = asset_to_damount(eos_balance);
    double rate = get_rate_with_e(current_params, is_buy, src_asset, e);

    return rate;
}

double AmmReserve::get_rate_with_e(const struct params &current_params,
                                   bool is_buy,
                                   asset src_asset,
                                   double e) {

    double delta_e;
    double sell_input_qty;
    double delta_t;

    double qty_in_src = asset_to_damount(src_asset);
    double rate;

    if (is_buy) {
        /* EOS goes in, token goes out */
        delta_e = qty_in_src;
        if (src_asset.amount > current_params.max_eos_cap_buy.amount) return 0;

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

        if (delta_e > current_params.max_eos_cap_sell.amount) return 0;
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

void AmmReserve::sell_rate(const struct params &current_params,
                           double e,
                           double sell_input_qty,
                           double delta_t,
                           double &rate,
                           double &delta_e) {

    delta_e = delta_e_func(current_params, e, delta_t);
    /* require(deltaEInFp <= maxQtyInFp); */
    rate =  delta_e / sell_input_qty;
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
    return (-1) *
           ((exp(-current_params.r * delta_e) - 1.0) / (current_params.r * p_of_e(current_params, e)));
}

double AmmReserve::delta_e_func(const struct params &current_params, double e, double delta_t) {
    return (1) *
           ((log(1 + current_params.r * p_of_e(current_params, e) * delta_t)) / current_params.r);
}

void AmmReserve::reserve_trade(const struct transfer &transfer, const account_name code) {

    if (transfer.to != _self) return; /* TODO: is this ok? */

    auto state_ptr = state_instance.find(_self);
    eosio_assert(state_ptr != state_instance.end(), "init was not called");
    const auto& state = *state_ptr;

    eosio_assert(transfer.from == state.network_contract, "trade can only come from contract");

    auto current_params_ptr = params_instance.find(_self);
    eosio_assert(current_params_ptr != params_instance.end(), "params were not set");
    const auto& current_params = *current_params_ptr;

    eosio_assert(code == state.token_contract || code == state.eos_contract , "needs to come from token contract or eos contract");
    eosio_assert(transfer.quantity.is_valid(), "invalid transfer");
    eosio_assert(state.trade_enabled, "trade is not enabled");

    eosio_assert(transfer.memo.length() > 0 , "needs a memo");
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
             parsed_memo.dest_address,
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
    record_imbalance(current_params, token, buy);

    send(_self, dest_address, dest_asset, dest_contract);
}

void AmmReserve::record_imbalance(const struct params &current_params,
                                  asset token,
                                  bool buy) {

    /* require(val <= MAX_QTY); */

    double dfees;
    double token_damount = amount_to_damount(token.amount, token.symbol.precision());
    uint64_t fees_amount;

    if (buy) {
        dfees = (token_damount * current_params.fee_percent / (100.0 - current_params.fee_percent));
    } else {
        dfees = (token_damount * current_params.fee_percent) / 100.0;
    }
    fees_amount = damount_to_amount(dfees, token.symbol.precision());

    auto itr = state_instance.find(_self);
    state_instance.modify(itr, _self, [&](auto& s) {
        s.collected_fees_in_tokens.amount = fees_amount;
    });
}


reserve_memo_trade_structure AmmReserve::parse_memo(std::string memo) {
    auto res = reserve_memo_trade_structure();
    auto parts = split(memo, ",");

    res.dest_address = string_to_name(parts[0].c_str());
    res.conversion_rate = stof(parts[1].c_str()); /* TODO - is stof ok for parsing to double? */

    return res;
}

void AmmReserve::apply(const account_name contract, const account_name act) {

    if (act == N(transfer)) {
        reserve_trade(unpack_action_data<struct transfer>(), contract);
        return;
    }

    auto &thiscontract = *this;

    switch (act) {
        EOSIO_API(AmmReserve, (init)(setparams)(setnetwork)(enabletrade)(disabletrade)(resetfee)(getconvrate))
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
