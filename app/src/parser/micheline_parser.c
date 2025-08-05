/* Mavryk Embedded C parser for Ledger - Micheline data parser

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

/*
 * We use a few idioms throughout this file.  In each function, we
 * define a local variable "m" which is a ptr to state->micheline.
 */

#include "micheline_parser.h"
#include "num_parser.h"

/* Prototypes */

static mv_parser_result push_frame(mv_parser_state              *state,
                                   mv_micheline_parser_step_kind step);
static mv_parser_result pop_frame(mv_parser_state *state);
static mv_parser_result begin_sized(mv_parser_state *state);
static mv_parser_result print_escaped(mv_parser_state *state, uint8_t b);
static mv_parser_result parser_put(mv_parser_state *state, char c);
static mv_parser_result tag_selection(mv_parser_state *state, uint8_t t);

#ifdef MAVRYK_DEBUG
const char *const mv_micheline_parser_step_name[]
    = {"TAG",   "PRIM_OP", "PRIM_NAME", "PRIM", "SIZE",      "SEQ",
       "BYTES", "STRING",  "ANNOT",     "INT",  "PRINT_INT", "CONTINUE"};
#endif

const char hex_c[] = "0123456789ABCDEF";

void
mv_micheline_parser_init(mv_parser_state *state)
{
    mv_micheline_state *m = &state->micheline;

    m->frame         = m->stack;
    m->stack[0].step = MV_MICHELINE_STEP_TAG;
    m->is_unit       = false;
}

/**
 * @brief Push a new frame onto the micheline parser stack
 *
 * @param state: parser state
 * @param step: step of the new frame
 * @return mv_parser_result: parser result
 */
static mv_parser_result
push_frame(mv_parser_state *state, mv_micheline_parser_step_kind step)
{
    mv_micheline_state *m = &state->micheline;

    if (m->frame >= &m->stack[MV_MICHELINE_STACK_DEPTH - 1]) {
        mv_raise(TOO_DEEP);
    }
    m->frame++;
    m->frame->step = step;
    mv_continue;
}

/**
 * @brief Pop the micheline parser stack
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
pop_frame(mv_parser_state *state)
{
    mv_micheline_state *m = &state->micheline;

    if (m->frame == m->stack) {
        m->frame = NULL;
        mv_stop(DONE);
    }
    m->frame--;
    mv_continue;
}

/**
 * @brief Ask to read a 4-bytes size
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
begin_sized(mv_parser_state *state)
{
    mv_micheline_state *m = &state->micheline;

    if (push_frame(state, MV_MICHELINE_STEP_SIZE)) {
        mv_reraise;
    }
    m->frame->step_size.size = 0;
    m->frame->stop           = state->ofs + 4;
    mv_continue;
}

/**
 * @brief Ask to print an escape character
 *
 * @param state: parser state
 * @param b: escape character
 * @return mv_parser_result: parser result
 */
static mv_parser_result
print_escaped(mv_parser_state *state, uint8_t b)
{
    char *buf = (char *)state->buffers.capture;
    mv_must(push_frame(state, MV_MICHELINE_STEP_PRINT_CAPTURE));
    state->micheline.frame->step_capture.ofs = 0;
    // clang-format off
    switch (b) {
    case '\\': strncpy(buf,"\\\\",MV_CAPTURE_BUFFER_SIZE); break;
    case '"':  strncpy(buf,"\\\"",MV_CAPTURE_BUFFER_SIZE); break;
    case '\r': strncpy(buf,"\\r",MV_CAPTURE_BUFFER_SIZE);  break;
    case '\n': strncpy(buf,"\\n",MV_CAPTURE_BUFFER_SIZE);  break;
    case '\t': strncpy(buf,"\\t",MV_CAPTURE_BUFFER_SIZE);  break;
    default:
        buf[0] = '0' + (b/100);
        buf[1] = '0' + ((b/10)%10);
        buf[2] = '0' + (b%10);
        buf[3] = 0;
        break;
    }
    // clang-format on
    mv_continue;
}

/**
 * @brief Print a character
 *
 * @param state: parser state
 * @param c: character
 * @return mv_parser_result: parser result
 */
static mv_parser_result
parser_put(mv_parser_state *state, char c)
{
    PRINTF("[DEBUG] put(char: '%c',int: %d)\n", c, (int)c);
    return mv_parser_put(state, c);
}

/**
 * @brief Plan the steps required to read the micheline value
 *        associated to the micheline tag
 *
 * @param state: parser state
 * @param t: micheline tag
 * @return mv_parser_result: parser result
 */
