/* Tezos Ledger application - Clear signing command handler

   Copyright 2023 Nomadic Labs <contact@nomadic-labs.com>
   Copyright 2023 TriliTech <contact@trili.tech>
   Copyright 2023 Functori <contact@functori.com>

   With code excerpts from:
    - Legacy Tezos app, Copyright 2019 Obsidian Systems
    - Ledger Blue sample apps, Copyright 2016 Ledger

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include <memory.h>
#include <string.h>
#include <stdbool.h>

#include <buffer.h>
#include <cx.h>
#include <io.h>
#include <os.h>
#include <ux.h>

#ifdef HAVE_SWAP
#include <swap.h>
#endif

#include "../format.h"
#include "globals.h"
#include "handle_swap.h"
#include "keys.h"
#include "sign.h"
#include "ui_stream.h"

#include "parser/parser_state.h"
#include "parser/operation_parser.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

/* Prototypes */

static void sign_packet(void);
static void send_reject(int error_code);
static void send_continue(void);
static void send_cancel(void);
static void refill(void);
static void refill_all(void);
static void stream_cb(mv_ui_cb_type_t cb_type);
static void start_displaying_signature_review(void);
static void init_blind_stream(void);
static void handle_data_apdu_clear(buffer_t *cdata, bool last);
static void handle_data_apdu_blind(void);
static void pass_from_clear_to_summary(void);
#ifdef HAVE_BAGL
static void init_too_many_screens_stream(void);
#endif
#ifdef HAVE_NBGL
static void continue_blindsign_cb(void);
static void pass_from_summary_to_blind(void);
#endif
static void init_summary_stream(void);

/* Macros */

#define APDU_SIGN_ASSERT(_cond) MV_ASSERT(EXC_UNEXPECTED_SIGN_STATE, (_cond))
#define APDU_SIGN_ASSERT_STEP(x) \
    APDU_SIGN_ASSERT(global.keys.apdu.sign.step == (x))

#ifdef HAVE_BAGL
#define SCREEN_DISPLAYED global.keys.apdu.sign.u.clear.screen_displayed
#endif

#ifdef HAVE_BAGL
void
mv_ui_stream_push_accept_reject(void)
{
    FUNC_ENTER(("void"));
#ifdef TARGET_NANOS
    mv_ui_stream_push(MV_UI_STREAM_CB_ACCEPT, "Accept and send", "",
                      MV_UI_LAYOUT_HOME_PB, MV_UI_ICON_TICK);
#else
    mv_ui_stream_push(MV_UI_STREAM_CB_ACCEPT, "Accept", "and send",
                      MV_UI_LAYOUT_HOME_PB, MV_UI_ICON_TICK);
#endif
    mv_ui_stream_push(MV_UI_STREAM_CB_REJECT, "Reject", "",
                      MV_UI_LAYOUT_HOME_PB, MV_UI_ICON_CROSS);
    FUNC_LEAVE();
}

void
mv_ui_stream_push_risky_accept_reject(mv_ui_cb_type_t accept_cb_type,
                                      mv_ui_cb_type_t reject_cb_type)
{
    FUNC_ENTER(("void"));
    mv_ui_stream_push(accept_cb_type, "Accept risk", "", MV_UI_LAYOUT_HOME_PB,
                      MV_UI_ICON_TICK);
    mv_ui_stream_push(reject_cb_type, "Reject", "", MV_UI_LAYOUT_HOME_PB,
                      MV_UI_ICON_CROSS);
    FUNC_LEAVE();
}

static void
mv_ui_stream_push_warning_not_trusted(const char *title_reason,
                                      const char *value_reason)
{
    FUNC_ENTER(("void"));
#ifdef TARGET_NANOS
    mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, "The transaction",
                      "cannot be trusted.", MV_UI_LAYOUT_HOME_B,
                      MV_UI_ICON_NONE);
#else
    mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, "The transaction",
                      "cannot be trusted.", MV_UI_LAYOUT_HOME_PB,
                      MV_UI_ICON_WARNING);
#endif
    if ((title_reason != NULL) && (value_reason != NULL)) {
        mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, title_reason, value_reason,
                          MV_UI_LAYOUT_HOME_N, MV_UI_ICON_NONE);
    }
#ifndef TARGET_NANOS
    mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, "It may not be safe",
                      "to sign this\ntransaction.", MV_UI_LAYOUT_HOME_N,
                      MV_UI_ICON_NONE);
#endif
    FUNC_LEAVE();
}

void
mv_ui_stream_push_learn_more(void)
{
    FUNC_ENTER(("void"));
    mv_ui_stream_push(MV_UI_STREAM_CB_NOCB,
                      "Learn More:", "bit.ly/ledger-tez",
                      MV_UI_LAYOUT_HOME_BN, MV_UI_ICON_NONE);
    FUNC_LEAVE();
}
#endif

