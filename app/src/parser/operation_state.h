/* Mavryk Embedded C parser for Ledger - Parser state for operations

   Copyright 2023 Nomadic Labs <contact@nomadic-labs.com>
   Copyright 2023 Functori <contact@functori.com>

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

#include "num_state.h"

/**
 * @brief Enumeration of all operations tags
 */
typedef enum {
    MV_OPERATION_TAG_END          = 0,
    MV_OPERATION_TAG_PROPOSALS    = 5,
    MV_OPERATION_TAG_BALLOT       = 6,
    MV_OPERATION_TAG_FAILING_NOOP = 17,
    MV_OPERATION_TAG_REVEAL       = 107,
    MV_OPERATION_TAG_TRANSACTION  = 108,
    MV_OPERATION_TAG_ORIGINATION  = 109,
    MV_OPERATION_TAG_DELEGATION   = 110,
    MV_OPERATION_TAG_REG_GLB_CST  = 111,
    MV_OPERATION_TAG_SET_DEPOSIT  = 112,
    MV_OPERATION_TAG_INC_PAID_STG = 113,
    MV_OPERATION_TAG_UPDATE_CK    = 114,
    MV_OPERATION_TAG_TRANSFER_TCK = 158,
    MV_OPERATION_TAG_SORU_ORIGIN  = 200,
    MV_OPERATION_TAG_SORU_ADD_MSG = 201,
    MV_OPERATION_TAG_SORU_EXE_MSG = 206
} mv_operation_tag;

/**
 * @brief Enumeration of all operations parser step
 */
typedef enum {
    MV_OPERATION_STEP_OPTION,
    MV_OPERATION_STEP_TUPLE,
    MV_OPERATION_STEP_MAGIC,
    MV_OPERATION_STEP_READ_BINARY,
    MV_OPERATION_STEP_BRANCH,
    MV_OPERATION_STEP_BATCH,
    MV_OPERATION_STEP_TAG,
    MV_OPERATION_STEP_SIZE,
    MV_OPERATION_STEP_FIELD,
    MV_OPERATION_STEP_PRINT,
    MV_OPERATION_STEP_PARTIAL_PRINT,
    MV_OPERATION_STEP_READ_NUM,
    MV_OPERATION_STEP_READ_INT32,
    MV_OPERATION_STEP_READ_PK,
    MV_OPERATION_STEP_READ_BYTES,
    MV_OPERATION_STEP_READ_STRING,
    MV_OPERATION_STEP_READ_SMART_ENTRYPOINT,
    MV_OPERATION_STEP_READ_MICHELINE,
    MV_OPERATION_STEP_READ_SORU_MESSAGES,
    MV_OPERATION_STEP_READ_SORU_KIND,
    MV_OPERATION_STEP_READ_BALLOT,
    MV_OPERATION_STEP_READ_PROTOS,
    MV_OPERATION_STEP_READ_PKH_LIST
} mv_operation_parser_step_kind;

/**
 * @brief Enumeration of all operations fields
 */
typedef enum {
    MV_OPERATION_FIELD_END = 0,  // not for use in field descriptors
    MV_OPERATION_FIELD_OPTION,
    MV_OPERATION_FIELD_TUPLE,
    MV_OPERATION_FIELD_BINARY,
    MV_OPERATION_FIELD_INT,
    MV_OPERATION_FIELD_NAT,
    MV_OPERATION_FIELD_AMOUNT,
    MV_OPERATION_FIELD_FEE,
    MV_OPERATION_FIELD_INT32,
    MV_OPERATION_FIELD_STRING,
    MV_OPERATION_FIELD_SOURCE,
    MV_OPERATION_FIELD_PKH,
    MV_OPERATION_FIELD_PK,
    MV_OPERATION_FIELD_SR,
    MV_OPERATION_FIELD_SRC,
    MV_OPERATION_FIELD_PROTO,
    MV_OPERATION_FIELD_PROTOS,
    MV_OPERATION_FIELD_DESTINATION,
    MV_OPERATION_FIELD_SMART_ENTRYPOINT,
    MV_OPERATION_FIELD_EXPR,
    MV_OPERATION_FIELD_OPH,
    MV_OPERATION_FIELD_BH,
    MV_OPERATION_FIELD_SORU_MESSAGES,
    MV_OPERATION_FIELD_SORU_KIND,
    MV_OPERATION_FIELD_PKH_LIST,
    MV_OPERATION_FIELD_BALLOT
} mv_operation_field_kind;

struct mv_operation_field_descriptor;

/**
 * @brief This struct represents the field descriptor of an option field
 */
typedef struct {
    const struct mv_operation_field_descriptor *field;  /// field descriptor
    uint8_t display_none : 1;                           /// display if is none
} mv_operation_option_field_descriptor;

/**
 * @brief This struct represents the descriptor of field
 */
typedef struct mv_operation_field_descriptor {
    const char             *name;      /// name
    mv_operation_field_kind kind : 5;  /// kind
    union {
        mv_operation_option_field_descriptor
            field_option;  /// field of the option
                           ///    MV_OPERATION_FIELD_OPTION

        struct {
            const struct mv_operation_field_descriptor
                *fields;  /// fields of the tuple
        } field_tuple;    /// MV_OPERATION_FIELD_TUPLE
    };
    uint8_t skip : 1;     /// if the field is not printed
    uint8_t complex : 1;  /// if the field is considered too complex for a
                          /// common user
} mv_operation_field_descriptor;

