#cleos wallet unlock --password XXXXXXXXXXXXXXXXXXXX...

#in another shell:
#rm -rf ~/.local/share/eosio/nodeos/data
#nodeos -e -p eosio --plugin eosio::chain_api_plugin --plugin eosio::history_api_plugin --contracts-console --verbose-http-errors

set -x

#tmp disable
#rm contracts/Token/*.wasm contracts/Token/*.abi contracts/Network/*.wasm contracts/Network/*.abi 

PUBLIC_KEY=EOS5CYr5DvRPZvfpsUGrQ2SnHeywQn66iSbKKXn4JDTbFFr36TRTX
cleos create account eosio moshe $PUBLIC_KEY
cleos create account eosio network $PUBLIC_KEY
cleos create account eosio reserve1 $PUBLIC_KEY
cleos create account eosio reserve2 $PUBLIC_KEY
cleos create account eosio reserve3 $PUBLIC_KEY

#move some tokens to network just for testing
cleos push action eosio.token issue '[ "network", "10.0000 SYS", "memo" ]' -p eosio@active

#deploy network
cleos set account permission network active "{\"threshold\": 1, \"keys\":[{\"key\":\"$PUBLIC_KEY\", \"weight\":1}] , \"accounts\":[{\"permission\":{\"actor\":\"network\",\"permission\":\"eosio.code\"},\"weight\":1}], \"waits\":[] }" owner -p network
cd contracts/Network/

#disable to skip compilation:
#eosio-cpp -o Network.wasm Network.cpp --abigen

#fix abi file since in current version of abigen vectors are not processed correctly
sed -i -e 's/vector<account_name>/uint64[]/g' Network.abi
cd ../../ 
cleos set contract network contracts/Network Network.wasm -p network@active

cleos push action network setenable '[ true ]' -p network@active
cleos push action network addreserve '[ "reserve1", true ]' -p network@active
cleos push action network addreserve '[ "reserve2", true ]' -p network@active
cleos push action network addreserve '[ "reserve3", true ]' -p network@active
cleos push action network addreserve '[ "reserve3", false ]' -p network@active

cleos push action network addreserve '[ "reserve", true ]' -p network@active

cleos push action network listpairres '[ "reserve", "0.0000 SYS", "eosio.token", true, true, true ]' -p network@active
cleos push action network listpairres '[ "reserve1", "0.0000 SYS", "eosio.token", true, true, true ]' -p network@active
cleos push action network listpairres '[ "reserve2", "0.0000 SYS", "eosio.token", true, true, true ]' -p network@active

cleos get table network network reservespert


cleos get table reserve reserve rate
cleos push action eosio.token transfer '[ "alice", "network", "0.0100 SYS", "alice,eosio.token,4 SYS,eosio.token,4 EOS,moshe,20,0.000001,some_wallet_id,some_hint" ]' -p alice@active
cleos get table reserve reserve rate