static void
sign_packet(void)
{
    buffer_t bufs[2] = {0};
    uint8_t  sig[MAX_SIGNATURE_SIZE];
    MV_PREAMBLE(("void"));

    APDU_SIGN_ASSERT_STEP(SIGN_ST_WAIT_USER_INPUT);
    APDU_SIGN_ASSERT(global.keys.apdu.sign.received_last_msg);

    MV_CHECK(swap_check_validity());

    bufs[0].ptr  = global.keys.apdu.hash.final_hash;
    bufs[0].size = sizeof(global.keys.apdu.hash.final_hash);
    bufs[1].ptr  = sig;
    bufs[1].size = sizeof(sig);
    MV_CHECK(sign(global.path_with_curve.derivation_type,
                  &global.path_with_curve.bip32_path, bufs[0].ptr,
                  bufs[0].size, sig, &bufs[1].size));

    /* If we aren't returning the hash, zero its buffer. */
    if (!global.keys.apdu.sign.return_hash) {
        memset((void *)bufs[0].ptr, 0, bufs[0].size);
        bufs[0].size = 0;
    }

    io_send_response_buffers(bufs, 2, SW_OK);
    global.step = ST_IDLE;
    MV_POSTAMBLE;
}

static void
send_reject(int error_code)
{
    MV_PREAMBLE(("void"));

    APDU_SIGN_ASSERT_STEP(SIGN_ST_WAIT_USER_INPUT);
    MV_FAIL(error_code);
    MV_POSTAMBLE;
}

static void
send_continue(void)
{
    MV_PREAMBLE(("void"));

    APDU_SIGN_ASSERT((global.keys.apdu.sign.step == SIGN_ST_WAIT_USER_INPUT)
                     || (global.keys.apdu.sign.step == SIGN_ST_WAIT_DATA));
    APDU_SIGN_ASSERT(!global.keys.apdu.sign.received_last_msg);

    if (global.keys.apdu.sign.u.clear.received_msg) {
        global.keys.apdu.sign.u.clear.received_msg = false;
        io_send_sw(SW_OK);
    }

    global.keys.apdu.sign.step = SIGN_ST_WAIT_DATA;

    MV_POSTAMBLE;
}

static void
refill_blo_im_full(void)
{
    mv_parser_state *st    = &global.keys.apdu.sign.u.clear.parser_state;
    size_t           wrote = 0;
    MV_PREAMBLE(("void"));

    // No display for Swap or Summary flow
    if (
#ifdef HAVE_SWAP
        G_called_from_swap ||
#endif
        global.step == ST_SUMMARY_SIGN) {
        mv_parser_flush(st, global.line_buf, MV_UI_STREAM_CONTENTS_SIZE);
        // invoke refill until we consume entire msg.
        MV_SUCCEED();
    }

    global.keys.apdu.sign.step = SIGN_ST_WAIT_USER_INPUT;
#ifdef HAVE_BAGL
    if (N_settings.blindsigning
        && (SCREEN_DISPLAYED >= NB_MAX_SCREEN_ALLOWED)) {
        pass_from_clear_to_summary();
        MV_SUCCEED();
    }

    if (st->field_info.is_field_complex && !N_settings.expert_mode) {
        mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, st->field_info.field_name,
                          "Needs Expert mode", MV_UI_LAYOUT_HOME_B,
                          MV_UI_ICON_NONE);
        mv_ui_stream_push(MV_UI_STREAM_CB_REJECT, "Home", "",
                          MV_UI_LAYOUT_HOME_PB, MV_UI_ICON_BACK);
        mv_ui_stream_close();
        goto end;
    } else {
        if (st->field_info.is_field_complex
            && !global.keys.apdu.sign.u.clear.displayed_expert_warning) {
            SCREEN_DISPLAYED++;
            mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, "Next field requires",
                              "careful review", MV_UI_LAYOUT_HOME_B,
                              MV_UI_ICON_NONE);
            global.keys.apdu.sign.u.clear.last_field_index
                = st->field_info.field_index;
            global.keys.apdu.sign.u.clear.displayed_expert_warning = true;
        }
    }

    SCREEN_DISPLAYED++;
    wrote = mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, st->field_info.field_name,
                              global.line_buf, MV_UI_LAYOUT_BN,
                              MV_UI_ICON_NONE);

#elif HAVE_NBGL
    PRINTF("[DEBUG] field=%s complex=%d\n", st->field_info.field_name,
           st->field_info.is_field_complex);
    if (st->field_info.is_field_complex
        && !global.keys.apdu.sign.u.clear.displayed_expert_warning) {
        global.keys.apdu.sign.u.clear.last_field_index
            = st->field_info.field_index;
        global.keys.apdu.sign.u.clear.displayed_expert_warning = true;
        if (!N_settings.expert_mode) {
            mv_ui_stream_push_all(MV_UI_STREAM_CB_EXPERT_MODE_ENABLE,
                                  st->field_info.field_name, "complex",
                                  MV_UI_LAYOUT_BN, MV_UI_ICON_NONE);
        }

        wrote = mv_ui_stream_push_all(
            MV_UI_STREAM_CB_EXPERT_MODE_FIELD, st->field_info.field_name,
            global.line_buf, MV_UI_LAYOUT_BN, MV_UI_ICON_NONE);
    } else {
        wrote = mv_ui_stream_push(MV_UI_STREAM_CB_NOCB,
                                  st->field_info.field_name, global.line_buf,
                                  MV_UI_LAYOUT_BN, MV_UI_ICON_NONE);
    }

