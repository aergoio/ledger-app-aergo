
char global_title[20];
char global_text[20];


int  cmd_type;
bool is_signing;
bool is_first_part;
bool is_last_part;
bool txn_is_complete;
bool has_partial_payload;


static cx_sha256_t hash;
static cx_sha256_t hash2;
unsigned char txn_hash[32];
unsigned char payload_hash[32];

