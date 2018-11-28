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

#define EOS_SYMBOL S(4, EOS)
#define MAX_RATE = 100000 /* up to 1M tokens per EOS */
#define MAX_QTY = pow(10,28); /* 10B tokens */

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
    float           stored_rate;
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

void send(account_name from, account_name to, asset quantity, account_name dst_contract) {
    action {
        permission_level{from, N(active)},
        dst_contract,
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