#endif
    mv_parser_flush_up_to(st, global.line_buf, MV_UI_STREAM_CONTENTS_SIZE,
                          wrote);
    MV_POSTAMBLE;
}

static void
refill_blo_done(void)
{
    mv_parser_state *st = &global.keys.apdu.sign.u.clear.parser_state;
    MV_PREAMBLE(("void"));

    MV_ASSERT(
        EXC_UNEXPECTED_STATE,
        (global.keys.apdu.sign.received_last_msg && st->regs.ilen) == 0);

    global.keys.apdu.sign.u.clear.received_msg = false;
    if (st->regs.oofs != 0) {
        refill_blo_im_full();
        MV_SUCCEED();
    }

    global.keys.apdu.sign.step = SIGN_ST_WAIT_USER_INPUT;
    if (global.step == ST_SWAP_SIGN) {
        MV_CHECK(sign_packet());
        MV_SUCCEED();
    }

#ifdef HAVE_BAGL
    if (global.step == ST_SUMMARY_SIGN) {
        init_too_many_screens_stream();
        MV_SUCCEED();
    }
    mv_ui_stream_push_accept_reject();
#endif

#ifdef HAVE_NBGL
    if (global.step == ST_SUMMARY_SIGN) {
        init_summary_stream();
        MV_SUCCEED();
    }
#endif

    mv_ui_stream_close();
    MV_POSTAMBLE;
}

static void
refill_error(void)
{
    mv_parser_state *st = &global.keys.apdu.sign.u.clear.parser_state;
    MV_PREAMBLE(("void"));

    global.keys.apdu.sign.step = SIGN_ST_WAIT_USER_INPUT;
#ifdef HAVE_SWAP
    if (G_called_from_swap) {
        global.keys.apdu.sign.u.clear.received_msg = false;
        MV_FAIL(EXC_PARSE_ERROR);
    }
#endif

    // clang-format off
#ifdef HAVE_BAGL
    mv_ui_stream_init(stream_cb);

#ifdef TARGET_NANOS
    mv_ui_stream_push_warning_not_trusted(NULL, NULL);
#else
    mv_ui_stream_push_warning_not_trusted("This transaction",
                                          "could not be\ndecoded correctly.");
#endif

    mv_ui_stream_push_all(MV_UI_STREAM_CB_NOCB,
                          "Parsing error",
                          mv_parser_result_name(st->errno),
                          MV_UI_LAYOUT_HOME_BN,
                          MV_UI_ICON_NONE);
    mv_ui_stream_push_learn_more();
    mv_ui_stream_push_risky_accept_reject(
        MV_UI_STREAM_CB_BLINDSIGN, MV_UI_STREAM_CB_CANCEL);

#elif HAVE_NBGL
    global.blindsign_reason = REASON_PARSING_ERROR;
    strncpy(global.error_code, mv_parser_result_name(st->errno), ERROR_CODE_SIZE);
    if(global.step == ST_SUMMARY_SIGN) {
        switch_to_blindsigning_on_error();
        MV_SUCCEED();
    }
    else if(global.step == ST_CLEAR_SIGN){
        // The following call is just to invoke switch_to_blindsigning
        // with MV_UI_STREAM_CB_CANCEL callback type in function mv_ui_nav_cb()
        // The text will not be shown.
        mv_ui_stream_push_all(MV_UI_STREAM_CB_CANCEL,
                              "Parsing error",
                              mv_parser_result_name(st->errno),
                              MV_UI_LAYOUT_BN,
                              MV_UI_ICON_NONE);
    }
    else{
        MV_FAIL(EXC_UNEXPECTED_STATE); // Only two states can lead to refill error. ST_CLEAR_SIGN and ST_SUMMARY_SIGN
    }
#endif
    // clang-format on

    mv_ui_stream_close();

#ifdef HAVE_BAGL
    mv_ui_stream_start();
#endif

    MV_POSTAMBLE;
}

static void
refill(void)
{
    mv_parser_state *st = &global.keys.apdu.sign.u.clear.parser_state;
    MV_PREAMBLE(("void"));

    while (!MV_IS_BLOCKED(mv_operation_parser_step(st))) {
        // Loop while the result is successful and not blocking
    }
    PRINTF("[DEBUG] refill(errno: %s)\n", mv_parser_result_name(st->errno));
    // clang-format off
    switch (st->errno) {
    case MV_BLO_IM_FULL: MV_CHECK(refill_blo_im_full());
        break;
    case MV_BLO_FEED_ME: MV_CHECK(send_continue());
        break;
    case MV_BLO_DONE: MV_CHECK(refill_blo_done());
        break;
    default: MV_CHECK(refill_error());
        break;
    }
    // clang-format on
    MV_POSTAMBLE;
}