static mv_parser_result
tag_selection(mv_parser_state *state, uint8_t t)
{
    mv_micheline_state *m = &state->micheline;
    uint8_t             nargs;
    uint8_t             annot;
    uint8_t             wrap;

    switch (t) {
    case MV_MICHELINE_TAG_INT:
        m->frame->step = MV_MICHELINE_STEP_INT;
        mv_parse_num_state_init(&state->buffers.num, &m->frame->step_int);
        for (int i = 0; i < (MV_NUM_BUFFER_SIZE / 8); i++) {
            state->buffers.num.bytes[i] = 0;
        }
        break;
    case MV_MICHELINE_TAG_SEQ:
        m->frame->step           = MV_MICHELINE_STEP_SEQ;
        m->frame->step_seq.first = true;
        mv_must(begin_sized(state));
        break;
    case MV_MICHELINE_TAG_BYTES:
        m->frame->step                    = MV_MICHELINE_STEP_BYTES;
        m->frame->step_bytes.first        = true;
        m->frame->step_bytes.has_rem_half = false;
        mv_must(begin_sized(state));
        break;
    case MV_MICHELINE_TAG_STRING:
        m->frame->step              = MV_MICHELINE_STEP_STRING;
        m->frame->step_string.first = true;
        mv_must(begin_sized(state));
        break;
    case MV_MICHELINE_TAG_PRIM_0_ANNOTS:
    case MV_MICHELINE_TAG_PRIM_0_NOANNOTS:
    case MV_MICHELINE_TAG_PRIM_1_ANNOTS:
    case MV_MICHELINE_TAG_PRIM_1_NOANNOTS:
    case MV_MICHELINE_TAG_PRIM_2_ANNOTS:
    case MV_MICHELINE_TAG_PRIM_2_NOANNOTS:
        nargs = (t - 3) >> 1;
        annot = (~t & 1);
        wrap  = (m->frame > m->stack)
               && (m->frame[-1].step == MV_MICHELINE_STEP_PRIM)
               && ((nargs > 0) || annot);
        goto common_prim;
    case MV_MICHELINE_TAG_PRIM_N:
        wrap = (m->frame > m->stack)
               && (m->frame[-1].step == MV_MICHELINE_STEP_PRIM);
        nargs = 3;
        annot = true;
    common_prim:
        m->frame->step            = MV_MICHELINE_STEP_PRIM_OP;
        m->frame->step_prim.ofs   = 0;
        m->frame->step_prim.nargs = nargs;
        m->frame->step_prim.wrap  = wrap;
        m->frame->step_prim.spc   = false;
        m->frame->step_prim.first = true;
        m->frame->step_prim.annot = annot;
        break;
    default:
        mv_raise(INVALID_TAG);
    }
    mv_continue;
}

