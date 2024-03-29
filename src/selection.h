
static void display_payload_hash() {
  int i, start_screen = num_screens;

  /* calculate the payload hash */
  sha256_finish(hash2, payload_hash);

  add_screens("New Contract 1/6", (char*)payload_hash +  0, 6, false);
  add_screens("New Contract 2/6", (char*)payload_hash +  6, 6, false);
  add_screens("New Contract 3/6", (char*)payload_hash + 12, 6, false);
  add_screens("New Contract 4/6", (char*)payload_hash + 18, 6, false);
  add_screens("New Contract 5/6", (char*)payload_hash + 24, 6, false);
  add_screens("New Contract 6/6", (char*)payload_hash + 30, 2, false);

  /* display the payload hash in hex format */
  for (i=start_screen; i<num_screens; i++) {
    screens[i].in_hex = true;
  }

  /* to avoid asking for txn parts again */
  is_first_part = true;
  is_last_part = true;

}

static void display_transaction() {
  unsigned int pos = 0;
  char *function_name, *args;
  unsigned int name_len, size;

  clear_screens();
  max_pages = 0;

  if (strcmp(amount_str,"0 AERGO") != 0 && !txn.is_system) {
    add_screens("Amount", amount_str, strlen(amount_str), false);
  }

  /* determine what to display according to the transaction type */
  switch (txn_type) {
  case TXN_TRANSFER:

    //pos = 1;

    if (num_screens == 0) {
      add_screens("Amount", amount_str, strlen(amount_str), false);
    }
    add_screens("Recipient", recipient_address, strlen(recipient_address), false);
    if (txn.payload) {
      add_screens("Payload", txn.payload, txn.payload_part_len, true);
      /* if the payload is long, display only the first pages */
      if (txn.payload_len >= 50) max_pages = 4;
    }

    break;

  case TXN_CALL:
  case TXN_FEEDELEGATION:

    pos = 2;

    /* set the screens to be displayed */

    add_screens("Contract", recipient_address, strlen(recipient_address), false);

    if (txn.payload) {
      /* parse the payload */
      /* {"Name":"some_function","Args":[<parameters>]} */
      if (parse_payload_function(&function_name, &size) == false) goto loc_invalid;
      add_screens("Function", function_name, size, true);
      screens[num_screens-1].is_call = true;
      screens[num_screens-1].trim_payload = true;
      //add_screens("Parameters", args, size, true);
    } else {
      function_name = "default";
      add_screens("Function", function_name, strlen(function_name), true);
    }

    break;

  case TXN_MULTICALL:

    pos = 3;

    /* display the payload */
    /* [["call","...","fn","arg"],["assert","..."]] */

    if (!txn.payload || txn.payload_part_len<1 || txn.payload[0]!='[') {
      goto loc_invalid;
    }

    add_screens("MultiCall", txn.payload+1, txn.payload_part_len-1, true);
    screens[num_screens-1].is_multicall = true;

    break;

  case TXN_GOVERNANCE:

    pos = 4;

    /* parse the payload */
    if (parse_payload(&function_name, &name_len, &args, &size) == false) goto loc_invalid;

    if (txn.is_system) {

      pos = 5;

      // {"Name":"v1stake"}
      if (strncmp(function_name,"v1stake",name_len) == 0) {

        add_screens("Stake", amount_str, strlen(amount_str), true);

      // {"Name":"v1unstake"}
      } else if (strncmp(function_name,"v1unstake",name_len) == 0) {

        add_screens("Unstake", amount_str, strlen(amount_str), true);

      // {"Name":"v1voteBP","Args":[<peer IDs>]}
      } else if (strncmp(function_name,"v1voteBP",name_len) == 0) {

        if (!args) goto loc_invalid;

        if (strcmp(amount_str,"0 AERGO") != 0) {
          add_screens("Amount", amount_str, strlen(amount_str), false);
        }
        add_screens("BP Vote", args, size, true);
        screens[num_screens-1].trim_payload = true;

      // {"Name":"v1voteDAO","Args":[<DAO ID>,<candidate>]}
      } else if (strncmp(function_name,"v1voteDAO",name_len) == 0) {

        if (!args) goto loc_invalid;

        if (strcmp(amount_str,"0 AERGO") != 0) {
          add_screens("Amount", amount_str, strlen(amount_str), false);
        }
        add_screens("DAO Vote", args, size, true);
        screens[num_screens-1].trim_payload = true;

      } else {
        pos = 6;
        goto loc_invalid;
      }

    } else if (txn.is_name) {

      pos = 7;

      if (!args) goto loc_invalid;

      // {"Name":"v1createName","Args":[<a name string>]}
      if (strncmp(function_name,"v1createName",name_len) == 0) {

        add_screens("Create Name", args, size, true);
        screens[num_screens-1].trim_payload = true;

      // {"Name":"v1updateName","Args":[<a name string>, <new owner address>]}
      } else if (strncmp(function_name,"v1updateName",name_len) == 0) {

        add_screens("Update Name", args, size, true);
        screens[num_screens-1].trim_payload = true;

      } else {
        pos = 8;
        goto loc_invalid;
      }

    } else if (txn.is_enterprise) {

      pos = 9;

      if (!args) goto loc_invalid;

      // {"Name":"appendAdmin","Args":[<new admin address>]}
      if (strncmp(function_name,"appendAdmin",name_len) == 0) {

        add_screens("Add Admin", args, size, true);
        screens[num_screens-1].trim_payload = true;

      // {"Name":"removeAdmin","Args":[<admin address>]}
      } else if (strncmp(function_name,"removeAdmin",name_len) == 0) {

        add_screens("Remove Admin", args, size, true);
        screens[num_screens-1].trim_payload = true;

      // {"Name":"appendConf","Args":[<config key>,<config value>]}
      } else if (strncmp(function_name,"appendConf",name_len) == 0) {

        add_screens("Add Config", args, size, true);
        screens[num_screens-1].trim_payload = true;

      // {"Name":"removeConf","Args":[<config key>,<config value>]}
      } else if (strncmp(function_name,"removeConf",name_len) == 0) {

        add_screens("Remove Config", args, size, true);
        screens[num_screens-1].trim_payload = true;

      // {"Name":"enableConf","Args":[<config key>,<true|false>]}
      } else if (strncmp(function_name,"enableConf",name_len) == 0) {

        add_screens("Enable Config", args, size, true);
        screens[num_screens-1].trim_payload = true;

      // {"Name":"changeCluster","Args":[{"command":"add","name":"[node name]","address":"[peer address]","peerid":"[peer id]"}]}
      } else if (strncmp(function_name,"changeCluster",name_len) == 0) {

        add_screens("Change Cluster", args, size, true);
        screens[num_screens-1].trim_payload = true;

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

    display_payload_hash();

    break;

  case TXN_REDEPLOY:

    //pos = 13;

    add_screens("Redeploy", recipient_address, strlen(recipient_address), false);
    display_payload_hash();

    break;

  case TXN_NORMAL:

    pos = 14;

    if (recipient_address[0] != 0) {
      add_screens("Recipient", recipient_address, strlen(recipient_address), false);
    }
    if (txn.payload) {
      add_screens("Payload", txn.payload, txn.payload_part_len, true);
      /* if the payload is long, display only the first pages */
      if (txn.payload_len >= 50) max_pages = 4;
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
  sha256(txn_hash, text, len);

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