/**
 * @brief Parse until there is nothing left to parse or user input is
 * required.
 */
static void
refill_all(void)
{
    MV_PREAMBLE(("void"));

    while (global.keys.apdu.sign.u.clear.received_msg) {
        MV_CHECK(refill());
        if ((global.step == ST_SUMMARY_SIGN)
            && (global.keys.apdu.sign.step == SIGN_ST_WAIT_USER_INPUT)) {
            break;
        }
    }

    MV_POSTAMBLE;
}

static void
pass_from_clear_to_summary(void)
{
    MV_PREAMBLE(("void"));

    APDU_SIGN_ASSERT_STEP(SIGN_ST_WAIT_USER_INPUT);

    global.step                = ST_SUMMARY_SIGN;
    global.keys.apdu.sign.step = SIGN_ST_WAIT_DATA;
#ifdef HAVE_NBGL
    init_blind_stream();
#endif
    MV_CHECK(refill_all());

    MV_POSTAMBLE;
}

static void
send_cancel(void)
{
    mv_parser_state *st = &global.keys.apdu.sign.u.clear.parser_state;
    MV_PREAMBLE(("void"));

    global.keys.apdu.sign.step = SIGN_ST_IDLE;

    switch (st->errno) {
    case MV_ERR_INVALID_STATE:
        MV_FAIL(EXC_UNEXPECTED_STATE);
        break;
    case MV_ERR_INVALID_TAG:
    case MV_ERR_INVALID_OP:
    case MV_ERR_INVALID_DATA:
    case MV_ERR_UNSUPPORTED:
    case MV_ERR_TOO_LARGE:
    case MV_ERR_TOO_DEEP:
        MV_FAIL(EXC_PARSE_ERROR);
        break;
    default:
        MV_FAIL(EXC_UNEXPECTED_STATE);
    }

    MV_POSTAMBLE;
}

static void
pass_from_clear_to_blind(void)
{
    MV_PREAMBLE(("void"));

    APDU_SIGN_ASSERT_STEP(SIGN_ST_WAIT_USER_INPUT);

    global.step                        = ST_BLIND_SIGN;
    global.keys.apdu.sign.step         = SIGN_ST_WAIT_DATA;
    global.keys.apdu.sign.u.blind.step = BLINDSIGN_ST_OPERATION;

    init_blind_stream();
    handle_data_apdu_blind();

    MV_POSTAMBLE;
}

static void
stream_cb(mv_ui_cb_type_t cb_type)
{
    MV_PREAMBLE(("cb_type=%u", cb_type));

    // clang-format off
    switch (cb_type) {
    case MV_UI_STREAM_CB_ACCEPT:           MV_CHECK(sign_packet());                break;
    case MV_UI_STREAM_CB_REFILL:           MV_CHECK(refill());                     break;
    case MV_UI_STREAM_CB_REJECT:           send_reject(EXC_REJECT);                break;
    case MV_UI_STREAM_CB_BLINDSIGN_REJECT: send_reject(EXC_PARSE_ERROR);           break;
    case MV_UI_STREAM_CB_CANCEL:           MV_CHECK(send_cancel());                break;
#ifdef HAVE_BAGL
    case MV_UI_STREAM_CB_BLINDSIGN:        MV_CHECK(pass_from_clear_to_blind());   break;
#else  // HAVE_NBGL
    case MV_UI_STREAM_CB_BLINDSIGN:
        if (global.step == ST_CLEAR_SIGN) {
            MV_CHECK(pass_from_clear_to_blind());
        } else if (global.step == ST_SUMMARY_SIGN) {
            MV_CHECK(pass_from_summary_to_blind());
        } else {
            MV_FAIL(EXC_UNEXPECTED_STATE);
        }
        break;
    case MV_UI_STREAM_CB_SUMMARY:          MV_CHECK(pass_from_clear_to_summary()); break;
#endif
    default: MV_FAIL(EXC_UNKNOWN);                                                 break;
    }
    // clang-format on

    MV_POSTAMBLE;
}

