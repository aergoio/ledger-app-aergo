
#define MAX_BIP32_PATH 10

static bool derive_keys(unsigned char *bip32Path, unsigned char bip32PathLength) {
    unsigned int  path[MAX_BIP32_PATH];
    unsigned char i;
    unsigned char privateKeyData[64];

    account_selected = false;

    /* the length must be a multiple of 4 */
    if ((bip32PathLength & 0x03) != 0) {
        return false;
    }
    bip32PathLength /= 4;
    if (bip32PathLength < 1 || bip32PathLength > MAX_BIP32_PATH) {
        return false;
    }
    for (i = 0; i < bip32PathLength; i++) {
        path[i] = U4BE(bip32Path, 0);
        bip32Path += 4;
    }

    BEGIN_TRY {
      TRY {
          io_seproxyhal_io_heartbeat();
          os_perso_derive_node_bip32(CX_CURVE_256K1, path, bip32PathLength, privateKeyData, NULL);
          cx_ecdsa_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
          io_seproxyhal_io_heartbeat();
          cx_ecfp_generate_pair(CX_CURVE_256K1, &publicKey, &privateKey, 1);

          /* convert the public key to compact format (33 bytes) */
          publicKey.W[0] = ((publicKey.W[64] & 1) ? 0x03 : 0x02);

          account_selected = true;
      }
      CATCH_OTHER(e) {
          explicit_bzero(privateKeyData, sizeof privateKeyData);
          THROW(e);
      }
      FINALLY {
          explicit_bzero(privateKeyData, sizeof privateKeyData);
          return account_selected;
      }
    } END_TRY;
}
