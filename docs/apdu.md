# APDU (Application Protocol Data Unit)

In the Ledger app, each command/response packet is called an APDU. The application is driven by a never-ending APDU interpretation loop called straight from the application main function.

More about APDU:

[Wikipedia](https://en.wikipedia.org/wiki/Smart_card_application_protocol_data_unit)

[Ledger app structure and I/O](https://ledger.readthedocs.io/en/latest/userspace/application_structure.html) 

## BIP44 path

The BIP44 address consists of 5 levels, 4 bytes each.

```
m / purpose' / coin_type' / account' / change / address_index
```

The Ledger app expects the path to be serialized as big-endian integers

The default values are:

| purpose    | coin_type  | account    | change  | address_index |
|------------|------------|------------|------------|------------|
| 0x8000002C | 0x800001B9 | 0x80000000 | 0x00000000 | 0x00000000 |

The coin_type above is registered for Aergo.

You can use the default values or any other BIP32 path.

More about [BIP44](https://github.com/bitcoin/bips/blob/master/bip-0044.mediawiki).


## Implemented APDUs:

### 1. Get App Version

This command returns the version of Ledger app.

***Description***:

- **Command**

| *CLA* | *INS*  | *P1* | *P2* | *Lc* | *Le* |
|-------|--------|------|------|------|------|
| 0xAE  |  0x01  |   0  |   0  |   0  |      |

- **Output data**

| *Description*       | *Length* |
|---------------------|----------|
| App Major version   |    1     |
| App Minor version   |    1     |


### 2. Get Public Key

This command returns the compressed public key for the given BIP44 path.

***Description***:

- **Command**

| *CLA* | *INS*  | *P1* | *P2* | *Lc* | *Le* |
|-------|--------|------|------|------|------|
| 0xAE  |  0x02  |   0  |   0  |   N  |      |

- **Input data**

| *Description*    | *Length*  |
|------------------|-----------|
| BIP44 address    |     N     |

- **Output data**

| *Description* | *Length*  |
|---------------|-----------|
| Public key    |    33     |


### 3. Display Account Address

This command is used to display the account address on the device screen.

The client application must first select the account using the `Get Public Key` command.

***Description***:

- **Command**

| *CLA* | *INS*  | *P1* | *P2* | *Lc* | *Le* |
|-------|--------|------|------|------|------|
| 0xAE  |  0x03  |   0  |   0  |   0  |      |

- **Input data**

none

- **Output data**

none


### 4. Sign Transaction

This command gets a raw transaction as an input and returns the transaction hash and the signature

**Important:** This command does not accept a BIP44 path. We need to call "Get Public Key" first so the path will be stored and that account will be used for signing

The maximum data packet size is 250 bytes. So, if the transaction is bigger than this it must be split in chunks.

***Description***:

- **Command**

| *CLA* | *INS*  | *P1* | *P2* | *Lc* | *Le* |
|-------|--------|------|------|------|------|
| 0xAE  |  0x04  |  P1  | 0x00 |   N  |      |

- **P1**

| *Description*    | *Value*  |
|------------------|----------|
| First Txn Part   |   0x01   |
| Last Txn Part    |   0x02   |

If the transaction fits into a single packet then P1 should be `0x03`

- **Input data**

| *Description*    | *Length* |
|------------------|----------|
| Transaction part |    N     |

If the transaction size is < 250, then it can be sent entirely in a single packet.

Otherwise each chunk must be sent in a separate APDU.

`N` is the size of the data being sent.

- **Output data**

| *Description* | *Length*  |
|---------------|-----------|
| Txn Hash      |    32     |
| Signature     | variable  |
  
If the signing process is canceled by the user then we get **0x6982** as an error status code



## Example of ADPU call

Let's get an account address from the Ledger app using the BIP44 path `8000002C / 800001B9 / 80000000 / 00000000/ 00000000`

| *CLA* | *INS* | *P1* | *P2* | *Lc* |               *Data field*               |
|-------|-------|------|------|------|------------------------------------------|
| 0xAE  | 0x02  | 0x00 | 0x00 | 0x14 | 8000002C800001B9800000000000000000000000 |

So, our request will be:

```
AE020000148000002C800001B9800000000000000000000000
```

From the Ledger app we will get the public key that must be converted to the address using base58check
