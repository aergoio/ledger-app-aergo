clang -fsanitize=fuzzer,address,undefined -g -O2 fuzz_tx_parser.c ../src/uint256.c sha256.c -o fuzz_tx_parser
