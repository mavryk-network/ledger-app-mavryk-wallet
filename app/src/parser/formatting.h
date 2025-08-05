/* Mavryk Embedded C parser for Ledger - Human printing of Mavryk formats

   Copyright 2023 Nomadic Labs <contact@nomadic-labs.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#pragma once

#include "compat.h"

/**
 * @brief Last Michelson operation code
 *
 *        Should be kept in sync with the last protocol update.
 */
#define MV_LAST_MICHELSON_OPCODE 157

/**
 * @brief Get the human readable name of a Michelson op_code if valid
 *        (`op_code <= MV_LAST_MICHELSON_OPCODE`), or `NULL`.
 *
 *        This function needs to be updated when new michelson
 *        instructions are added via a Mavryk protocol upgrade to
 *        support new opcodes, the existing opcodes are guaranteed to
 *        stay unchanged, so not updating does not break security.
 *
 * @param op_code: code of the michelson primitive
 * @return const char*: name of the michelson primitive
 */
const char *mv_michelson_op_name(uint8_t op_code);

/**
 * @brief Enumeration of all Michelson operation code
 *
 *        Should be kept in sync with the last protocol update, including
 *        order, currently defined in the `michelson_v1_primitives.ml` file
 *        in the Mavryk protocol code.
 */
typedef enum {
    MV_MICHELSON_OP_parameter                      = 0,
    MV_MICHELSON_OP_storage                        = 1,
    MV_MICHELSON_OP_code                           = 2,
    MV_MICHELSON_OP_False                          = 3,
    MV_MICHELSON_OP_Elt                            = 4,
    MV_MICHELSON_OP_Left                           = 5,
    MV_MICHELSON_OP_None                           = 6,
    MV_MICHELSON_OP_Pair                           = 7,
    MV_MICHELSON_OP_Right                          = 8,
    MV_MICHELSON_OP_Some                           = 9,
    MV_MICHELSON_OP_True                           = 10,
    MV_MICHELSON_OP_Unit                           = 11,
    MV_MICHELSON_OP_PACK                           = 12,
    MV_MICHELSON_OP_UNPACK                         = 13,
    MV_MICHELSON_OP_BLAKE2B                        = 14,
    MV_MICHELSON_OP_SHA256                         = 15,
    MV_MICHELSON_OP_SHA512                         = 16,
    MV_MICHELSON_OP_ABS                            = 17,
    MV_MICHELSON_OP_ADD                            = 18,
    MV_MICHELSON_OP_AMOUNT                         = 19,
    MV_MICHELSON_OP_AND                            = 20,
    MV_MICHELSON_OP_BALANCE                        = 21,
    MV_MICHELSON_OP_CAR                            = 22,
    MV_MICHELSON_OP_CDR                            = 23,
    MV_MICHELSON_OP_CHECK_SIGNATURE                = 24,
    MV_MICHELSON_OP_COMPARE                        = 25,
    MV_MICHELSON_OP_CONCAT                         = 26,
    MV_MICHELSON_OP_CONS                           = 27,
    MV_MICHELSON_OP_CREATE_ACCOUNT                 = 28,
    MV_MICHELSON_OP_CREATE_CONTRACT                = 29,
    MV_MICHELSON_OP_IMPLICIT_ACCOUNT               = 30,
    MV_MICHELSON_OP_DIP                            = 31,
    MV_MICHELSON_OP_DROP                           = 32,
    MV_MICHELSON_OP_DUP                            = 33,
    MV_MICHELSON_OP_EDIV                           = 34,
    MV_MICHELSON_OP_EMPTY_MAP                      = 35,
    MV_MICHELSON_OP_EMPTY_SET                      = 36,
    MV_MICHELSON_OP_EQ                             = 37,
    MV_MICHELSON_OP_EXEC                           = 38,
    MV_MICHELSON_OP_FAILWITH                       = 39,
    MV_MICHELSON_OP_GE                             = 40,
    MV_MICHELSON_OP_GET                            = 41,
    MV_MICHELSON_OP_GT                             = 42,
    MV_MICHELSON_OP_HASH_KEY                       = 43,
    MV_MICHELSON_OP_IF                             = 44,
    MV_MICHELSON_OP_IF_CONS                        = 45,
    MV_MICHELSON_OP_IF_LEFT                        = 46,
    MV_MICHELSON_OP_IF_NONE                        = 47,
    MV_MICHELSON_OP_INT                            = 48,
    MV_MICHELSON_OP_LAMBDA                         = 49,
    MV_MICHELSON_OP_LE                             = 50,
    MV_MICHELSON_OP_LEFT                           = 51,
    MV_MICHELSON_OP_LOOP                           = 52,
    MV_MICHELSON_OP_LSL                            = 53,
    MV_MICHELSON_OP_LSR                            = 54,
    MV_MICHELSON_OP_LT                             = 55,
    MV_MICHELSON_OP_MAP                            = 56,
    MV_MICHELSON_OP_MEM                            = 57,
    MV_MICHELSON_OP_MUL                            = 58,
    MV_MICHELSON_OP_NEG                            = 59,
    MV_MICHELSON_OP_NEQ                            = 60,
    MV_MICHELSON_OP_NIL                            = 61,
    MV_MICHELSON_OP_NONE                           = 62,
    MV_MICHELSON_OP_NOT                            = 63,
    MV_MICHELSON_OP_NOW                            = 64,
    MV_MICHELSON_OP_OR                             = 65,
    MV_MICHELSON_OP_PAIR                           = 66,
    MV_MICHELSON_OP_PUSH                           = 67,
    MV_MICHELSON_OP_RIGHT                          = 68,
    MV_MICHELSON_OP_SIZE                           = 69,
    MV_MICHELSON_OP_SOME                           = 70,
    MV_MICHELSON_OP_SOURCE                         = 71,
    MV_MICHELSON_OP_SENDER                         = 72,
    MV_MICHELSON_OP_SELF                           = 73,
    MV_MICHELSON_OP_STEPS_TO_QUOTA                 = 74,
    MV_MICHELSON_OP_SUB                            = 75,
    MV_MICHELSON_OP_SWAP                           = 76,
    MV_MICHELSON_OP_TRANSFER_TOKENS                = 77,
    MV_MICHELSON_OP_SET_DELEGATE                   = 78,
    MV_MICHELSON_OP_UNIT                           = 79,
    MV_MICHELSON_OP_UPDATE                         = 80,
    MV_MICHELSON_OP_XOR                            = 81,
    MV_MICHELSON_OP_ITER                           = 82,
    MV_MICHELSON_OP_LOOP_LEFT                      = 83,
    MV_MICHELSON_OP_ADDRESS                        = 84,
    MV_MICHELSON_OP_CONTRACT                       = 85,
    MV_MICHELSON_OP_ISNAT                          = 86,
    MV_MICHELSON_OP_CAST                           = 87,
    MV_MICHELSON_OP_RENAME                         = 88,
    MV_MICHELSON_OP_bool                           = 89,
    MV_MICHELSON_OP_contract                       = 90,
    MV_MICHELSON_OP_int                            = 91,
    MV_MICHELSON_OP_key                            = 92,
    MV_MICHELSON_OP_key_hash                       = 93,
    MV_MICHELSON_OP_lambda                         = 94,
    MV_MICHELSON_OP_list                           = 95,
    MV_MICHELSON_OP_map                            = 96,
    MV_MICHELSON_OP_big_map                        = 97,
    MV_MICHELSON_OP_nat                            = 98,
    MV_MICHELSON_OP_option                         = 99,
    MV_MICHELSON_OP_or                             = 100,
    MV_MICHELSON_OP_pair                           = 101,
    MV_MICHELSON_OP_set                            = 102,
    MV_MICHELSON_OP_signature                      = 103,
    MV_MICHELSON_OP_string                         = 104,
    MV_MICHELSON_OP_bytes                          = 105,
    MV_MICHELSON_OP_mumav                          = 106,
    MV_MICHELSON_OP_timestamp                      = 107,
    MV_MICHELSON_OP_unit                           = 108,
    MV_MICHELSON_OP_operation                      = 109,
    MV_MICHELSON_OP_address                        = 110,
    MV_MICHELSON_OP_SLICE                          = 111,
    MV_MICHELSON_OP_DIG                            = 112,
    MV_MICHELSON_OP_DUG                            = 113,
    MV_MICHELSON_OP_EMPTY_BIG_MAP                  = 114,
    MV_MICHELSON_OP_APPLY                          = 115,
    MV_MICHELSON_OP_chain_id                       = 116,
    MV_MICHELSON_OP_CHAIN_ID                       = 117,
    MV_MICHELSON_OP_LEVEL                          = 118,
    MV_MICHELSON_OP_SELF_ADDRESS                   = 119,
    MV_MICHELSON_OP_never                          = 120,
    MV_MICHELSON_OP_NEVER                          = 121,
    MV_MICHELSON_OP_UNPAIR                         = 122,
    MV_MICHELSON_OP_VOTING_POWER                   = 123,
    MV_MICHELSON_OP_TOTAL_VOTING_POWER             = 124,
    MV_MICHELSON_OP_KECCAK                         = 125,
    MV_MICHELSON_OP_SHA3                           = 126,
    MV_MICHELSON_OP_PAIRING_CHECK                  = 127,
    MV_MICHELSON_OP_bls12_381_g1                   = 128,
    MV_MICHELSON_OP_bls12_381_g2                   = 129,
    MV_MICHELSON_OP_bls12_381_fr                   = 130,
    MV_MICHELSON_OP_sapling_state                  = 131,
    MV_MICHELSON_OP_sapling_transaction_deprecated = 132,
    MV_MICHELSON_OP_SAPLING_EMPTY_STATE            = 133,
    MV_MICHELSON_OP_SAPLING_VERIFY_UPDATE          = 134,
    MV_MICHELSON_OP_ticket                         = 135,
    MV_MICHELSON_OP_TICKET_DEPRECATED              = 136,
    MV_MICHELSON_OP_READ_TICKET                    = 137,
    MV_MICHELSON_OP_SPLIT_TICKET                   = 138,
    MV_MICHELSON_OP_JOIN_TICKETS                   = 139,
    MV_MICHELSON_OP_GET_AND_UPDATE                 = 140,
    MV_MICHELSON_OP_chest                          = 141,
    MV_MICHELSON_OP_chest_key                      = 142,
    MV_MICHELSON_OP_OPEN_CHEST                     = 143,
    MV_MICHELSON_OP_VIEW                           = 144,
    MV_MICHELSON_OP_view                           = 145,
    MV_MICHELSON_OP_constant                       = 146,
    MV_MICHELSON_OP_SUB_MUMAV                      = 147,
    MV_MICHELSON_OP_tx_rollup_l2_address           = 148,
    MV_MICHELSON_OP_MIN_BLOCK_TIME                 = 149,
    MV_MICHELSON_OP_sapling_transaction            = 150,
    MV_MICHELSON_OP_EMIT                           = 151,
    MV_MICHELSON_OP_Lambda_rec                     = 152,
    MV_MICHELSON_OP_LAMBDA_REC                     = 153,
    MV_MICHELSON_OP_TICKET                         = 154,
    MV_MICHELSON_OP_BYTES                          = 155,
    MV_MICHELSON_OP_NAT                            = 156,
    MV_MICHELSON_OP_Ticket                         = 157
} mv_michelson_opcode;

