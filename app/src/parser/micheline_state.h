/* Mavryk Embedded C parser for Ledger - Parser state for Micheline data

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

#include "num_state.h"

#define MV_MICHELINE_STACK_DEPTH 45  /// Maximum micheline depth handled

/**
 * @brief Enumeration of all micheline tags
 */
typedef enum {
    MV_MICHELINE_TAG_INT,
    MV_MICHELINE_TAG_STRING,
    MV_MICHELINE_TAG_SEQ,
    MV_MICHELINE_TAG_PRIM_0_NOANNOTS,
    MV_MICHELINE_TAG_PRIM_0_ANNOTS,
    MV_MICHELINE_TAG_PRIM_1_NOANNOTS,
    MV_MICHELINE_TAG_PRIM_1_ANNOTS,
    MV_MICHELINE_TAG_PRIM_2_NOANNOTS,
    MV_MICHELINE_TAG_PRIM_2_ANNOTS,
    MV_MICHELINE_TAG_PRIM_N,
    MV_MICHELINE_TAG_BYTES
} mv_micheline_tag;

/**
 * @brief Enumeration of all micheline parser step
 */
typedef enum {
    MV_MICHELINE_STEP_TAG,
    MV_MICHELINE_STEP_PRIM_OP,
    MV_MICHELINE_STEP_PRIM_NAME,
    MV_MICHELINE_STEP_PRIM,
    MV_MICHELINE_STEP_SIZE,
    MV_MICHELINE_STEP_SEQ,
    MV_MICHELINE_STEP_BYTES,
    MV_MICHELINE_STEP_STRING,
    MV_MICHELINE_STEP_ANNOT,
    MV_MICHELINE_STEP_INT,
    MV_MICHELINE_STEP_PRINT_INT,
    MV_MICHELINE_STEP_PRINT_CAPTURE
} mv_micheline_parser_step_kind;

/**
 * @brief
 */
typedef enum {
    MV_CAP_STREAM_ANY    = 0,
    MV_CAP_STREAM_BYTES  = 1,
    MV_CAP_STREAM_INT    = 2,
    MV_CAP_STREAM_STRING = 3,
    MV_CAP_ADDRESS       = 4,
    MV_CAP_LIST          = 62,
    MV_CAP_OR            = 63
} mv_micheline_capture_kind;

/**
 * @brief This struct represents the frame of the parser of micheline
 *
 *        A frame contains the next step to be performed and its
 *        corresponding context
 */
typedef struct {
    mv_micheline_parser_step_kind step : 4;  /// step
    uint16_t                      stop;      /// stop offset
    union {
        struct {
            uint16_t size;  /// size read
        } step_size;        /// MV_MICHELINE_STEP_SIZE
        struct {
            uint8_t first : 1;  /// if read first byte
        } step_seq;             /// MV_MICHELINE_STEP_SEQ
        struct {
            uint8_t first : 1;         /// if read first byte
            uint8_t has_rem_half : 1;  /// if half the byte remains to print
            uint8_t rem_half;          /// remaining half of the byte
        } step_bytes;                  /// MV_MICHELINE_STEP_BYTES
        struct {
            uint8_t first : 1;  /// if read first byte
        } step_string;          /// MV_MICHELINE_STEP_STRING
        struct {
            uint8_t first : 1;        /// if read first byte
        } step_annot;                 /// MV_MICHELINE_STEP_ANNOT
        mv_num_parser_regs step_int;  /// number parser register
                                      /// MV_MICHELINE_STEP_INT,
                                      /// MV_MICHELINE_STEP_PRINT_INT
        struct {
            uint8_t op;         /// prim op
            uint8_t ofs;        /// offset
            uint8_t nargs : 2;  /// number of arguments
            uint8_t wrap : 1;   /// if wrap in a prim
            uint8_t spc : 1;    /// if has space
            uint8_t annot : 1;  /// if need to read an annotation
            uint8_t first : 1;  /// if read first byte
        } step_prim;            /// MV_MICHELINE_STEP_PRIM_OP,
                                /// MV_MICHELINE_STEP_PRIM_NAME,
                                /// MV_MICHELINE_STEP_PRIM
        struct {
            int ofs;     /// offset of the capture buffer
        } step_capture;  /// MV_MICHELINE_STEP_CAPTURE_BYTES,
                         /// MV_MICHELINE_STEP_PRINT_CAPTURE
    };
} mv_micheline_parser_frame;

/**
 * @brief This struct represents the parser of micheline
 *
 *        The parser is a one-by-one byte reader. It uses a stack
 *        automaton, for which each frame of the stack represents the
 *        reading states of the different layers of the micheline
 *        value read.
 */
typedef struct {
    mv_micheline_parser_frame
        stack[MV_MICHELINE_STACK_DEPTH];  /// stack of frames
    mv_micheline_parser_frame *frame;     /// current frame
                                          /// init == stack, NULL when done
    bool is_unit;  /// indicates whether the micheline read is a unit
} mv_micheline_state;