#ifdef HAVE_BAGL
static void
push_next_summary_screen(void)
{
#define FINAL_HASH       global.keys.apdu.hash.final_hash
#define SUMMARYSIGN_STEP global.keys.apdu.sign.u.summary.step

    char num_buffer[MV_DECIMAL_BUFFER_SIZE(MV_NUM_BUFFER_SIZE / 8)] = {0};
    char hash_buffer[MV_BASE58_BUFFER_SIZE(sizeof(FINAL_HASH))]     = {0};
    mv_operation_state *op
        = &global.keys.apdu.sign.u.clear.parser_state.operation;

    MV_PREAMBLE(("void"));

    switch (SUMMARYSIGN_STEP) {
    case SUMMARYSIGN_ST_OPERATION:
        SUMMARYSIGN_STEP = SUMMARYSIGN_ST_NB_TX;

        snprintf(num_buffer, sizeof(num_buffer), "%d", op->batch_index);
        mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, "Number of Tx", num_buffer,
                          MV_UI_LAYOUT_BN, MV_UI_ICON_NONE);
        break;
    case SUMMARYSIGN_ST_NB_TX:
        SUMMARYSIGN_STEP = SUMMARYSIGN_ST_AMOUNT;

        mv_mumav_to_string(num_buffer, sizeof(num_buffer), op->total_amount);
        mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, "Total amount", num_buffer,
                          MV_UI_LAYOUT_BN, MV_UI_ICON_NONE);
        break;
    case SUMMARYSIGN_ST_AMOUNT:
        SUMMARYSIGN_STEP = SUMMARYSIGN_ST_FEE;

        mv_mumav_to_string(num_buffer, sizeof(num_buffer), op->total_fee);
        mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, "Total fee", num_buffer,
                          MV_UI_LAYOUT_BN, MV_UI_ICON_NONE);
        break;
    case SUMMARYSIGN_ST_FEE:
        SUMMARYSIGN_STEP = SUMMARYSIGN_ST_HASH;

        if (mv_format_base58(FINAL_HASH, sizeof(FINAL_HASH), hash_buffer,
                             sizeof(hash_buffer))) {
            MV_FAIL(EXC_UNKNOWN);
        }
        mv_ui_stream_push_all(MV_UI_STREAM_CB_NOCB, "Hash", hash_buffer,
                              MV_UI_LAYOUT_BN, MV_UI_ICON_NONE);
        break;
    case SUMMARYSIGN_ST_HASH:
        SUMMARYSIGN_STEP = SUMMARYSIGN_ST_ACCEPT_REJECT;

        mv_ui_stream_push_accept_reject();
        mv_ui_stream_close();
        break;
    default:
        PRINTF("Unexpected summary state: %d\n", (int)SUMMARYSIGN_STEP);
        MV_FAIL(EXC_UNEXPECTED_STATE);
    };

    MV_POSTAMBLE;

#undef SUMMARYSIGN_STEP
#undef FINAL_HASH
}

static void
summary_stream_cb(mv_ui_cb_type_t cb_type)
{
    MV_PREAMBLE(("cb_type=%u", cb_type));

    // clang-format off
    switch (cb_type) {
    case MV_UI_STREAM_CB_ACCEPT: MV_CHECK(sign_packet());               break;
    case MV_UI_STREAM_CB_REJECT:  send_reject(EXC_REJECT);              break;
    case MV_UI_STREAM_CB_REFILL: MV_CHECK(push_next_summary_screen());  break;
    default: MV_FAIL(EXC_UNKNOWN);
        break;
    }
    // clang-format on

    MV_POSTAMBLE;
}

#endif

static void
init_summary_stream(void)
{
    MV_PREAMBLE(("void"));
#ifdef HAVE_BAGL
    mv_ui_stream_init(summary_stream_cb);
    global.keys.apdu.sign.u.summary.step = SUMMARYSIGN_ST_OPERATION;
    push_next_summary_screen();
    mv_ui_stream();
#elif defined(HAVE_NBGL)
    continue_blindsign_cb();
#endif

    MV_POSTAMBLE;
}

#define FINAL_HASH global.keys.apdu.hash.final_hash

#ifdef HAVE_BAGL
static void
pass_to_summary_stream_cb(mv_ui_cb_type_t cb_type)
{
    MV_PREAMBLE(("cb_type=%u", cb_type));

    // clang-format off
    switch (cb_type) {
    case MV_UI_STREAM_CB_VALIDATE: MV_CHECK(init_summary_stream()); break;
    case MV_UI_STREAM_CB_REJECT:   send_reject(EXC_REJECT);                break;
    default:                       MV_FAIL(EXC_UNKNOWN);                   break;
    }
    // clang-format on

    MV_POSTAMBLE;
}

static void
init_too_many_screens_stream(void)
{
    mv_ui_stream_init(pass_to_summary_stream_cb);

#ifdef TARGET_NANOS
    mv_ui_stream_push_warning_not_trusted("Operation too long",
                                          "Accept blindsign");
#else
    mv_ui_stream_push_warning_not_trusted("Operation too long",
                                          "Proceed to\nblindsign.");
#endif
    mv_ui_stream_push_risky_accept_reject(MV_UI_STREAM_CB_VALIDATE,
                                          MV_UI_STREAM_CB_REJECT);

    mv_ui_stream_close();

    mv_ui_stream();
}