#define MV_DECIMAL_BUFFER_SIZE(_l) ((((_l)*241) / 100) + 1)

/**
 * @brief Formats a positive number of arbitrary to decimal.
 *
 *        The number is stored in little-endian order in the first `l`
 *        bytes of `n`. The output buffer `obuf` must be at least
 *        `MV_DECIMAL_BUFFER_SIZE(l)` (caller responsibility).
 *
 * @param n: input number
 * @param l: length of the input number
 * @param obuf: output buffer
 * @param olen: length of the output buffer
 * @return int: 0 on success
 */
int mv_format_decimal(const uint8_t *n, size_t l, char *obuf, size_t olen);

#define MV_BASE58_BUFFER_SIZE(_l) ((((_l)*138) / 100) + 1)

/**
 * @brief Formats a data `n` of size `l` in base58 using Mavryk'
 *        alphabet order (same as Bitcoin).
 *
 *        The output buffer `obuf` must be at least
 *        `BASE58_BUFFER_SIZE(l)` (caller responsibility).
 *
 * @param n: input data
 * @param l: length of the input data
 * @param obuf: output buffer
 * @param olen: length of the output buffer
 * @return int: 0 on success
 */
int mv_format_base58(const uint8_t *n, size_t l, char *obuf, size_t olen);

