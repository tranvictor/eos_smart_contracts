//const { ensureContractAssertionError, getEos, snooze } = require('./utils');
//const { ERRORS } = require('./constants');
//const assert = require('assert')

const fs = require('fs')
const Eos = require('eosjs')
const services = require('../scripts/networkServices')
const BigNumber = require('bignumber.js');
const path = require('path');

//const assert = require('chai').should();
const {
    ensureContractAssertionError,
    getEos,
    snooze
} = require('./utils');
//const { ERRORS } = require('./constants');

/*  Assign keypair to accounts.
 *  Unique account prefixes are important to prevent collisions between test modules. */
const keyPairArray = JSON.parse(fs.readFileSync("scripts/keys.json"))
const tokenData =   {account: "ammtoken",   publicKey: keyPairArray[0][0], privateKey: keyPairArray[0][1]}
const reserveData = {account: "ammreserve", publicKey: keyPairArray[1][0], privateKey: keyPairArray[1][1]}

/* create eos handler objects */
tokenData.eos = Eos({ keyProvider: tokenData.privateKey /* , verbose: 'false' */})
reserveData.eos = Eos({ keyProvider: reserveData.privateKey /* , verbose: 'false' */})

/* create accounts */
let createObject = {creator: "eosio", name:tokenData.account, owner: tokenData.publicKey, active: systemData.publicKey}

async function createAccounts() {
    await systemData.eos.transaction(tr => {tr.newaccount(
        {creator: "eosio", name:tokenData.account, owner: tokenData.publicKey, active: tokenData.publicKey}
    )});

    await systemData.eos.transaction(tr => {tr.newaccount(
        {creator: "eosio", name:reserveData.account, owner: reserveData.publicKey, active: reserveData.publicKey}
    )});
}
createAccounts();

/* deploy contracts */



beforeEach('create contracts', async function () {
    /* init reserve */
    /* set params */
});

describe('As owner', () => {

    /*
    const rerouter = await getEos(testUser).contract(rerouteContract);
    const p = rerouter.reroutetx({tx_id: 555,
        blockchain: 'eth',
        target: '0x666'
        },{authorization: `${testUser}@active`});
    await ensureContractAssertionError(p, ERRORS.SINGLETON_DOESNT_EXIST);
    */
    //const network = await eos.contract()

    xit('init a reserve', async function() {});
    xit('init can not be called twice', async function() {});
    xit('set params', async function() {});
    xit('set network', async function() {});
    xit('enable trade', async function() {});
    xit('disable trade', async function() {});
    xit('reset fee', async function() {});
    it('get buy rate with 0 quantity', async function() {});
    it('get buy rate with non 0 quantity', async function() {});
    it('get sell rate with 0 quantity', async function() {});
    it('get sell rate with non 0 quantity', async function() {});
});

describe('as non owner', () => {
    it('can not init a reserve', async function() {});
    xit('can not init set params', async function() {});
    xit('can not set network', async function() {});
    xit('can not enable trade', async function() {});
    xit('can not disable trade', async function() {});
    xit('can not reset fee', async function() {});
    it('can not get conversion rate', async function() {});
});

describe('as anyone', () => {
    it('buy with dest amount < max dest amount', async function() {});
    it('buy with dest amount > max dest amount', async function() {});
    it('sell with dest amount < max dest amount', async function() {});
    it('sell with dest amount > max dest amount', async function() {});
    xit('buy with rate > max_buy_rate', async function() {});
    xit('buy with rate < min_buy_rate', async function() {});
    xit('sell with rate > max_buy_rate', async function() {});
    xit('sell with rate < min_buy_rate', async function() {});
    xit('fee is kept on buy', async function() {});
    xit('fee is kept on sell', async function() {});
    


});
