#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/symbol.hpp>

#include <math.h>
#include <string>
#include <vector>

using std::string;
using std::vector;
using namespace eosio;

#define EOS_PRECISION 4
#define EOS_SYMBOL S(EOS_PRECISION, EOS)
#define MAX_RATE 100000 /* up to 1M tokens per EOS */

/* TODO: should we support MAX_QTY? precision 18 leaves place for only 18.4 tokens in uint64*/
/* #define MAX_QTY pow(10,28) */

struct transfer {
    account_name from;
    account_name to;
    asset        quantity;
    std::string  memo;
};

struct account {
    asset    balance;
    uint64_t primary_key() const { return balance.symbol.name(); }
};

struct rate {
    account_name    manager;
    double          stored_rate;
    uint64_t        dest_amount;
    uint64_t        primary_key() const { return manager; }
    EOSLIB_SERIALIZE( rate, (manager)(stored_rate)(dest_amount) )
};

typedef eosio::multi_index<N(accounts), account> accounts;
typedef eosio::multi_index<N(rate), rate> rate_type;

asset get_balance(account_name user, account_name token_contract, symbol_type symbol) {
    accounts fromAcc(token_contract, user);
    auto itr = fromAcc.find(symbol.name());
    if ( itr == fromAcc.end()) {
        /* balance was never created */
        return asset(0, symbol);
    }
    const auto& userAcc = fromAcc.get(symbol.name());
    return userAcc.balance;
}

void send(account_name from, account_name to, asset quantity, account_name dest_contract) {
    action {
        permission_level{from, N(active)},
        dest_contract,
        N(transfer),
        std::make_tuple(from, to, quantity, std::string("memo"))
    }.send();
}

vector<string> split(const string& str, const string& delim) {
    vector<string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == string::npos) pos = str.length();
        string token = str.substr(prev, pos-prev);
        tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

float stof(const char* s) {
    float rez = 0, fact = 1;
    if (*s == '-') {
        s++;
        fact = -1;
    }

    for (int point_seen = 0; *s; s++) {
        if (*s == '.') {
            point_seen = 1;
            continue;
        }

        int d = *s - '0';
        if (d >= 0 && d <= 9) {
            if (point_seen) fact /= 10.0f;
            rez = rez * 10.0f + (float)d;
        }
    }

    return rez * fact;
};

double amount_to_damount(uint64_t amount, uint64_t precision) {
    return (amount / double(pow(10, precision)));
}

uint64_t damount_to_amount(double damount, uint64_t precision) {
    return uint64_t(damount * double(pow(10, precision)));
}

double asset_to_damount(asset quantity) {
    return (quantity.amount / double(pow(10, quantity.symbol.precision())));
}

uint64_t calc_src_amount(double rate,
                         uint64_t src_precision,
                         uint64_t dest_amount,
                         uint64_t dest_precision) {

    double dest_damount = amount_to_damount(dest_amount, dest_precision);
    double src_damount = dest_damount / rate;
    uint64_t src_amount = damount_to_amount(src_damount, src_precision);

    return src_amount;
}

uint64_t calc_dest_amount(double rate,
                          uint64_t src_precision,
                          uint64_t src_amount,
                          uint64_t dest_precision) {

    double src_damount = amount_to_damount(src_amount, src_precision);
    double dest_damount = src_damount * rate;
    uint64_t dest_amount = damount_to_amount(dest_damount, dest_precision);

    return dest_amount;
}
