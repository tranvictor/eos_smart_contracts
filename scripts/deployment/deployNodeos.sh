#cleos wallet unlock --password XXXXXXXXXXXXXXXXXXXX...

#in another shell:
#rm -rf ~/.local/share/eosio/nodeos/data
#nodeos -e -p eosio --plugin eosio::chain_api_plugin --plugin eosio::history_api_plugin --contracts-console --verbose-http-errors

set -x

PUBLIC_KEY=EOS5CYr5DvRPZvfpsUGrQ2SnHeywQn66iSbKKXn4JDTbFFr36TRTX
EOSIO_DEV_KEY=EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

cleos create account eosio eosio.token $EOSIO_DEV_KEY # deploy token with eosio development key
cleos create account eosio reserve $PUBLIC_KEY
cleos create account eosio resowner $PUBLIC_KEY
cleos create account eosio network $PUBLIC_KEY
cleos create account eosio netowner $PUBLIC_KEY
cleos create account eosio reserve1 $PUBLIC_KEY
cleos create account eosio reserve2 $PUBLIC_KEY
cleos create account eosio reserve3 $PUBLIC_KEY
cleos create account eosio alice $PUBLIC_KEY
cleos create account eosio moshe $PUBLIC_KEY

cleos set contract eosio.token contracts/Token Token.wasm --abi Token.abi -p eosio.token@active

#spread initial funds
cleos push action eosio.token create '[ "eosio", "1000000000.0000 SYS"]' -p eosio.token@active
cleos push action eosio.token issue '[ "alice", "100.0000 SYS", "memo" ]' -p eosio@active
cleos push action eosio.token issue '[ "reserve", "100.0000 SYS", "deposit" ]' -p eosio@active

cleos push action eosio.token create '[ "eosio", "1000000000.0000 EOS"]' -p eosio.token@active
cleos push action eosio.token issue '[ "alice", "100.0000 EOS", "memo" ]' -p eosio@active
cleos push action eosio.token issue '[ "network", "100.0000 EOS", "memo" ]' -p eosio@active
cleos push action eosio.token issue '[ "reserve", "69.3000 EOS", "deposit" ]' -p eosio@active

#deploy reserve
cleos set account permission reserve active "{\"threshold\": 1, \"keys\":[{\"key\":\"$PUBLIC_KEY\", \"weight\":1}] , \"accounts\":[{\"permission\":{\"actor\":\"reserve\",\"permission\":\"eosio.code\"},\"weight\":1}], \"waits\":[] }" owner -p reserve

cleos set contract reserve contracts/Reserve/AmmReserve AmmReserve.wasm -p reserve@active

#account_name network_contract owner, account_name network_contract, asset token_asset, account_name token_contract, account_name eos_contract, bool trade_enabled
cleos push action reserve init '["resowner", "network", "0.0000 SYS", "eosio.token", "eosio.token", true ]' -p reserve@active
#double r, double p_min, asset  max_eos_cap_buy, asset  max_eos_cap_sell, double fee_percent, double max_sell_rate, double min_sell_rate
cleos push action reserve setparams '[ "0.01", "0.05", "20.0000 EOS", "20.0000 EOS", "0.25", "0.5555", "0.00000555" ]' -p resowner@active
cleos push action reserve enabletrade '[ ]' -p resowner@active

#get conversion rate for buy
cleos push action reserve getconvrate '[ "0.0100 EOS"]' -p network@active
cleos get table reserve reserve rate

#get conversion rate for sell
cleos push action reserve getconvrate '[ "1.000 SYS"]' -p network@active
cleos get table reserve reserve rate

#do a trade (send back to alice)
cleos push action eosio.token transfer '[ "network", "reserve", "0.0100 EOS", "alice" ]' -p network@active

####################################################################

#deploy network
cleos set account permission network active "{\"threshold\": 1, \"keys\":[{\"key\":\"$PUBLIC_KEY\", \"weight\":1}] , \"accounts\":[{\"permission\":{\"actor\":\"network\",\"permission\":\"eosio.code\"},\"weight\":1}], \"waits\":[] }" owner -p network
cleos set contract network contracts/Network Network.wasm -p network@active

cleos push action network init '[ "netowner", true ]' -p network@active
cleos push action network addreserve '[ "reserve1", true ]' -p netowner@active
cleos push action network addreserve '[ "reserve2", true ]' -p netowner@active
cleos push action network addreserve '[ "reserve3", true ]' -p netowner@active
cleos push action network addreserve '[ "reserve3", false ]' -p netowner@active

cleos push action network addreserve '[ "reserve", true ]' -p netowner@active

cleos push action network listpairres '[ "reserve", "0.0000 SYS", "eosio.token", true ]' -p netowner@active
cleos push action network listpairres '[ "reserve1", "0.0000 SYS", "eosio.token", true ]' -p netowner@active
cleos push action network listpairres '[ "reserve2", "0.0000 SYS", "eosio.token", true ]' -p netowner@active

cleos push action network listpairres '[ "reserve1", "0.0000 SYS", "eosio.token", false ]' -p netowner@active
cleos push action network listpairres '[ "reserve2", "0.0000 SYS", "eosio.token", false ]' -p netowner@active

cleos get table network network reservespert
cleos get table reserve reserve rate
cleos push action eosio.token transfer '[ "alice", "network", "0.0100 SYS", "alice,eosio.token,4 SYS,eosio.token,4 EOS,moshe,20,0.000001,somewallid,somehint" ]' -p alice@active
cleos get table reserve reserve rate