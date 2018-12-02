const networkAccount = "network"
const reserveAccount = "reserve"
//TODO: not constant, need to read token contract per different token:
const tokenAccount = "eosio.token"

/////////// exported accounts /////////// 

module.exports.reserveAccount = reserveAccount
module.exports.tokenAccount = tokenAccount
module.exports.networkAccount = tokenAccount

/////////// exported function function /////////// 

module.exports.getMatchingAmount = async function(options) {
    let eos = options.eos
    let srcSymbol = options.srcSymbol
    let destSymbol = options.destSymbol
    let amount = options.amount

    if (amount == 0) {
        return -1;
    }

    let params = await getParams(eos);
    let r = parseFloat(params["rows"][0]["r"]);
    let pmin = parseFloat(params["rows"][0]["p_min"]);
    let reserveEos = await getReserveEos(eos);

    if (srcSymbol == "EOS") {
        return -1.0 *calcDeltaT(r, pmin, amount, reserveEos);
    } else if (srcSymbol == "SYS") {
        return -1.0 * calcDeltaE(r, pmin, amount, reserveEos);
    } else {
        return -1;
    }
}

module.exports.getUserBalance = async function(options){
    let eos = options.eos
    let account = options.account
    let symbol = options.symbol

    let balanceRes = await eos.getCurrencyBalance({
        code: tokenAccount,
        account: account,
        symbol: symbol}
    )
    return parseFloat(balanceRes[0]);
}

module.exports.getRate = async function(options) {
    eos = options.eos
    srcSymbol = options.srcSymbol
    destSymbol = options.destSymbol
    srcAmount = options.srcAmount

    let params = await getParams(eos);
    let r = parseFloat(params["rows"][0]["r"]);
    let pmin = parseFloat(params["rows"][0]["p_min"]);
    let reserveEos = await getReserveEos(eos);

    if (srcSymbol == "EOS") {
        if (srcAmount == 0) {
            return pOfE(r, pmin, reserveEos)
        } else {
            return -1.0 / priceForDeltaE(r, pmin, srcAmount, reserveEos)
        }
    } else {
        if (srcAmount == 0) {
            return pOfE(r, pmin, reserveEos) // TODO: divide 1/?
        } else {
            return priceForDeltaT(r, pmin, srcAmount, reserveEos)
        }
    }
}

module.exports.trade = async function(options) {
    let eos = options.eos
    let userAccount = options.userAccount 
    let srcAmount = options.srcAmount
    let srcPrecision = options.srcPrecision
    let srcAccount = options.srcAccount
    let srcSymbol = options.srcSymbol
    let destPrecision = options.destPrecision
    let destSymbol = options.destSymbol
    let destAccount = options.destAccount
    let maxDestAmount = options.maxDestAmount
    let minConversionRate = options.minConversionRate
    let walletId = options.walletId
    let hint = options.hint

    let memo = `${srcAccount},${tokenAccount},${srcPrecision} ${srcSymbol},` +
               `${tokenAccount},${destPrecision} ${destSymbol},${destAccount},` +
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

/////////// internal function /////////// 

function pOfE(r, pMin, curE) { 
    return pMin * Math.exp(r * curE); 
}

function buyPriceForZeroQuant(r, pMin, curE) { 
    let pOfERes = pOfE(r, pMin, curE);
    let buyPrice = 1.0 / pOfE(r, pMin, curE);
    return buyPrice;
}

function sellPriceForZeroQuant(r, pMin, curE) { 
    return pOfE(r, pMin, curE);
}

function calcDeltaT(r, pMin, deltaE, curE) {
    return (Math.exp((-r) * deltaE) - 1.0) / (r * pOfE(r, pMin, curE));
}

function calcDeltaE(r, pMin, deltaT, curE) {
    return (-1) * Math.log(1.0 + r * pOfE(r, pMin, curE) * deltaT) / r;
}

function priceForDeltaE(r, pMin, deltaE, curE) { 
    let deltaT = calcDeltaT(r, pMin, deltaE, curE);
    return deltaT / deltaE;
}

function priceForDeltaT(r, pMin, deltaT, curE) {
    let deltaE = calcDeltaE(r, pMin, deltaT, curE);
    return -deltaE / deltaT;
}

async function getParams(eos) {
    let params = await eos.getTableRows({
        code: reserveAccount,
        scope:reserveAccount,
        table:"params",
        json: true
    })

    return params;
}

async function getReserveEos(eos) {
    let balanceRes = await eos.getCurrencyBalance({
        code: tokenAccount,
        account: reserveAccount,
        symbol: 'EOS'}
    )
    let reserveEos = parseFloat(balanceRes[0])

    return reserveEos; 
}