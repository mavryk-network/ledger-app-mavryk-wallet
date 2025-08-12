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

#include "parser_state.h"

/**
 * @brief Helper to handle parser result case
 */
#define MV_LABEL(_x) \
    case MV_##_x:    \
        return #_x

/**
 * @brief Helper to handle blocking parser result case
 */
#define BLO_LABEL(_x) \
    case MV_BLO_##_x: \
        return #_x

const char *
mv_parser_result_name(mv_parser_result code)
{
    // clang-format off
    switch (code) {
    MV_LABEL(CONTINUE);
    MV_LABEL(BREAK);
    BLO_LABEL(DONE);
    BLO_LABEL(FEED_ME);
    BLO_LABEL(IM_FULL);
    MV_LABEL(ERR_INVALID_TAG);
    MV_LABEL(ERR_INVALID_OP);
    MV_LABEL(ERR_INVALID_DATA);
    MV_LABEL(ERR_UNSUPPORTED);
    MV_LABEL(ERR_TOO_LARGE);
    MV_LABEL(ERR_TOO_DEEP);
    MV_LABEL(ERR_INVALID_STATE);
    default:
        return "Unknown";
    }
    // clang-format on
}

void
mv_parser_init(mv_parser_state *state)
{
    state->errno                       = MV_CONTINUE;
    state->ofs                         = 0;
    state->field_info.field_name[0]    = 0;
    state->field_info.is_field_complex = false;
    state->field_info.field_index      = 0;
}

void
mv_parser_flush(mv_parser_state *st, char *obuf, size_t olen)
{
    mv_parser_flush_up_to(st, obuf, olen, olen);
}

void
mv_parser_flush_up_to(mv_parser_state *st, char *obuf, size_t olen,
                      size_t up_to)
{
    mv_parser_regs *regs = &st->regs;

    regs->obuf = obuf;
    regs->oofs = 0;
    regs->olen = olen;

    size_t len = strlen(regs->obuf + up_to);
    regs->oofs += len;
    regs->olen -= len;

    memmove(regs->obuf, regs->obuf + up_to, len);
    memset(regs->obuf + regs->oofs, 0x0, regs->olen);
}

void
mv_parser_refill(mv_parser_state *st, const uint8_t *ibuf, size_t ilen)
{
    mv_parser_regs *regs = &st->regs;

    regs->ibuf = ibuf;
    regs->iofs = 0;
    regs->ilen = ilen;
}

mv_parser_result
mv_parser_set_errno(mv_parser_state *state, mv_parser_result code)
{
    state->errno = ((code == MV_BREAK) ? MV_CONTINUE : code);
    return code;
}

mv_parser_result
mv_parser_put(mv_parser_state *state, char c)
{
    mv_parser_regs *regs = &state->regs;

    if (regs->olen < 1) {
        mv_stop(IM_FULL);
    }
    regs->obuf[regs->oofs] = c;
    regs->oofs++;
    regs->olen--;
    mv_continue;
}

mv_parser_result
mv_parser_read(mv_parser_state *state, uint8_t *r)
{
    mv_parser_regs *regs = &state->regs;

    if (regs->ilen < 1) {
        mv_stop(FEED_ME);
    }
    state->ofs++;
    regs->ilen--;
    *r = regs->ibuf[regs->iofs++];
    mv_continue;
}

mv_parser_result
mv_parser_peek(mv_parser_state *state, uint8_t *r)
{
    mv_parser_regs *regs = &state->regs;

    if (regs->ilen < 1) {
        mv_stop(FEED_ME);
    }
    *r = regs->ibuf[regs->iofs];
    mv_continue;
}

void
mv_parser_skip(mv_parser_state *state)
{
    mv_parser_regs *regs = &state->regs;

    regs->iofs++;
    regs->ilen--;
    state->ofs++;
}
