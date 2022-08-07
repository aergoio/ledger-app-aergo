from hashlib import sha256
from sha3 import keccak_256

from ecdsa.curves import SECP256k1
from ecdsa.keys import VerifyingKey
from ecdsa.util import sigdecode_der


def test_sign_tx(cmd, model):
    bip32_path: str = "m/44'/0'/0'/0/0"

    pub_key, chain_code = cmd.get_public_key(
        bip32_path=bip32_path,
        display=False
    )  # type: bytes, bytes

    pk: VerifyingKey = VerifyingKey.from_string(
        pub_key,
        curve=SECP256k1,
        hashfunc=sha256
    )

    tx = b"".join(...)

    v, der_sig = cmd.sign_tx(bip32_path=bip32_path,
                             transaction=tx, model=model)

    assert pk.verify(signature=der_sig,
                     data=tx,
                     hashfunc=keccak_256,
                     sigdecode=sigdecode_der) is True
