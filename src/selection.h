
static void display_payload_hash() {
  int i;

  /* calculate the payload hash */
  cx_hash(&hash2.header, CX_LAST, NULL, 0, payload_hash, sizeof payload_hash);

  add_screens("New Contract 1/6", (char*)payload_hash +  0, 6, false);
  add_screens("New Contract 2/6", (char*)payload_hash +  6, 6, false);
  add_screens("New Contract 3/6", (char*)payload_hash + 12, 6, false);
  add_screens("New Contract 4/6", (char*)payload_hash + 18, 6, false);
  add_screens("New Contract 5/6", (char*)payload_hash + 24, 6, false);
  add_screens("New Contract 6/6", (char*)payload_hash + 30, 2, false);

  /* display the payload hash in hex format */
  for (i=0; i<num_screens; i++) {
    screens[i].in_hex = true;
  }

  /* to avoid asking for txn parts again */
  is_first_part = true;
  is_last_part = true;

}

static void display_transaction() {
  unsigned int pos = 0;
  char *function_name, *args;
  unsigned int size;

  /* determine what to display according to the transaction type */
  switch (txn_type) {
  case TXN_TRANSFER:

    //pos = 1;

    clear_screens();
    add_screens("Amount", amount_str, strlen(amount_str), false);
    add_screens("Recipient", recipient_address, strlen(recipient_address), false);
    if (txn.payload) {
      add_screens("Payload", txn.payload, txn.payload_part_len, true);
      /* display the payload in hex format because it can be just binary data */
      screens[num_screens-1].in_hex = true;
    }

    break;

  case TXN_CALL:
  case TXN_FEEDELEGATION:

    pos = 2;

    /* set the screens to be displayed */

    clear_screens();
    add_screens("Contract", recipient_address, strlen(recipient_address), false);

    if (txn.payload) {
      /* parse the payload */
      /* {"Name":"some_function","Args":[<parameters>]} */
      if (parse_payload(&function_name, &args, &size) == false) goto loc_invalid;
      if (!args) goto loc_invalid;

      add_screens("Function", function_name, strlen(function_name), true);
      add_screens("Parameters", args, size, true);
    } else {
      function_name = "default";
      add_screens("Function", function_name, strlen(function_name), false);
    }

    break;

  case TXN_MULTICALL:

    pos = 3;

    /* display the payload */
    /* [["call","...","fn","arg"],["assert","..."]] */

    if (!txn.payload || txn.payload_part_len<1 || txn.payload[0]!='[') {
      goto loc_invalid;
    }

    clear_screens();
    add_screens("MultiCall", txn.payload+1, txn.payload_part_len-1, true);
    screens[num_screens-1].is_multicall = true;

    break;

  case TXN_GOVERNANCE:

    pos = 4;

    /* parse the payload */
    if (parse_payload(&function_name, &args, &size) == false) goto loc_invalid;

    if (txn.is_system) {

      pos = 5;

      // {"Name":"v1stake"}
      if (strcmp(function_name,"v1stake") == 0) {

        clear_screens();
        add_screens("Stake", amount_str, strlen(amount_str), true);

      // {"Name":"v1unstake"}
      } else if (strcmp(function_name,"v1unstake") == 0) {

        clear_screens();
        add_screens("Unstake", amount_str, strlen(amount_str), true);

      // {"Name":"v1voteBP","Args":[<peer IDs>]}
      } else if (strcmp(function_name,"v1voteBP") == 0) {

        if (!args) goto loc_invalid;

        clear_screens();
        add_screens("BP Vote", args, size, true);

      // {"Name":"v1voteDAO","Args":[<DAO ID>,<candidate>]}
      } else if (strcmp(function_name,"v1voteDAO") == 0) {

        if (!args) goto loc_invalid;

        clear_screens();
        add_screens("DAO Vote", args, size, true);

      } else {
        pos = 6;
        goto loc_invalid;
      }

    } else if (txn.is_name) {

      pos = 7;

      if (!args) goto loc_invalid;

      // {"Name":"v1createName","Args":[<a name string>]}
      if (strcmp(function_name,"v1createName") == 0) {

        clear_screens();
        add_screens("Create Name", args, size, true);

      // {"Name":"v1updateName","Args":[<a name string>, <new owner address>]}
      } else if (strcmp(function_name,"v1updateName") == 0) {

        clear_screens();
        add_screens("Update Name", args, size, true);

      } else {
        pos = 8;
        goto loc_invalid;
      }

    } else if (txn.is_enterprise) {

      pos = 9;

      if (!args) goto loc_invalid;

      // {"Name":"appendAdmin","Args":[<new admin address>]}
      if (strcmp(function_name,"appendAdmin") == 0) {

        clear_screens();
        add_screens("Add Admin", args, size, true);

      // {"Name":"removeAdmin","Args":[<admin address>]}
      } else if (strcmp(function_name,"removeAdmin") == 0) {

        clear_screens();
        add_screens("Remove Admin", args, size, true);

      // {"Name":"appendConf","Args":[<config key>,<config value>]}
      } else if (strcmp(function_name,"appendConf") == 0) {

        clear_screens();
        add_screens("Add Config", args, size, true);

      // {"Name":"removeConf","Args":[<config key>,<config value>]}
      } else if (strcmp(function_name,"removeConf") == 0) {

        clear_screens();
        add_screens("Remove Config", args, size, true);

      // {"Name":"enableConf","Args":[<config key>,<true|false>]}
      } else if (strcmp(function_name,"enableConf") == 0) {

        clear_screens();
        add_screens("Enable Config", args, size, true);

      // {"Name":"changeCluster","Args":[{"command":"add","name":"[node name]","address":"[peer address]","peerid":"[peer id]"}]}
      } else if (strcmp(function_name,"changeCluster") == 0) {

        clear_screens();
        add_screens("Change Cluster", args, size, true);

      } else {
        pos = 10;
        goto loc_invalid;
      }

    } else {
      pos = 11;
      goto loc_invalid;
    }

    break;

  case TXN_DEPLOY:

    //pos = 12;

    clear_screens();
    display_payload_hash();

    break;

  case TXN_REDEPLOY:

    //pos = 13;

    clear_screens();
    add_screens("Redeploy", recipient_address, strlen(recipient_address), false);
    display_payload_hash();

    break;

  case TXN_NORMAL:

    pos = 14;

    clear_screens();
    if (txn.amount[0] != 0) {
      add_screens("Amount", amount_str, strlen(amount_str), false);
    }
    if (recipient_address[0] != 0) {
      add_screens("Recipient", recipient_address, strlen(recipient_address), false);
    }
    if (txn.payload) {
      add_screens("Payload", txn.payload, txn.payload_part_len, true);
      /* display the payload in hex format because it can be just binary data */
      screens[num_screens-1].in_hex = true;
    }

    if (num_screens == 0) {
      goto loc_invalid;
    }

    break;

  default:
    pos = 21;
    goto loc_invalid;
  }


  /* display the first or expected page */
  display_proper_page();

  return;

loc_invalid:
  THROW(0x6740 + pos);  // invalid data

}

