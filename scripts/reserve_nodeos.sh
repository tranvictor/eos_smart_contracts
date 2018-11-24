#cleos wallet unlock --password XXXXXXXXXXXXXXXXXXXX...

#in another shell:
#rm -rf ~/.local/share/eosio/nodeos/data
#nodeos -e -p eosio --plugin eosio::chain_api_plugin --plugin eosio::history_api_plugin --contracts-console --verbose-http-errors

set -x

#tmp disable
#rm contracts/Token/*.wasm contracts/Token/*.abi contracts/Reserve/AmmReserve/*.wasm contracts/Reserve/AmmReserve/*.abi 

PUBLIC_KEY=EOS5CYr5DvRPZvfpsUGrQ2SnHeywQn66iSbKKXn4JDTbFFr36TRTX
cleos create account eosio alice $PUBLIC_KEY
cleos create account eosio reserve $PUBLIC_KEY

#deploy token with eosio development key
cleos create account eosio eosio.token EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
#tmp disable
cd contracts/Token/ ; eosio-cpp -o eosio.token.wasm eosio.token.cpp --abigen; cd ../../
cleos set contract eosio.token contracts/Token eosio.token.wasm --abi eosio.token.abi -p eosio.token@active
cleos push action eosio.token create '[ "eosio", "1000000000.0000 SYS"]' -p eosio.token@active
cleos push action eosio.token issue '[ "alice", "100.0000 SYS", "memo" ]' -p eosio@active
cleos push action eosio.token issue '[ "reserve", "100.0000 SYS", "memo" ]' -p eosio@active

cleos push action eosio.token create '[ "eosio", "1000000000.0000 EOS"]' -p eosio.token@active
cleos push action eosio.token issue '[ "alice", "100.0000 EOS", "memo" ]' -p eosio@active
cleos push action eosio.token issue '[ "reserve", "70.0000 EOS", "memo" ]' -p eosio@active

#deploy reserve
cleos set account permission reserve active "{\"threshold\": 1, \"keys\":[{\"key\":\"$PUBLIC_KEY\", \"weight\":1}] , \"accounts\":[{\"permission\":{\"actor\":\"reserve\",\"permission\":\"eosio.code\"},\"weight\":1}], \"waits\":[] }" owner -p reserve
#tmp disable
cd contracts/Reserve/AmmReserve ; eosio-cpp -o AmmReserve.wasm AmmReserve.cpp --abigen ; cd ../../..
cleos set contract reserve contracts/Reserve/AmmReserve AmmReserve.wasm -p reserve@active
cleos push action reserve setparams '[ "0.01", "0.05", "0.0000 SYS", "eosio.token", "eosio.token" ]' -p reserve@active
cleos push action reserve enabletrade '[ ]' -p reserve@active

# do a trade (send back to alice)
cleos push action eosio.token transfer '[ "alice", "reserve", "0.0100 EOS", "alice" ]' -p alice@active

# get convrate
cleos get table reserve reserve rate