/**
 * @brief This struct represents the descriptor of operations
 */
typedef struct {
    mv_operation_tag                     tag;     /// tag
    const char                          *name;    /// name
    const mv_operation_field_descriptor *fields;  /// fields
} mv_operation_descriptor;

/**
 * @brief This struct represents the frame of the parser of operations
 *
 *        A frame contains the next step to be performed and its
 *        corresponding context
 */
typedef struct {
    mv_operation_parser_step_kind step : 5;  /// step
    uint16_t                      stop;      /// stop offset
    union {
        mv_operation_option_field_descriptor
            step_option;  /// option field
                          /// MV_OPERATION_STEP_OPTION
        struct {
            uint8_t  size_len;  /// number of bytes to read
            uint16_t size;      /// current parsed value
        } step_size;            /// MV_OPERATION_STEP_SIZE
        struct {
            const mv_operation_field_descriptor
                *field;  /// field that need to be read
        } step_field;    /// MV_OPERATION_STEP_FIELD
        struct {
            const mv_operation_field_descriptor
                   *fields;       /// fields of the tuple
            uint8_t field_index;  /// index of the current field to read
        } step_tuple;             /// MV_OPERATION_STEP_TUPLE
        struct {
            const char *str;  /// string to print
        } step_print;         /// MV_OPERATION_STEP_PRINT
                              /// MV_OPERATION_STEP_PARTIAL_PRINT
        struct {
            uint16_t ofs;  /// current bytes buffer offset
            uint16_t len;  /// expected bytes length
            mv_operation_field_kind
                kind : 5;      /// kind of field
                               /// MV_OPERATION_FIELD_SOURCE
                               /// MV_OPERATION_FIELD_PKH
                               /// MV_OPERATION_FIELD_PK
                               /// MV_OPERATION_FIELD_SR
                               /// MV_OPERATION_FIELD_SRC
                               /// MV_OPERATION_FIELD_PROTO
                               /// MV_OPERATION_FIELD_DESTINATION
                               /// MV_OPERATION_FIELD_OPH
                               /// MV_OPERATION_FIELD_BH
            uint8_t skip : 1;  /// if the field is skipped
        } step_read_bytes;     /// MV_OPERATION_STEP_READ_BYTES
        struct {
            mv_num_parser_regs      state;     /// number parser register
            mv_operation_field_kind kind : 5;  /// what kind of field
                                               /// MV_OPERATION_FIELD_NAT
                                               /// MV_OPERATION_FIELD_FEE
                                               /// MV_OPERATION_FIELD_AMOUNT
                                               /// MV_OPERATION_FIELD_INT
            uint8_t skip : 1;                  /// if the field is skipped
            uint8_t natural : 1;               /// if its a natural number
        } step_read_num;                       /// MV_OPERATION_STEP_READ_NUM
        struct {
            int32_t value;     /// current value parsed
            uint8_t skip : 1;  /// if the field is skipped
            uint8_t ofs : 3;   /// number offset
        } step_read_int32;     /// MV_OPERATION_STEP_READ_INT32
        struct {
            uint16_t ofs;       /// current buffer string offset
            uint8_t  skip : 1;  /// if the field is skipped
        } step_read_string;     /// MV_OPERATION_STEP_READ_STRING
                                /// MV_OPERATION_STEP_READ_BINARY
        struct {
            const char *name;  /// field name
            uint8_t
                inited : 1;    /// if the parser micheline has been initialize
            uint8_t skip : 1;  /// if the field is skipped
        } step_read_micheline;  /// MV_OPERATION_STEP_READ_MICHELINE
        struct {
            const char *name;      /// field name
            uint16_t    index;     /// current index in the list
            uint8_t     skip : 1;  /// if the field is skipped
        } step_read_list;          /// MV_OPERATION_STEP_READ_PROTOS
                                   /// MV_OPERATION_STEP_READ_SORU_MESSAGES
    };
} mv_operation_parser_frame;

#define MV_OPERATION_STACK_DEPTH 6  /// Maximum operations depth handled

/**
 * @brief This struct represents the parser of operations
 *
 *        The parser is a one-by-one byte reader. It uses a stack
 *        automaton, for which each frame of the stack represents the
 *        reading states of the different layers of the operations
 *        value read.
 */
typedef struct {
    mv_operation_parser_frame
        stack[MV_OPERATION_STACK_DEPTH];  /// stack of frames
    mv_operation_parser_frame *frame;     /// current frame
                                          /// init == stack, NULL when done
    uint8_t  seen_reveal : 1;             /// check at most one reveal
    uint8_t  source[22];                  /// check consistent source in batch
    uint8_t  destination[22];             /// saved for entrypoint dispatch
    uint16_t batch_index;                 /// to print a sequence number
#ifdef HAVE_SWAP
    mv_operation_tag last_tag;   /// last operations tag encountered
    uint16_t         nb_reveal;  /// number of reveal encountered
#endif                           // HAVE_SWAP
    uint64_t total_fee;          /// last fee encountered
    uint64_t total_amount;       /// last amount encountered
} mv_operation_state;
