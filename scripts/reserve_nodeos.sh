#cleos wallet unlock --password XXXXXXXXXXXXXXXXXXXX...

#in another shell:
#rm -rf ~/.local/share/eosio/nodeos/data
#nodeos -e -p eosio --plugin eosio::chain_api_plugin --plugin eosio::history_api_plugin --contracts-console --verbose-http-errors

set -x

#rm contracts/Token/*.wasm contracts/Token/*.abi contracts/Reserve/AmmReserve/*.wasm contracts/Reserve/AmmReserve/*.abi 

PUBLIC_KEY=EOS5CYr5DvRPZvfpsUGrQ2SnHeywQn66iSbKKXn4JDTbFFr36TRTX
cleos create account eosio alice $PUBLIC_KEY
cleos create account eosio reserve $PUBLIC_KEY
cleos create account eosio network $PUBLIC_KEY

#deploy token with eosio development key
cleos create account eosio eosio.token EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
#disable to skip compilation
#cd contracts/Token/ ; eosio-cpp -o eosio.token.wasm eosio.token.cpp --abigen; cd ../../
cleos set contract eosio.token contracts/Token eosio.token.wasm --abi eosio.token.abi -p eosio.token@active
cleos push action eosio.token create '[ "eosio", "1000000000.0000 SYS"]' -p eosio.token@active
cleos push action eosio.token issue '[ "alice", "100.0000 SYS", "memo" ]' -p eosio@active
cleos push action eosio.token issue '[ "reserve", "100.0000 SYS", "memo" ]' -p eosio@active

cleos push action eosio.token create '[ "eosio", "1000000000.0000 EOS"]' -p eosio.token@active
cleos push action eosio.token issue '[ "alice", "100.0000 EOS", "memo" ]' -p eosio@active
cleos push action eosio.token issue '[ "reserve", "69.3000 EOS", "memo" ]' -p eosio@active
cleos push action eosio.token issue '[ "network", "100.0000 EOS", "memo" ]' -p eosio@active

#deploy reserve
cleos set account permission reserve active "{\"threshold\": 1, \"keys\":[{\"key\":\"$PUBLIC_KEY\", \"weight\":1}] , \"accounts\":[{\"permission\":{\"actor\":\"reserve\",\"permission\":\"eosio.code\"},\"weight\":1}], \"waits\":[] }" owner -p reserve
#disable to skip compilation:
#cd contracts/Reserve/AmmReserve ; eosio-cpp -o AmmReserve.wasm AmmReserve.cpp --abigen ; cd ../../..
cleos set contract reserve contracts/Reserve/AmmReserve AmmReserve.wasm -p reserve@active

#double r, double p_min, asset  max_eos_cap_buy, asset  max_eos_cap_sell, double fee_percent, double max_sell_rate, double min_sell_rate
cleos push action reserve setparams '[ "0.01", "0.05", "20.0000 EOS", "20.0000 EOS", "0.25", "0.5555", "0.00000555" ]' -p reserve@active

#account_name network_contract, asset token_asset, account_name token_contract, account_name eos_contract, bool trade_enabled
cleos push action reserve init '["network", "0.0000 SYS", "eosio.token", "eosio.token", true ]' -p reserve@active

cleos push action reserve enabletrade '[ ]' -p reserve@active

#get conversion rate for buy
cleos push action reserve getconvrate '[ "0.0100 EOS"]' -p network@active
cleos get table reserve reserve rate

#get conversion rate for sell
cleos push action reserve getconvrate '[ "1.000 SYS"]' -p network@active
cleos get table reserve reserve rate

#do a trade (send back to alice)
cleos push action eosio.token transfer '[ "network", "reserve", "0.0100 EOS", "alice,9.97596942734602443" ]' -p network@active
