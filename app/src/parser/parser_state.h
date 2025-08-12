/* Mavryk Embedded C parser for Ledger - Full parser state definition and
   helpers

   Copyright 2023 Nomadic Labs <contact@nomadic-labs.com>
   Copyright 2023 TriliTech <contact@trili.tech>

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
#include "micheline_state.h"
#include "operation_state.h"

// Parser buffers and buffer handling registers

/**
 * @brief This struct represents the parser register.
 *
 *        It contains an output and an input buffers
 */
typedef struct {
    /// update `ibuf`, `iofs` and `ilen` at once with `parser_regs_refill`
    /// invariant between two refills:
    ///   `iofs + ilen` = (constant) number of readable bytes in `ibuf`
    const uint8_t *ibuf;  /// input buffer
    size_t         iofs;  /// current offset
    size_t         ilen;  /// remaining bytes readable in input
    /// update `obuf`, `oofs` and `olen` at once with `parser_regs_flush`
    /// invariant between two refills:
    ///   `oofs + olen` = (constant) number of readable bytes in `obuf`
    char  *obuf;  /// output buffer
    size_t oofs;  /// current offset
    size_t olen;  /// remaining bytes writable in output
} mv_parser_regs;

// Parser state

#define MV_FIELD_NAME_SIZE     30   /// Size of the field name buffer
#define MV_CAPTURE_BUFFER_SIZE 256  /// Size of the capture buffer

/**
 * @brief Enumeration of all kinds of results meaningful to the parser
 */
typedef enum {
    /// success and non blocking, should loop again
    MV_CONTINUE = 0,  /// fall through rest of current step
    MV_BREAK    = 1,  /// signals caller to return, errno should be CONTINUE
    /// success but parsing blocked
    MV_BLO_DONE    = 100,  /// parsing complete
    MV_BLO_FEED_ME = 101,  /// blocked on read from input
    MV_BLO_IM_FULL = 102,  /// blocked on output space
    /// everything below is an error
    MV_ERR_INVALID_TAG = 200,  /// an invalid tag has been found
    MV_ERR_INVALID_OP
        = 201,  /// an invalid michelson operation has been found
    MV_ERR_INVALID_DATA  = 202,  /// a data has been considered as invalid
    MV_ERR_UNSUPPORTED   = 203,  /// a non-supported action has been triggered
    MV_ERR_TOO_LARGE     = 204,  /// too large data has been found
    MV_ERR_TOO_DEEP      = 205,  /// too deep data has been found
    MV_ERR_INVALID_STATE = 206,  /// parser is in an invalid state
} mv_parser_result;

#define MV_IS_BLOCKED(code) \
    (code >= 100)                      /// If the result is a blocking result
#define MV_IS_ERR(code) (code >= 200)  /// If the result is an error

/**
 * @brief Get the human readable name of a parser result
 *
 * @param code: parser result
 * @return const char*: name of the parser result
 */
const char *mv_parser_result_name(mv_parser_result code);

/**
 * @brief This struct represents the parser state.
 */
typedef struct {
    mv_parser_regs regs;  /// parser register

    /// common fields to communicate with caller
    struct {
        char field_name[MV_FIELD_NAME_SIZE];  /// name of the last field
                                              /// parsed
        bool is_field_complex;  /// if the last field parsed is considered
                                /// too complex for a common user
        int field_index;        /// index of the last field parsed
    } field_info;               /// information of the last field parsed
                                // common singleton buffers
    int ofs;                    /// offset for the parser
    /// input type specific state
    mv_micheline_state micheline;  /// micheline parser state
    mv_operation_state operation;  /// operation parser state
    struct {
        mv_num_parser_buffer num;                 /// number parser buffer
        uint8_t capture[MV_CAPTURE_BUFFER_SIZE];  /// capture buffer is used
                                                  /// to store string values
    } buffers;
    mv_parser_result errno;  /// current parser result
} mv_parser_state;

/**
 * @brief Initialize a parser state
 *
 * @param state: parser state
 */
void mv_parser_init(mv_parser_state *state);

/**
 * @brief Flush what has been parsed
 *
 * @param state: parser state
 * @param obuf: ouput buffer
 * @param olen: length of the output buffer
 */
void mv_parser_flush(mv_parser_state *state, char *obuf, size_t olen);

/**
 * @brief Flush a part of what has been parsed
 *
 * @param state: parser state
 * @param obuf: ouput buffer
 * @param olen: length of the output buffer
 * @param up_to: length of what we want to flush up to
 */
void mv_parser_flush_up_to(mv_parser_state *state, char *obuf, size_t olen,
                           size_t up_to);

/**
 * @brief Refill what should be parsed
 *
 * @param state: parser state
 * @param ibuf: input buffer
 * @param ilen: length of the input buffer
 */
void mv_parser_refill(mv_parser_state *state, const uint8_t *ibuf,
                      size_t ilen);

/**
 * @brief Skip to next byte
 *
 * @param state: parser state
 */
void mv_parser_skip(mv_parser_state *state);

/**
 * @brief Put a character at the end of was has been parsed
 *
 * @param state: parser state
 * @param c: character
 * @return mv_parser_result: parser result
 */
mv_parser_result mv_parser_put(mv_parser_state *state, char c);

/**
 * @brief Read a bytes
 *
 * @param state: parser state
 * @param out: output character pointer
 * @return mv_parser_result: parser result
 */
mv_parser_result mv_parser_read(mv_parser_state *state, uint8_t *out);

/**
 * @brief Peek a bytes
 *
 * @param state: parser state
 * @param out: output character pointer
 * @return mv_parser_result: parser result
 */
mv_parser_result mv_parser_peek(mv_parser_state *state, uint8_t *out);

// error handling utils

/**
 * @brief Set and raise a parser result
 *
 * @param state: parser state
 * @param code: parser result to set
 * @return mv_parser_result: parser result raised
 */
mv_parser_result mv_parser_set_errno(mv_parser_state *state,
                                     mv_parser_result code);

/**
 * @brief Set a parser result and raise
 *
 *        Expect a `state` variable
 */
#ifdef MAVRYK_DEBUG
#define mv_return(e)                                               \
    do {                                                           \
        mv_parser_result _c = (e);                                 \
        if (_c) {                                                  \
            PRINTF("[DEBUG] mv_return(code: %s, loc: %s:%d)\n",    \
                   mv_parser_result_name(_c), __FILE__, __LINE__); \
        }                                                          \
        return mv_parser_set_errno(state, _c);                     \
    } while (0)
#else
#define mv_return(e) return mv_parser_set_errno(state, e)
#endif

#define mv_raise(e) mv_return(MV_ERR_##e)  /// Raise an parser error
#define mv_stop(e)  mv_return(MV_BLO_##e)  /// Stop the parser
#define mv_reraise  return state->errno    /// Re-raise the parser result

/**
 * @brief Raise if the condition did not continue
 *
 *        Usually used to ensure that the parser did not raise any error
 */
#define mv_must(cond)                 \
    do {                              \
        mv_parser_result _err = cond; \
        if (_err) {                   \
            mv_return(_err);          \
        }                             \
    } while (0)

#define mv_continue mv_return(MV_CONTINUE)  /// continue parsing
#define mv_break    mv_return(MV_BREAK)     /// break parsing
