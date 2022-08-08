#pragma once

/**
 * Status word for success.
 */
#define SW_OK 0x9000
/**
 * Status word for re-send first transaction part.
 */
#define SW_RESEND_FIRST_PART 0x9001
/**
 * Status word for rejected by the user.
 */
#define SW_REJECTED 0x6982
/**
 * Status word for either wrong Lc or length of APDU command less than 5.
 */
#define SW_WRONG_DATA_LENGTH 0x6A87
/**
 * Status word for instruction class is different than CLA.
 */
#define SW_CLA_NOT_SUPPORTED 0x6E00
/**
 * Status word for unknown command with this INS.
 */
#define SW_INS_NOT_SUPPORTED 0x6D00
/**
 * Status word for incorrect P1 or P2.
 */
#define SW_WRONG_P1P2 0x6A86
/**
 * Status word for incorrect LC.
 */
#define SW_WRONG_LENGTH 0x6700
/**
 * Status word for invalid state.
 */
#define SW_INVALID_STATE 0x6985