static void display_txn_part() {

  screens[num_screens-1].text = txn.payload;
  screens[num_screens-1].size = txn.payload_part_len;

  display_new_input();

}

static void on_new_transaction_part(unsigned char *buf, unsigned int len, bool is_first, bool is_last){
  bool is_payload_part = (!is_first && has_partial_payload);

  parse_transaction_part(buf, len, is_first, is_last);

  is_signing = true;

  if (txn_type == TXN_DEPLOY || txn_type == TXN_REDEPLOY) {
    if (txn_is_complete) {
      display_transaction();
    } else {
      request_next_part();
    }
  } else if (is_first) {
    display_transaction();
  } else if (is_payload_part) {
    display_txn_part();
  } else {
    display_proper_page();
  }

}

static void on_new_message(unsigned char *text, unsigned int len, bool as_hex){

  /* calculate the message hash */
  cx_sha256_init(&hash3);
  sha256_add_message(text, len);
  cx_hash(&hash3.header, CX_LAST, NULL, 0, txn_hash, sizeof txn_hash);

  /* display the message */
  clear_screens();
  add_screens("Message", (char*)text, len, true);
  screens[num_screens-1].in_hex = as_hex;

  is_signing = true;
  is_first_part = true;
  is_last_part = true;
  txn_is_complete = true;
  display_proper_page();

}

static void on_display_account(unsigned char *pubkey, int pklen){

  /* encode the public key into the account address */
  encode_account(pubkey, pklen, recipient_address, sizeof recipient_address);

  /* display the account address */
  clear_screens();
  add_screens("Account", recipient_address, strlen(recipient_address), false);

  is_signing = false;
  is_first_part = true;
  is_last_part = true;
  txn_is_complete = true;
  reset_display_state();
  display_proper_page();

}
