
int crypto_derive_private_key(cx_ecfp_private_key_t *private_key) {
    uint8_t raw_private_key[32] = {0};

    BEGIN_TRY {
        TRY {
            // derive the seed with bip32_path
            os_perso_derive_node_bip32(CX_CURVE_256K1,
                                       bip32_path,
                                       bip32_path_len,
                                       raw_private_key,
                                       NULL);
            // new private_key from raw
            cx_ecfp_init_private_key(CX_CURVE_256K1,
                                     raw_private_key,
                                     sizeof(raw_private_key),
                                     private_key);
        }
        CATCH_OTHER(e) {
            explicit_bzero(&raw_private_key, sizeof(raw_private_key));
            THROW(e);
        }
        FINALLY {
            explicit_bzero(&raw_private_key, sizeof(raw_private_key));
        }
    }
    END_TRY;

    return 0;
}

int crypto_init_public_key(cx_ecfp_private_key_t *private_key) {
    // generate corresponding public key
    cx_ecfp_generate_pair(CX_CURVE_256K1, &public_key, private_key, 1);

    // convert the public key to compact format (33 bytes)
    public_key.W[0] = ((public_key.W[64] & 1) ? 0x03 : 0x02);

    account_selected = true;

    return 0;
}

bool crypto_select_account(unsigned char *bip32Path,
                           unsigned char  bip32PathLength) {
  unsigned char i;

  account_selected = false;

  // store the BIP32 path

  /* the length must be a multiple of 4 */
  if ((bip32PathLength & 0x03) != 0) {
    return false;
  }
  bip32_path_len = bip32PathLength / 4;
  if (bip32_path_len < 1 || bip32_path_len > MAX_BIP32_PATH) {
    return false;
  }
  for (i = 0; i < bip32_path_len; i++) {
    bip32_path[i] = U4BE(bip32Path, 0);
    bip32Path += 4;
  }

  // generate the public key for this account

  cx_ecfp_private_key_t private_key;

  BEGIN_TRY {
    TRY {
      crypto_derive_private_key(&private_key);
      io_seproxyhal_io_heartbeat();  // required?
      crypto_init_public_key(&private_key);
    }
    CATCH_OTHER(e) {
      explicit_bzero(&private_key, sizeof(private_key));
      THROW(e);
    }
    FINALLY {
      explicit_bzero(&private_key, sizeof(private_key));
      return account_selected;
    }
  }
  END_TRY;

}

int crypto_sign_message(unsigned char *signature, unsigned int sig_max_len) {
    cx_ecfp_private_key_t private_key = {0};
    uint32_t info = 0;
    int sig_len = 0;

    // derive private key according to BIP32 path
    crypto_derive_private_key(&private_key);

    BEGIN_TRY {
        TRY {
            sig_len = cx_ecdsa_sign(&private_key,
                                    CX_RND_RFC6979 | CX_LAST,
                                    CX_SHA256,
                                    txn_hash,
                                    sizeof txn_hash,
                                    signature,
                                    sig_max_len,
                                    &info);
            PRINTF("Signature: %.*H\n", sig_len, signature);
        }
        CATCH_OTHER(e) {
            explicit_bzero(&private_key, sizeof(private_key));
            THROW(e);
        }
        FINALLY {
            explicit_bzero(&private_key, sizeof(private_key));
        }
    }
    END_TRY;

    //tx_info.v = (uint8_t)(info & CX_ECCINFO_PARITY_ODD);
    signature[0] &= 0xF0; // discard the parity information

    return sig_len;
}