static void
bs_push_next(void)
{
#define BLINDSIGN_STEP global.keys.apdu.sign.u.blind.step

    char obuf[MV_BASE58_BUFFER_SIZE(sizeof(FINAL_HASH))];

    MV_PREAMBLE(("void"));

    switch (BLINDSIGN_STEP) {
    case BLINDSIGN_ST_OPERATION:
        BLINDSIGN_STEP = BLINDSIGN_ST_HASH;

        if (mv_format_base58(FINAL_HASH, sizeof(FINAL_HASH), obuf,
                             sizeof(obuf))) {
            MV_FAIL(EXC_UNKNOWN);
        }

        mv_ui_stream_push_all(MV_UI_STREAM_CB_NOCB, "Sign Hash", obuf,
                              MV_UI_LAYOUT_BN, MV_UI_ICON_NONE);
        break;
    case BLINDSIGN_ST_HASH:
        BLINDSIGN_STEP = BLINDSIGN_ST_ACCEPT_REJECT;

        mv_ui_stream_push_accept_reject();
        mv_ui_stream_close();
        break;
    default:
        PRINTF("Unexpected blindsign state: %d\n", (int)BLINDSIGN_STEP);
        MV_FAIL(EXC_UNEXPECTED_STATE);
    };

    MV_POSTAMBLE;

#undef BLINDSIGN_STEP
}

static void
bs_stream_cb(mv_ui_cb_type_t cb_type)
{
    MV_PREAMBLE(("cb_type=%u", cb_type));

    // clang-format off
    switch (cb_type) {
    case MV_UI_STREAM_CB_ACCEPT:
        return sign_packet();
    case MV_UI_STREAM_CB_REFILL:
        return bs_push_next();
    case MV_UI_STREAM_CB_REJECT:
        return send_reject(EXC_REJECT);
    case MV_UI_STREAM_CB_CANCEL:
        return send_cancel();
    default: MV_FAIL(EXC_UNKNOWN);
    }
    // clang-format on

    MV_POSTAMBLE;
}
#endif  // HAVE_BAGL

void
handle_signing_key_setup(buffer_t *cdata, derivation_type_t derivation_type,
                         bool return_hash)
{
    MV_PREAMBLE(("cdata=%p, derivation_type=%d, return_hash=%d", cdata,
                 derivation_type, return_hash));

    MV_ASSERT_NOTNULL(cdata);

    memset(&global.keys, 0, sizeof(global.keys));
    global.keys.apdu.sign.return_hash = return_hash;

    MV_LIB_CHECK(read_bip32_path(&global.path_with_curve.bip32_path, cdata));
    global.path_with_curve.derivation_type = derivation_type;

    CX_CHECK(cx_blake2b_init_no_throw(&global.keys.apdu.hash.state,
                                      SIGN_HASH_SIZE * 8));
    /*
     * We set the tag to zero here which indicates that it is unset.
     * The first data packet will set it to the first byte.
     */
    global.keys.apdu.sign.tag = 0;

    MV_CHECK(start_displaying_signature_review());

    MV_ASSERT(EXC_UNEXPECTED_STATE, (global.step == ST_CLEAR_SIGN)
                                        || (global.step == ST_SWAP_SIGN));

    io_send_sw(SW_OK);
    global.keys.apdu.sign.step = SIGN_ST_WAIT_DATA;

    MV_POSTAMBLE;
}

static void
start_displaying_signature_review(void)
{
    MV_PREAMBLE(("global.step=%d", global.step));
    mv_parser_state *st = &global.keys.apdu.sign.u.clear.parser_state;

    global.keys.apdu.sign.u.clear.received_msg = false;
    // NO ui display during swap.
#ifdef HAVE_SWAP
    if (!G_called_from_swap) {
#endif
        mv_ui_stream_init(stream_cb);
        global.step = ST_CLEAR_SIGN;

#ifdef HAVE_BAGL
#ifdef TARGET_NANOS
        mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, "Review operation", "",
                          MV_UI_LAYOUT_HOME_PB, MV_UI_ICON_EYE);
#else
        mv_ui_stream_push(MV_UI_STREAM_CB_NOCB, "Review", "operation",
                          MV_UI_LAYOUT_HOME_PB, MV_UI_ICON_EYE);
#endif
#endif
#ifdef HAVE_SWAP
    } else {
        PRINTF("[DEBUG] If called from SWAP : global.step =%d\n",
               global.step);
    }
#endif
    mv_operation_parser_init(st, MV_UNKNOWN_SIZE, false);
    mv_parser_refill(st, NULL, 0);
    mv_parser_flush(st, global.line_buf, MV_UI_STREAM_CONTENTS_SIZE);

    MV_POSTAMBLE;
}

static void
init_blind_stream(void)
{
#ifdef HAVE_BAGL
    mv_ui_stream_init(bs_stream_cb);
#elif HAVE_NBGL
    nbgl_useCaseSpinner("Loading operation");
#endif
}