mv_parser_result
mv_micheline_parser_step(mv_parser_state *state)
{
    mv_micheline_state *m = &state->micheline;
    uint8_t             b;
    uint8_t             op;
    uint8_t             t;

    // cannot restart after error
    if (MV_IS_ERR(state->errno)) {
        mv_reraise;
    }
    // nothing else to do
    if (state->micheline.frame == NULL) {
        mv_stop(DONE);
    }

    PRINTF(
        "[DEBUG] micheline(frame: %d, offset:%d/%d, step: %s, errno: %s)\n",
        (int)(m->frame - m->stack), (int)state->ofs, (int)m->frame->stop,
        (const char *)PIC(mv_micheline_parser_step_name[m->frame->step]),
        mv_parser_result_name(state->errno));

    switch (state->micheline.frame->step) {
    case MV_MICHELINE_STEP_INT:
        mv_must(mv_parser_read(state, &b));
        mv_must(
            mv_parse_int_step(&state->buffers.num, &m->frame->step_int, b));
        if (m->frame->step_int.stop) {
            m->frame->step          = MV_MICHELINE_STEP_PRINT_INT;
            m->frame->step_int.size = 0;
        }
        break;
    case MV_MICHELINE_STEP_PRINT_INT:
        if (m->frame->step_int.sign) {
            mv_must(parser_put(state, '-'));
            m->frame->step_int.sign = 0;
        } else if (state->buffers.num.decimal[m->frame->step_int.size]) {
            mv_must(parser_put(
                state, state->buffers.num.decimal[m->frame->step_int.size]));
            m->frame->step_int.size++;
        } else {
            mv_must(pop_frame(state));
        }
        break;
    case MV_MICHELINE_STEP_SIZE:
        mv_must(mv_parser_read(state, &b));
        if (m->frame->step_size.size > 255) {
            mv_raise(TOO_LARGE);  // enforce 16-bit restriction
        }
        m->frame->step_size.size = (m->frame->step_size.size << 8) | b;
        if (m->frame->stop == state->ofs) {
            m->frame[-1].stop = state->ofs + m->frame->step_size.size;
            mv_must(pop_frame(state));
        }
        break;
    case MV_MICHELINE_STEP_SEQ:
        if (m->frame->stop == state->ofs) {
            if (m->frame->step_seq.first) {
                mv_must(parser_put(state, '{'));
                m->frame->step_seq.first = false;
            } else {
                mv_must(parser_put(state, '}'));
                mv_must(pop_frame(state));
            }
        } else {
            if (m->frame->step_seq.first) {
                mv_must(parser_put(state, '{'));
                m->frame->step_seq.first = false;
            } else {
                mv_must(parser_put(state, ';'));
            }
            mv_must(push_frame(state, MV_MICHELINE_STEP_TAG));
        }
        break;
    case MV_MICHELINE_STEP_PRINT_CAPTURE:
        if (state->buffers
                .capture[state->micheline.frame->step_capture.ofs]) {
            mv_must(parser_put(
                state, state->buffers.capture[m->frame->step_capture.ofs]));
            m->frame->step_capture.ofs++;
        } else {
            mv_must(pop_frame(state));
        }
        break;
    case MV_MICHELINE_STEP_BYTES:
        if (m->frame->step_bytes.has_rem_half) {
            mv_must(parser_put(state, m->frame->step_bytes.rem_half));
            m->frame->step_bytes.has_rem_half = 0;
        } else if (state->micheline.frame->step_bytes.first) {
            mv_must(parser_put(state, '0'));
            m->frame->step_bytes.has_rem_half = true;
            m->frame->step_bytes.rem_half     = 'x';
            m->frame->step_bytes.first        = false;
        } else if (m->frame->stop == state->ofs) {
            mv_must(pop_frame(state));
        } else {
            char half;
            mv_must(mv_parser_peek(state, &b));
            half = hex_c[(b & 0xF0) >> 4];
            mv_must(parser_put(state, half));
            m->frame->step_bytes.has_rem_half = true;
            m->frame->step_bytes.rem_half     = hex_c[b & 0x0F];
            mv_parser_skip(state);
        }
        break;
    case MV_MICHELINE_STEP_STRING:
        if (m->frame->step_string.first) {
            mv_must(parser_put(state, '\"'));
            m->frame->step_string.first = false;
        } else if (m->frame->stop == state->ofs) {
            mv_must(parser_put(state, '\"'));
            mv_must(pop_frame(state));
        } else {
            mv_must(mv_parser_peek(state, &b));
            if ((b >= 0x20) && (b < 0x80) && (b != '\"') && (b != '\\')) {
                mv_must(parser_put(state, b));
                mv_parser_skip(state);
            } else {
                mv_parser_skip(state);
                mv_must(print_escaped(state, b));
            }
        }
        break;
    case MV_MICHELINE_STEP_ANNOT:
        if (m->frame->step_annot.first) {
            // after reading the size, copy the stop in
            // parent MV_MICHELINE_STEP_PRIM frame
            m->frame[-1].stop = m->frame->stop;
        }
        if (m->frame->stop == state->ofs) {
            mv_must(pop_frame(state));
        } else {
            if (m->frame->step_annot.first) {
                mv_must(parser_put(state, ' '));
                m->frame->step_annot.first = false;
            }
            mv_must(mv_parser_peek(state, &b));
            mv_must(parser_put(state, b));
            mv_parser_skip(state);
        }
        break;
    case MV_MICHELINE_STEP_PRIM_OP:
        mv_must(mv_parser_read(state, &op));
        if (mv_michelson_op_name(op) == NULL) {
            mv_raise(INVALID_OP);
        }
        m->frame->step         = MV_MICHELINE_STEP_PRIM_NAME;
        m->frame->step_prim.op = op;
        // clang-format off
        m->is_unit = ((m->frame == m->stack)
                      && (op == MV_MICHELSON_OP_Unit)
                      && (m->frame->step_prim.nargs == 0)
                      && (!m->frame->step_prim.annot));
        // clang-format on
        break;
    case MV_MICHELINE_STEP_PRIM_NAME:
        if (m->frame->step_prim.wrap && m->frame->step_prim.first) {
            mv_must(parser_put(state, '('));
            m->frame->step_prim.first = false;
        }
        if (mv_michelson_op_name(
                m->frame->step_prim.op)[m->frame->step_prim.ofs]) {
            mv_must(parser_put(
                state, mv_michelson_op_name(
                           m->frame->step_prim.op)[m->frame->step_prim.ofs]));
            m->frame->step_prim.ofs++;
        } else {
            m->frame->step = MV_MICHELINE_STEP_PRIM;
            if (m->frame->step_prim.nargs == 3) {
                mv_must(begin_sized(state));
            }
        }
        break;
    case MV_MICHELINE_STEP_PRIM:
        if ((m->frame->step_prim.nargs == 0)
            || ((m->frame->step_prim.nargs == 3)
                && (m->frame->stop == state->ofs))) {
            if (m->frame->step_prim.annot) {
                m->frame->step_prim.annot = false;
                mv_must(push_frame(state, MV_MICHELINE_STEP_ANNOT));
                m->frame->step_annot.first = true;
                mv_must(begin_sized(state));
            } else {
                if (m->frame->step_prim.wrap) {
                    mv_must(parser_put(state, ')'));
                }
                mv_must(pop_frame(state));
            }
        } else if (!m->frame->step_prim.spc) {
            mv_must(parser_put(state, ' '));
            m->frame->step_prim.spc = true;
        } else {
            if (m->frame->step_prim.nargs < 3) {
                m->frame->step_prim.nargs--;
            }
            m->frame->step_prim.spc = false;
            mv_must(push_frame(state, MV_MICHELINE_STEP_TAG));
        }
        break;
    case MV_MICHELINE_STEP_TAG:
        mv_must(mv_parser_read(state, &t));
        mv_must(tag_selection(state, t));
        break;
    default:
        mv_raise(INVALID_STATE);
    }
    mv_continue;
}