#define MV_BASE58CHECK_BUFFER_SIZE(_l, _p) \
    MV_BASE58_BUFFER_SIZE(((_p) + (_l)) + 4)

/**
 * @brief Looks up the prefix from the provided string (arg1),
 *        e.g. "B", "o", "expr", "mv2", etc.
 *
 *        This indexes into a table which finds the appropriate binary
 *        prefix and the data length which is required. Will return an
 *        error if the lengths do not match.  Then it will add the
 *        provided prefix, append the four first bytes of a
 *        double-sha256 of this concatenation, and call
 *        `format_base58`. The output buffer `obuf` must be at least
 *        `MV_BASE58CHECK_BUFFER_SIZE(l, prefix_len)` (caller
 *        responsibility).
 *
 * @param prefix: base58 prefix
 * @param ibuf: input buffer
 * @param ilen: length of the input buffer
 * @param obuf: output buffer
 * @param olen: length of the output buffer
 * @return int: 0 on success
 */
int mv_format_base58check(const char *prefix, const uint8_t *ibuf,
                          size_t ilen, char *obuf, size_t olen);

/**
 * Some Mavryk-specific base58check formatters. These functions
 * deconstruct the Mavryk binary header to figure out the kind within
 * the type (e.g. the curve for keys), check the length, and feed the
 * appropriate prefix to `format_base58check`. These function need to
 * be updated when new formats are added via a Mavryk protocol upgrade.
 */

