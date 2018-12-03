const reserveServices = require('./ammReserveServices')
const networkAccount = "network"
const eosTokenAccount = "eosio.token"

module.exports.getBalances = async function(options){
    let eos = options.eos
    let reserveAccount = options.reserveAccount
    let tokenSymbols = options.tokenSymbols
    let tokenContracts = options.tokenContracts

    let balances = []
    let arrayLength = tokenSymbols.length;
    for (var i = 0; i < arrayLength; i++) {
        let balanceRes = await eos.getCurrencyBalance({
            code: tokenContracts[i],
            account: reserveAccount,
            symbol: tokenSymbols[i]}
        )
        balances.push(parseFloat(balanceRes[0]))
    }
    return balances
}

module.exports.getEnabled = async function(eos){
    let state = await eos.getTableRows({
        code: networkAccount,
        scope:networkAccount,
        table:"state",
        json: true
    })
    return state.rows[0].is_enabled
}

module.exports.getRate = async function(options) {
    // TODO: missing slippageRate calculation and handling

    eos = options.eos
    srcSymbol = options.srcSymbol
    destSymbol = options.destSymbol
    srcAmount = options.srcAmount

    let reservesReply = await eos.getTableRows({
        code: networkAccount,
        scope:networkAccount,
        table:"reservespert",
        json: true
    })

    let bestRate = 0
    let reserveContractsList = reservesReply.rows[0].reserve_contracts
    let arrayLength = reservesReply.rows[0].num_reserves
    for (var i = 0; i < arrayLength; i++) {
        reserveName = reserveContractsList[i];
        reserveStr = eos.modules.format.decodeName(reserveName, false)
        currentRate = await reserveServices.getRate({
            eos:eos,
            reserveAccount:reserveStr,
            eosTokenAccount:eosTokenAccount,
            srcSymbol:srcSymbol,
            destSymbol:destSymbol,
            srcAmount:srcAmount
        })
        if(currentRate > bestRate) {
            bestRate = currentRate
        }
    }
    return bestRate
}

module.exports.getRates = async function(options) {
    // TODO: missing slippageRate handling

    eos = options.eos
    srcSymbols = options.srcSymbols
    destSymbols = options.destSymbols
    srcAmounts = options.srcAmounts

    let arrayLength = srcSymbols.length
    let ratesArray = []
    for (var i = 0; i < arrayLength; i++) {
        rate = await this.getRate({
            eos:eos,
            srcSymbol:srcSymbols[i],
            destSymbol:destSymbols[i],
            srcAmount:srcAmounts[i]
        })
        ratesArray.push(rate)
    }
    return ratesArray
}

module.exports.trade = async function(options) {
    let eos = options.eos
    let userAccount = options.userAccount 
    let srcAmount = options.srcAmount
    let srcPrecision = options.srcPrecision
    let srcTokenAccount = options.srcTokenAccount
    let srcSymbol = options.srcSymbol
    let destPrecision = options.destPrecision
    let destSymbol = options.destSymbol
    let destTokenAccount = options.destTokenAccount
    let destAccount = options.destAccount
    let maxDestAmount = options.maxDestAmount
    let minConversionRate = options.minConversionRate
    let walletId = options.walletId
    let hint = options.hint

//    "alice,eosio.token,4 SYS,eosio.token,4 EOS,moshe,20,0.000001,some_wallet_id,some_hint" ]' -p alice@active

    let memo = `${userAccount},${srcTokenAccount},${srcPrecision} ${srcSymbol},` +
               `${destTokenAccount},${destPrecision} ${destSymbol},${destAccount},` +
               `${maxDestAmount},${minConversionRate},${walletId},${hint}`
    let asset = `${srcAmount} ${srcSymbol}`
    
    console.log(memo)
    console.log(asset)

    await eos.transfer(
            userAccount,
            networkAccount,
            asset, //'0.0100 EOS',
            memo,
            {authorization: userAccount}
    )
}

module.exports.getUserBalance = async function(options){
    let eos = options.eos
    let account = options.account
    let symbol = options.symbol
    let tokenContract = options.tokenContract

    let balanceRes = await eos.getCurrencyBalance({
        code: tokenContract,
        account: account,
        symbol: symbol}
    )
    return parseFloat(balanceRes[0]);
}