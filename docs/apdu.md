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

The default values for the Aergo app are:

| purpose    | coin_type  | account    | change  | address_index |
|------------|------------|------------|------------|------------|
| 0x8000002C | 0x800001B9 | 0x80000000 | 0x00000000 | 0x00000000 |

The `coin_type` above is registered for Aergo.

You can use the default values or any other BIP32 path.

More about [BIP44](https://github.com/bitcoin/bips/blob/master/bip-0044.mediawiki).


## Implemented APDUs:

| CLA | INS | COMMAND NAME        | DESCRIPTION |
|-----|-----|---------------------|-------------|
|  AE |  01 | GET_APP_VERSION     | Return the version of Ledger app |
|  AE |  02 | GET_PUBLIC_KEY      | Return the compressed public key for the given BIP44 path |
|  AE |  03 | DISPLAY_ADDRESS     | Display the account address on the device screen |
|  AE |  04 | SIGN_TRANSACTION    | Sign a transaction |
|  AE |  08 | SIGN_MESSAGE        | Sign a message |


### 1. Get App Version

This command returns the version of Ledger app.

***Command***

| *CLA* | *INS*  | *P1* | *P2* | *Lc* | *Le* |
|-------|--------|------|------|------|------|
| 0xAE  |  0x01  |   0  |   0  |   0  |      |

***Output data***

| *Description*       | *Length* |
|---------------------|----------|
| App Major version   |    1     |
| App Minor version   |    1     |


### 2. Get Public Key

This command returns the compressed public key for the given BIP44 path.

***Command***

| *CLA* | *INS*  | *P1* | *P2* | *Lc* | *Le* |
|-------|--------|------|------|------|------|
| 0xAE  |  0x02  |   0  |   0  |   N  |      |

***Input data***

| *Description*    | *Length*  |
|------------------|-----------|
| BIP44 address    |     N     |

***Output data***

| *Description* | *Length*  |
|---------------|-----------|
| Public key    |    33     |


### 3. Display Account Address

This command is used to display the account address on the device screen.

The client application must first select the account using the `Get Public Key` command.

***Command***

| *CLA* | *INS*  | *P1* | *P2* | *Lc* | *Le* |
|-------|--------|------|------|------|------|
| 0xAE  |  0x03  |   0  |   0  |   0  |      |

***Input data***

none

***Output data***

none


### 4. Sign Transaction

This command gets a raw transaction as an input and returns the transaction hash and the signature

**Important:** This command does not accept a BIP44 path. We need to call "Get Public Key" first so the path will be stored and that account will be used for signing

The maximum data packet size is 250 bytes. So, if the transaction is bigger than this it must be split in chunks.

***Command***

| *CLA* | *INS*  | *P1* | *P2* | *Lc* | *Le* |
|-------|--------|------|------|------|------|
| 0xAE  |  0x04  |  P1  | 0x00 |   N  |      |

***P1***

| *Description*    | *Value*  |
|------------------|----------|
| First Txn Part   |   0x01   |
| Last Txn Part    |   0x02   |

If the transaction fits into a single packet then P1 should be `0x03`

***Input data***

| *Description*    | *Length* |
|------------------|----------|
| Transaction part |    N     |

If the transaction size is < 250, then it can be sent entirely in a single packet.

Otherwise each chunk must be sent in a separate APDU.

`N` is the size of the data being sent.

***Output data***

| *Description* | *Length*  |
|---------------|-----------|
| Txn Hash      |    32     |
| Signature     | variable  |
  
If the signing process is canceled by the user then we get **0x6982** as an error status code


### 5. Sign Message

This command gets a message as an input and returns the message hash and the signature

**Important:** This command does not accept a BIP44 path. We need to call "Get Public Key" first so the path will be stored and that account will be used for signing

The maximum message size is 250 bytes.

***Command***

| *CLA* | *INS*  | *P1* | *P2* | *Lc* | *Le* |
|-------|--------|------|------|------|------|
| 0xAE  |  0x08  | 0x00 | 0x00 |   N  |      |

***Input data***

| *Description*    | *Length* |
|------------------|----------|
| Transaction part |    N     |

`N` is the size of the data being sent.

***Output data***

| *Description* | *Length*  |
|---------------|-----------|
| Message Hash  |    32     |
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


# SW Codes

| CODE   | NAME | DESCRIPTION |
|--------|------|-------------|
| 0x9000 | SW_OK | success |
| 0x9001 | SW_RESEND_FIRST_PART | request to re-send the first transaction part |
| 0x6982 | SW_REJECTED | rejected by the user |
| 0x6A87 | SW_WRONG_DATA_LENGTH | wrong length of APDU request |
| 0x6700 | SW_WRONG_LENGTH | wrong length of APDU Lc field |
| 0x6E00 | SW_CLA_NOT_SUPPORTED | invalid CLA |
| 0x6D00 | SW_INS_NOT_SUPPORTED | invalid INS |
| 0x6985 | SW_INVALID_STATE | invalid state |
| 0x6720 - 0x6732 | | invalid transaction data - parsing |
| 0x6740 - 0x6755 | | invalid transaction data - selection |
| 0x6735 | SW_TXN_INCOMPLETE | the transaction is incomplete |
