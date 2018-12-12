set -x
rm contracts/Token/*.wasm contracts/Token/*.abi contracts/Reserve/AmmReserve/*.wasm contracts/Reserve/AmmReserve/*.abi
cd contracts/Token/ ; eosio-cpp -o Token.wasm Token.cpp --abigen; cd ../../
cd contracts/Reserve/AmmReserve ; eosio-cpp -o AmmReserve.wasm AmmReserve.cpp --abigen ; cd ../../..
cd contracts/Network/ ; eosio-cpp -o Network.wasm Network.cpp --abigen ; cd ../../