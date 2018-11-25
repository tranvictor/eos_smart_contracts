const fs = require('fs')
const Eos = require('eosjs')
const services = require('./services')

const privateKeyDefault = "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"
const publicKeyDefault = "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"
const privateKey = "5JeKxP8eTUB9pU2jMKHBJB6cQc16VpMntZoeYz5Fv65wRyPNkZN"
const publicKey = "EOS5CYr5DvRPZvfpsUGrQ2SnHeywQn66iSbKKXn4JDTbFFr36TRTX"

const aliceAccount = "alice"
const mosheAccount = "moshe"
const networkAccount = "network"

const eos = Eos( {keyProvider: [ privateKeyDefault, privateKey ] /* , verbose: 'false' */ } )

async function main(){

    // example for getUserBalance
    const balanceBefore = await services.getUserBalance(eos, mosheAccount, 'SYS')
    console.log("moshe balance before", balanceBefore)
    
    // example for getMatchingAmount
    const expectedDestAmount = await services.getMatchingAmount(eos, 'EOS', 'SYS', 0.0100)
    console.log("expected destAmount", expectedDestAmount)

    // example for getRate
    const rate = await services.getRate(eos, 'EOS', 'SYS', 0.0100)
    console.log("expected rate", rate)

    // example for trade with network
    await services.trade(
        eos,
        aliceAccount, 
        "0.0100",
        4,
        "alice",
        "EOS",
        4,
        "SYS",
        "moshe",
        20,
        "0.000001",
        "some_wallet_id",
        "some_hint"
    )

    const balanceAfter = await services.getUserBalance(eos, mosheAccount, 'SYS')
    console.log("moshe balance after", balanceAfter) 
}

main()