/**
 * @brief Get the public key hash format
 *
 *        size 21: tag(1) + pkh(20)
 *        tag 0: mv1(36)
 *        tag 1: mv2(36)
 *        tag 2: mv3(36)
 *        tag 3: mv4(36)
 *
 * @param ibuf: input buffer
 * @param ilen: length of the input buffer
 * @param obuf: output buffer
 * @param olen: length of the output buffer
 * @return int: 0 on success
 */
int mv_format_pkh(const uint8_t *ibuf, size_t ilen, char *obuf, size_t olen);

/**
 * @brief Get the public key hash format
 *
 *        size 32, o(51)
 *
 * @param ibuf: input buffer
 * @param ilen: length of the input buffer
 * @param obuf: output buffer
 * @param olen: length of the output buffer
 * @return int: 0 on success
 *
 * @deprecated Use mv_format_base58check("o", ...) instead
 */
int mv_format_oph(const uint8_t *ibuf, size_t ilen, char *obuf, size_t olen);

/**
 * @brief Get the public key hash format
 *
 *        size 32, B(51)
 *
 * @param ibuf: input buffer
 * @param ilen: length of the input buffer
 * @param obuf: output buffer
 * @param olen: length of the output buffer
 * @return int: 0 on success
 *
 * @deprecated Use mv_format_base58check("B", ...) instead
 */
int mv_format_bh(const uint8_t *ibuf, size_t ilen, char *obuf, size_t olen);

/**
 * @brief Get the public key hash format
 *
 *        size 22: tag(1) + data(21)
 *        tag 0: tag(1) + pkh(20) (mv1, mv2, mv3, mv4, see format_pkh)
 *        tag 1: txrolluph(20) + padding(1), KT1(36)
 *        tag 2: txrolluph(20) + padding(1), txr1(36)
 *        tag 3: rolluph(20) + padding(1), scr1(36)
 *        tag 4: zkrolluph(20) + padding(1), zkr1(36)
 *
 * @param ibuf: input buffer
 * @param ilen: length of the input buffer
 * @param obuf: output buffer
 * @param olen: length of the output buffer
 * @return int: 0 on success
 */
int mv_format_address(const uint8_t *ibuf, size_t ilen, char *obuf,
                      size_t olen);

/**
 * @brief Get the address format
 *
 *        size 33/34/49: tag(1) + data(32/33/48)
 *        tag 0: pk(32), edpk(54)
 *        tag 1: pk(33), sppk(55)
 *        tag 2: pk(33), p2pk(55)
 *        tag 3: pk(48), BLpk(76)
 *
 * @param ibuf: input buffer
 * @param ilen: length of the input buffer
 * @param obuf: output buffer
 * @param olen: length of the output buffer
 * @return int: 0 on success
 */
int mv_format_pk(const uint8_t *ibuf, size_t ilen, char *obuf, size_t olen);