void
handle_sign(buffer_t *cdata, bool last, bool return_hash)
{
    MV_PREAMBLE(("cdata=%p, last=%d, return_hash=%d, \nglobal.step: %d",
                 cdata, last, return_hash, global.step));

    MV_ASSERT_NOTNULL(cdata);
    APDU_SIGN_ASSERT_STEP(SIGN_ST_WAIT_DATA);
    MV_ASSERT(EXC_INVALID_INS,
              return_hash == global.keys.apdu.sign.return_hash);

    global.keys.apdu.sign.packet_index++;  // XXX drop or check

    CX_CHECK(cx_hash_no_throw((cx_hash_t *)&global.keys.apdu.hash.state,
                              last ? CX_LAST : 0, cdata->ptr, cdata->size,
                              global.keys.apdu.hash.final_hash,
                              sizeof(global.keys.apdu.hash.final_hash)));

    if (last) {
        global.keys.apdu.sign.received_last_msg = true;
    }

    if (!global.keys.apdu.sign.tag) {
        MV_ASSERT(EXC_PARSE_ERROR, buffer_can_read(cdata, 1));

        global.keys.apdu.sign.tag = cdata->ptr[0];
    }

    switch (global.step) {
    case ST_CLEAR_SIGN:
    case ST_SWAP_SIGN:
    case ST_SUMMARY_SIGN:
        MV_CHECK(handle_data_apdu_clear(cdata, last));
        break;
    case ST_BLIND_SIGN:
        MV_CHECK(handle_data_apdu_blind());
        break;
    default:
        MV_FAIL(EXC_UNEXPECTED_STATE);
        break;
    }

    MV_POSTAMBLE;
}

static void
handle_data_apdu_clear(buffer_t *cdata, bool last)
{
    MV_PREAMBLE(("cdata=0x%p, last=%d", cdata, last));

    MV_ASSERT_NOTNULL(cdata);

    mv_parser_state *st = &global.keys.apdu.sign.u.clear.parser_state;

    // check we consume all input before asking for more
    MV_ASSERT(EXC_UNEXPECTED_SIGN_STATE, st->regs.ilen == 0);

    global.keys.apdu.sign.u.clear.received_msg = true;

    global.keys.apdu.sign.u.clear.total_length += cdata->size;

    mv_parser_refill(st, cdata->ptr, cdata->size);
    if (last) {
        mv_operation_parser_set_size(
            st, global.keys.apdu.sign.u.clear.total_length);
    }

    switch (global.step) {
    case ST_CLEAR_SIGN:
        MV_CHECK(refill());
        if (global.keys.apdu.sign.step == SIGN_ST_WAIT_USER_INPUT) {
            mv_ui_stream();
        }
        break;
    case ST_SWAP_SIGN:
    case ST_SUMMARY_SIGN:
        MV_CHECK(refill_all());
        break;
    default:
        MV_FAIL(EXC_UNEXPECTED_SIGN_STATE);
        break;
    }

    MV_POSTAMBLE;
}

#define OPERATION_TYPE_STR_LENGTH 22

static void
get_blindsign_type(char *type, size_t type_size)
{
    MV_PREAMBLE(("type=%s", type));
    MV_ASSERT(EXC_MEMORY_ERROR, type_size >= OPERATION_TYPE_STR_LENGTH);
    // clang-format off
    switch (global.keys.apdu.sign.tag) {
    case 0x01:
    case 0x11:
        memcpy(type,"Block\nproposal", OPERATION_TYPE_STR_LENGTH);
        break;
    case 0x03:
        memcpy(type,"Manager\noperation", OPERATION_TYPE_STR_LENGTH);
        break;
    case 0x02:
    case 0x12:
    case 0x13:
        memcpy(type,"Consensus\noperation", OPERATION_TYPE_STR_LENGTH);
        break;
    case 0x05:
        memcpy(type,"Micheline\nexpression", OPERATION_TYPE_STR_LENGTH);
        break;
    default:
        break;
    }
    // clang-format on
    MV_POSTAMBLE;
}

#ifdef HAVE_NBGL
static void
pass_from_summary_to_blind(void)
{
    MV_PREAMBLE(("void"));

    MV_ASSERT(EXC_UNEXPECTED_STATE, global.step == ST_SUMMARY_SIGN);

    global.step                        = ST_BLIND_SIGN;
    global.keys.apdu.sign.step         = SIGN_ST_WAIT_DATA;
    global.keys.apdu.sign.u.blind.step = BLINDSIGN_ST_OPERATION;

    init_blind_stream();
    handle_data_apdu_blind();

    MV_POSTAMBLE;
}

static nbgl_layoutTagValueList_t useCaseTagValueList;

void
accept_blindsign_cb(void)
{
    FUNC_ENTER(("void"));

    stream_cb(MV_UI_STREAM_CB_ACCEPT);
    ui_home_init();

    FUNC_LEAVE();
}

static void
reviewChoice(bool confirm)
{
    FUNC_ENTER(("confirm=%d", confirm));

    if (confirm) {
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED,
                                 accept_blindsign_cb);
    } else {
        mv_reject();
    }

    FUNC_LEAVE();
}

typedef enum {
    SUMMARY_INDEX_NB_OF_TX = 0,
    SUMMARY_INDEX_TOTAL_AMOUNT,
    SUMMARY_INDEX_TOTAL_FEES,
    SUMMARY_INDEX_TYPE,
    SUMMARY_INDEX_HASH,
    SUMMARY_INDEX_MAX
} summary_index_t;

#define DECIMAL_SIZE MV_DECIMAL_BUFFER_SIZE((MV_NUM_BUFFER_SIZE / 8))

