def test_get_public_key(cmd):
    pub_key = cmd.get_public_key(
        bip32_path="m/44'/0'/0'/0/0",
        display=False
    )  # type: bytes, bytes

    assert len(pub_key) == 33

    pub_key2 = cmd.get_public_key(
        bip32_path="m/44'/1'/0'/0/0",
        display=False
    )  # type: bytes, bytes

    assert len(pub_key2) == 33
