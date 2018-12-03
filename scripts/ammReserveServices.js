/////////// exported functions /////////// 

module.exports.getRate = async function(options) {
    eos = options.eos
    reserveAccount = options.reserveAccount
    eosTokenAccount = options.eosTokenAccount
    srcSymbol = options.srcSymbol
    destSymbol = options.destSymbol
    srcAmount = options.srcAmount

    let params = await getParams(eos, reserveAccount);
    let r = parseFloat(params["rows"][0]["r"]);
    let pmin = parseFloat(params["rows"][0]["p_min"]);
    let reserveEos = await getReserveEos(eos, reserveAccount, eosTokenAccount);

    if (srcSymbol == "EOS") {
        if (srcAmount == 0) {
            return 1 / pOfE(r, pmin, reserveEos)
        } else {
            return priceForDeltaE(r, pmin, srcAmount, reserveEos)
        }
    } else {
        if (srcAmount == 0) {
            return pOfE(r, pmin, reserveEos)
        } else {
            return priceForDeltaT(r, pmin, srcAmount, reserveEos)
        }
    }
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
    return (-1) * deltaT / deltaE;
}

function priceForDeltaT(r, pMin, deltaT, curE) {
    let deltaE = calcDeltaE(r, pMin, deltaT, curE);
    return -deltaE / deltaT;
}

async function getParams(eos, reserveAccount) {
    let params = await eos.getTableRows({
        code: reserveAccount,
        scope:reserveAccount,
        table:"params",
        json: true
    })
    return params;
}

async function getReserveEos(eos, reserveAccount, eosTokenAccount) {
    let balanceRes = await eos.getCurrencyBalance({
        code: eosTokenAccount,
        account: reserveAccount,
        symbol: 'EOS'}
    )
    let reserveEos = parseFloat(balanceRes[0])

    return reserveEos; 
}