static nbgl_layoutTagValue_t pair;

static nbgl_layoutTagValue_t *
getTagValuePair(uint8_t pairIndex)
{
    MV_PREAMBLE(("pairIndex=%u", pairIndex));
    // Reuse the buffer for tag value pair list.
    mv_operation_state *op
        = &global.keys.apdu.sign.u.clear.parser_state.operation;

    /// Following condition is setup because startIndex in useCaseTagValueList
    /// is not being used by the SDK.
    if (global.step == ST_BLIND_SIGN && useCaseTagValueList.nbPairs == 2) {
        if (pairIndex < SUMMARY_INDEX_TOTAL_FEES) {
            pairIndex += SUMMARY_INDEX_TYPE;
        }
    }

    char num_buffer[DECIMAL_SIZE]        = {0};
    char type[OPERATION_TYPE_STR_LENGTH] = "Unknown types";
    char
        hash[MV_BASE58_BUFFER_SIZE(sizeof(global.keys.apdu.hash.final_hash))];

    pair.value = NULL;  // A requirement for ui_strings_push
    switch (pairIndex) {
    case SUMMARY_INDEX_NB_OF_TX: {
        pair.item = "Number of Tx";

        snprintf(num_buffer, sizeof(num_buffer), "%d", op->batch_index);
        ui_strings_push(num_buffer, strlen(num_buffer),
                        (char **)&(pair.value));
    } break;
    case SUMMARY_INDEX_TOTAL_AMOUNT: {
        pair.item = "Total amount";

        mv_mumav_to_string(num_buffer, sizeof(num_buffer), op->total_amount);
        ui_strings_push(num_buffer, strlen(num_buffer),
                        (char **)&(pair.value));

    } break;
    case SUMMARY_INDEX_TOTAL_FEES: {
        pair.item = "Total Fees";

        mv_mumav_to_string(num_buffer, sizeof(num_buffer), op->total_fee);
        ui_strings_push(num_buffer, strlen(num_buffer),
                        (char **)&(pair.value));
    } break;
    case SUMMARY_INDEX_TYPE: {
        get_blindsign_type(type, sizeof(type));
        pair.item = "Type";
        ui_strings_push(type, strlen(type), (char **)&(pair.value));
    }

    break;
    case SUMMARY_INDEX_HASH: {
        if (mv_format_base58(FINAL_HASH, sizeof(FINAL_HASH), hash,
                             sizeof(hash))) {
            MV_FAIL(EXC_UNKNOWN);
        }

        pair.item = "Hash";
        ui_strings_push(hash, strlen(hash), (char **)&(pair.value));
    } break;
    default:
        return NULL;
    }

    MV_POSTAMBLE;
    return &pair;
}

void
continue_blindsign_cb(void)
{
    FUNC_ENTER(("void"));

    ui_strings_init();

    nbgl_operationType_t op = TYPE_TRANSACTION;

    useCaseTagValueList.pairs      = NULL;
    useCaseTagValueList.callback   = getTagValuePair;
    useCaseTagValueList.startIndex = 3;
    useCaseTagValueList.nbPairs    = 2;
    if (global.step == ST_SUMMARY_SIGN) {
        PRINTF("[DEBUG] SUMMARY_SIGN start_index %d\n",
               useCaseTagValueList.startIndex);
        useCaseTagValueList.startIndex = 0;
        useCaseTagValueList.nbPairs    = 5;
    }
    PRINTF("[DEBUG] SIGN Status: %d,  start_index %d Number of pairs:%d ",
           global.step, useCaseTagValueList.startIndex,
           useCaseTagValueList.nbPairs);
    useCaseTagValueList.smallCaseForValue = false;
    useCaseTagValueList.wrapping          = false;
    nbgl_useCaseReviewBlindSigning(op, &useCaseTagValueList, &C_mavryk,
                                   REVIEW("Transaction"), NULL,
                                   SIGN("Transaction"), NULL, reviewChoice);

    FUNC_LEAVE();
}

#endif

static void
handle_data_apdu_blind(void)
{
    MV_PREAMBLE(("void"));

    if (global.keys.apdu.sign.u.clear.received_msg) {
        global.keys.apdu.sign.u.clear.received_msg = false;
    }

    if (!global.keys.apdu.sign.received_last_msg) {
        io_send_sw(SW_OK);
        MV_SUCCEED();
    }

    global.keys.apdu.sign.step = SIGN_ST_WAIT_USER_INPUT;

#ifdef HAVE_BAGL

    char type[OPERATION_TYPE_STR_LENGTH] = "Unknown type";
    get_blindsign_type(type, sizeof(type));
    mv_ui_stream_push_all(MV_UI_STREAM_CB_NOCB, "Sign Hash", type,
                          MV_UI_LAYOUT_BN, MV_UI_ICON_NONE);

    mv_ui_stream();
#elif HAVE_NBGL
    continue_blindsign_cb();
#endif
    MV_POSTAMBLE;
}
#undef FINAL_HASH
