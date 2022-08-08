from hashlib import sha256
from sha3 import keccak_256

from ecdsa.curves import SECP256k1
from ecdsa.keys import VerifyingKey
from ecdsa.util import sigdecode_der


def test_sign_tx(cmd, model):
    bip32_path: str = "m/44'/0'/0'/0/0"

    pub_key = cmd.get_public_key(
        bip32_path=bip32_path,
        display=False
    )  # type: bytes, bytes

    pk: VerifyingKey = VerifyingKey.from_string(
        pub_key,
        curve=SECP256k1,
        hashfunc=sha256
    )

    tx = b"\x04\x08\x0a\x12\x21\x02\x9d\x02\x05\x91\xe7\xfb\x7b\x09\x21\x53\x68\x19\x95\xf8\x06\x09\xf0\xac\x98\x8a\x4d\x93\x5e\x0e\xa6\x3c\x06\x0f\x19\x54\xb0\x5f\x1a\x21\x03\x8c\xb9\x2c\xde\xbf\x39\x98\x69\x09\x3c\xac\x47\xe3\x70\xd8\xa9\xfa\x50\x17\x30\x42\x23\xf9\xad\x1a\x8c\x0a\x05\xa9\x06\xa9\xcb\x22\x08\x14\xd1\x12\x0d\x7b\x16\x00\x00\x3a\x01\x00\x40\x04\x4a\x20\x52\x48\x45\xc2\x4c\xd3\xe5\x3a\xec\xbc\xda\x8e\x31\x5d\x62\xdc\x95\xa7\xf2\xf8\x25\x48\x93\x0b\xc2\xfc\xc9\x86\xbf\x74\x53\xbd"
    print(tx)

    txn_hash, signature = cmd.sign_tx(transaction=tx, model=model)

    assert pk.verify_digest(signature=signature,
                            digest=txn_hash,
                            sigdecode=sigdecode_der) is True