const fs = require('fs')
const Eos = require('eosjs')
const services = require('./networkServices')
const BigNumber = require('bignumber.js');


const privateKeyDefault = "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"
const publicKeyDefault = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"
const privateKey = "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN"
const publicKey = "EOS5CYr5DvRPZvfpsUGrQ2SnHeywQn66iSbKKXn4JDTbFFr36TRTX"

const aliceAccount = "alice"
const mosheAccount = "moshe"
const networkAccount = "network"

const eos = Eos( {keyProvider: [ privateKeyDefault, privateKey ] /* , verbose: 'false' */ } )

async function main(){

    const balances = await services.getBalances({
        eos:eos,
        reserveAccount:"reserve",
        tokenSymbols:["SYS","EOS"],
        tokenContracts:["eosio.token","eosio.token"]
    })
    console.log(balances)

    const enabled = await services.getEnabled(eos);
    console.log(enabled)

    /* TODO: missing slippageRate calculation */

    let rate = await services.getRate({eos:eos, srcSymbol:'EOS', destSymbol:'SYS', srcAmount:0.0100})
    console.log(rate)

    rate = await services.getRate({eos:eos, srcSymbol:'EOS', destSymbol:'SYS', srcAmount:0})
    console.log(rate)

    const rates = await services.getRates({
        eos:eos,
        srcSymbols:['EOS','SYS'],
        destSymbols:['SYS','EOS'],
        srcAmounts:[0.0100, 0.0100]
    })
    console.log(rates)

    const balanceBefore = await services.getUserBalance({
        eos:eos,
        account:mosheAccount,
        symbol:'SYS',
        tokenContract:"eosio.token"
    })
    console.log("moshe SYS balance before ", balanceBefore)

    await services.trade({
        eos: eos,
        userAccount: aliceAccount, 
        srcAmount: "0.0100",
        srcPrecision: 4,
        srcTokenAccount: "eosio.token",
        srcSymbol: "EOS",
        destPrecision: 4,
        destSymbol: "SYS",
        destAccount: "moshe",
        destTokenAccount: "eosio.token",
        maxDestAmount: 200000,
        minConversionRate: "0.000001",
        walletId: "somewallid",
        hint: "somehint"
    })

    const balanceAfter = await services.getUserBalance({
        eos:eos,
        account:mosheAccount,
        symbol:'SYS',
        tokenContract:"eosio.token"
    })
    console.log("moshe SYS balance after ", balanceAfter)

}

main()