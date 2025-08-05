/* Mavryk Embedded C parser for Ledger - Operation parser

   Copyright 2023 Nomadic Labs <contact@nomadic-labs.com>
   Copyright 2023 Functori <contact@functori.com>
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

#include <string.h>

#include "operation_parser.h"
#include "micheline_parser.h"
#include "num_parser.h"

/* Prototypes */

static mv_parser_result push_frame(mv_parser_state              *state,
                                   mv_operation_parser_step_kind step);
static mv_parser_result pop_frame(mv_parser_state *state);

#ifdef MAVRYK_DEBUG
const char *const mv_operation_parser_step_name[] = {"OPTION",
                                                     "TUPLE",
                                                     "MAGIC",
                                                     "READ_BINARY",
                                                     "BRANCH",
                                                     "BATCH",
                                                     "TAG",
                                                     "SIZE",
                                                     "FIELD",
                                                     "PRINT",
                                                     "PARTIAL_PRINT",
                                                     "READ_NUM",
                                                     "READ_INT32",
                                                     "READ_PK",
                                                     "READ_BYTES",
                                                     "READ_STRING",
                                                     "READ_SMART_ENTRYPOINT",
                                                     "READ_MICHELINE",
                                                     "READ_SORU_MESSAGES",
                                                     "READ_SORU_KIND",
                                                     "READ_BALLOT",
                                                     "READ_PROTOS",
                                                     "READ_PKH_LIST"};

/**
 * @brief Get the string format of an operations step
 *
 */
#define STRING_STEP(step) \
    (const char *)PIC(mv_operation_parser_step_name[step])

#endif

// clang-format off

// Default .skip=false, .complex=false

/**
 * @brief Helper to create an operation field descriptor
 *
 * @required name: name of the field
 * @required kind: kind of the field
 *
 *        By default .skip=false, .complex=false
 */
#define MV_OPERATION_FIELD(name_v, kind_v, ...) \
  {.name=name_v, .kind=kind_v, __VA_ARGS__}

#define MV_OPERATION_LAST_FIELD {0} /// Empty field with an END step

/**
 * @brief Helper to create an operation option field descriptor
 *
 * @required name: name of the option field
 * @required field: field of the option field
 * @required display_none: display if is none
 *
 *        By default .skip=false, .complex=false
 */
#define MV_OPERATION_OPTION_FIELD(name_v, field_v, display_none, ...) \
  {.name=name_v, .kind=MV_OPERATION_FIELD_OPTION,                     \
   .field_option={                                                    \
       .field=&(const mv_operation_field_descriptor)field_v,          \
       display_none                                                   \
   },                                                                 \
   __VA_ARGS__}

/**
 * @brief Helper to create an operation tuple field descriptor
 *
 * @required name: name of the tuple field
 * @required fields: fields of the tuple field
 */
#define MV_OPERATION_TUPLE_FIELD(name_v, ...)           \
  {.name=name_v, .kind=MV_OPERATION_FIELD_TUPLE,        \
   .field_tuple={                                       \
       .fields=(const mv_operation_field_descriptor[]){ \
           __VA_ARGS__,                                 \
           MV_OPERATION_LAST_FIELD                      \
       }                                                \
   }                                                    \
  }

/**
 * @brief Helper to create an operation descriptor
 *
 * @required name: name of the operation
 * @required fields: fields of the operation
 */
#define MV_OPERATION_FIELDS(name, ...) \
  const mv_operation_field_descriptor name[] = { __VA_ARGS__, MV_OPERATION_LAST_FIELD}

MV_OPERATION_FIELDS(proposals_fields,
    MV_OPERATION_FIELD("Source",   MV_OPERATION_FIELD_PKH),
    MV_OPERATION_FIELD("Period",   MV_OPERATION_FIELD_INT32),
    MV_OPERATION_FIELD("Proposal", MV_OPERATION_FIELD_PROTOS)
);

MV_OPERATION_FIELDS(ballot_fields,
    MV_OPERATION_FIELD("Source",   MV_OPERATION_FIELD_PKH),
    MV_OPERATION_FIELD("Period",   MV_OPERATION_FIELD_INT32),
    MV_OPERATION_FIELD("Proposal", MV_OPERATION_FIELD_PROTO),
    MV_OPERATION_FIELD("Ballot",   MV_OPERATION_FIELD_BALLOT)
);

MV_OPERATION_FIELDS(failing_noop_fields,
    MV_OPERATION_FIELD("Message", MV_OPERATION_FIELD_BINARY)
);

/**
 * @brief Set of fields for manager operations
 */
#define MV_OPERATION_MANAGER_OPERATION_FIELDS                                \
    MV_OPERATION_FIELD("Source",        MV_OPERATION_FIELD_SOURCE),          \
    MV_OPERATION_FIELD("Fee",           MV_OPERATION_FIELD_FEE),             \
    MV_OPERATION_FIELD("_Counter",      MV_OPERATION_FIELD_NAT, .skip=true), \
    MV_OPERATION_FIELD("_Gas",          MV_OPERATION_FIELD_NAT, .skip=true), \
    MV_OPERATION_FIELD("Storage limit", MV_OPERATION_FIELD_NAT)

MV_OPERATION_FIELDS(transaction_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_FIELD("Amount",      MV_OPERATION_FIELD_AMOUNT),
    MV_OPERATION_FIELD("Destination", MV_OPERATION_FIELD_DESTINATION),
    MV_OPERATION_OPTION_FIELD("_Parameters",
        MV_OPERATION_TUPLE_FIELD("_Parameters",
            MV_OPERATION_FIELD("Entrypoint", MV_OPERATION_FIELD_SMART_ENTRYPOINT),
            MV_OPERATION_FIELD("Parameter",  MV_OPERATION_FIELD_EXPR, .complex=true)),
        .display_none=false)
);

MV_OPERATION_FIELDS(reveal_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_FIELD("Public key", MV_OPERATION_FIELD_PK)
);

MV_OPERATION_FIELDS(delegation_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_OPTION_FIELD("Delegate",
        MV_OPERATION_FIELD("Delegate", MV_OPERATION_FIELD_PKH),
        .display_none=true)
);

MV_OPERATION_FIELDS(reg_glb_cst_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_FIELD("Value", MV_OPERATION_FIELD_EXPR, .complex=true)
);

MV_OPERATION_FIELDS(set_deposit_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_OPTION_FIELD("Staking limit",
        MV_OPERATION_FIELD("Staking limit", MV_OPERATION_FIELD_AMOUNT),
        .display_none=true)
);

MV_OPERATION_FIELDS(inc_paid_stg_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_FIELD("Amount",      MV_OPERATION_FIELD_INT),
    MV_OPERATION_FIELD("Destination", MV_OPERATION_FIELD_DESTINATION)
);

MV_OPERATION_FIELDS(update_ck_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_FIELD("Public key", MV_OPERATION_FIELD_PK)
);

MV_OPERATION_FIELDS(origination_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_FIELD("Balance", MV_OPERATION_FIELD_AMOUNT),
    MV_OPERATION_OPTION_FIELD("Delegate",
        MV_OPERATION_FIELD("Delegate", MV_OPERATION_FIELD_PKH),
        .display_none=true),
    MV_OPERATION_FIELD("Code",    MV_OPERATION_FIELD_EXPR, .complex=true),
    MV_OPERATION_FIELD("Storage", MV_OPERATION_FIELD_EXPR, .complex=true)
);

MV_OPERATION_FIELDS(transfer_tck_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_FIELD("Contents",    MV_OPERATION_FIELD_EXPR, .complex=true),
    MV_OPERATION_FIELD("Type",        MV_OPERATION_FIELD_EXPR, .complex=true),
    MV_OPERATION_FIELD("Ticketer",    MV_OPERATION_FIELD_DESTINATION),
    MV_OPERATION_FIELD("Amount",      MV_OPERATION_FIELD_NAT),
    MV_OPERATION_FIELD("Destination", MV_OPERATION_FIELD_DESTINATION),
    MV_OPERATION_FIELD("Entrypoint",  MV_OPERATION_FIELD_STRING)
);

MV_OPERATION_FIELDS(soru_add_msg_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_FIELD("Message", MV_OPERATION_FIELD_SORU_MESSAGES)
);

MV_OPERATION_FIELDS(soru_exe_msg_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_FIELD("Rollup",       MV_OPERATION_FIELD_SR),
    MV_OPERATION_FIELD("Commitment",   MV_OPERATION_FIELD_SRC),
    MV_OPERATION_FIELD("Output proof", MV_OPERATION_FIELD_BINARY, .complex=true)
);

MV_OPERATION_FIELDS(soru_origin_fields,
    MV_OPERATION_MANAGER_OPERATION_FIELDS,
    MV_OPERATION_FIELD("Kind",       MV_OPERATION_FIELD_SORU_KIND),
    MV_OPERATION_FIELD("Kernel",     MV_OPERATION_FIELD_BINARY, .complex=true),
    MV_OPERATION_FIELD("Parameters", MV_OPERATION_FIELD_EXPR,   .complex=true),
    MV_OPERATION_OPTION_FIELD("Whitelist",
        MV_OPERATION_FIELD("Whitelist",  MV_OPERATION_FIELD_PKH_LIST),
        .display_none=false)
);

/**
 * @brief Array of all handled operations
 */
const mv_operation_descriptor mv_operation_descriptors[] = {
    {MV_OPERATION_TAG_PROPOSALS,    "Proposals",                  proposals_fields   },
    {MV_OPERATION_TAG_BALLOT,       "Ballot",                     ballot_fields      },
    {MV_OPERATION_TAG_FAILING_NOOP, "Failing noop",               failing_noop_fields},
    {MV_OPERATION_TAG_REVEAL,       "Reveal",                     reveal_fields      },
    {MV_OPERATION_TAG_TRANSACTION,  "Transaction",                transaction_fields },
    {MV_OPERATION_TAG_ORIGINATION,  "Origination",                origination_fields },
    {MV_OPERATION_TAG_DELEGATION,   "Delegation",                 delegation_fields  },
    {MV_OPERATION_TAG_REG_GLB_CST,  "Register global constant",   reg_glb_cst_fields },
    {MV_OPERATION_TAG_SET_DEPOSIT,  "Set deposit limit",          set_deposit_fields },
    {MV_OPERATION_TAG_INC_PAID_STG, "Increase paid storage",      inc_paid_stg_fields},
    {MV_OPERATION_TAG_UPDATE_CK,    "Set consensus key",          update_ck_fields   },
    {MV_OPERATION_TAG_TRANSFER_TCK, "Transfer ticket",            transfer_tck_fields},
    {MV_OPERATION_TAG_SORU_ADD_MSG, "SR: send messages",          soru_add_msg_fields},
    {MV_OPERATION_TAG_SORU_EXE_MSG, "SR: execute outbox message", soru_exe_msg_fields},
    {MV_OPERATION_TAG_SORU_ORIGIN,  "SR: originate",              soru_origin_fields },
    {0,                             NULL,                         0                  }
};
// clang-format on

static const char *expression_name = "Expression";   /// title for micheline
static const char *unset_message   = "Field unset";  /// title for unset field

/**
 * @brief Push a new frame onto the operations parser stack
 *
 * @param state: parser state
 * @param step: step of the new frame
 * @return mv_parser_result: parser result
 */
static mv_parser_result
push_frame(mv_parser_state *state, mv_operation_parser_step_kind step)
{
    mv_operation_state *op = &state->operation;

    if (op->frame >= &op->stack[MV_OPERATION_STACK_DEPTH - 1]) {
        mv_raise(TOO_DEEP);
    }
    op->frame++;
    op->frame->step = step;
    mv_continue;
}

/**
 * @brief Pop the operations parser stack
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
pop_frame(mv_parser_state *state)
{
    mv_operation_state *op = &state->operation;

    if (op->frame == op->stack) {
        op->frame = NULL;
        mv_stop(DONE);
    }
    op->frame--;
    mv_continue;
}

void
mv_operation_parser_set_size(mv_parser_state *state, uint16_t size)
{
    state->operation.stack[0].stop = size;
}

void
mv_operation_parser_init(mv_parser_state *state, uint16_t size,
                         bool skip_magic)
{
    mv_operation_state *op = &state->operation;

    mv_parser_init(state);
    state->operation.seen_reveal = 0;
    memset(&state->operation.source, 0, 22);
    memset(&state->operation.destination, 0, 22);
    op->batch_index = 0;
#ifdef HAVE_SWAP
    op->last_tag  = MV_OPERATION_TAG_END;
    op->nb_reveal = 0;
#endif  // HAVE_SWAP
    op->total_fee     = 0;
    op->total_amount  = 0;
    op->frame         = op->stack;
    op->stack[0].stop = size;
    if (!skip_magic) {
        op->stack[0].step = MV_OPERATION_STEP_MAGIC;
    } else {
        STRLCPY(state->field_info.field_name, "Branch");
        op->stack[0].step = MV_OPERATION_STEP_BRANCH;
        push_frame(state, MV_OPERATION_STEP_READ_BYTES);  // ignore result,
                                                          // assume success
        op->frame->step_read_bytes.kind = MV_OPERATION_FIELD_BH;
        op->frame->step_read_bytes.skip = true;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 32;
    }
}

/*
 * We use a macro for CAPTURE rather than defining a ptr like:
 *
 *     uint8_t *capture = state->buffers.capture
 *
 * because sizeof(*capture) == 1, whereas sizeof(CAPTURE) is
 * the size of the buffer.  This allows us to more idiomatically
 * check the size of buffers.
 */
#define CAPTURE (state->buffers.capture)

/**
 * @brief Ask to print what has been captured
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_print_string(mv_parser_state *state)
{
    mv_operation_state *op = &state->operation;

    if (op->frame->step_read_string.skip) {
        mv_must(pop_frame(state));
        mv_continue;
    }
    op->frame->step           = MV_OPERATION_STEP_PRINT;
    op->frame->step_print.str = (char *)CAPTURE;
    mv_continue;
}

/**
 * @brief Helper to assert the current step
 */
#define ASSERT_STEP(state, expected_step)                                    \
    do {                                                                     \
        mv_operation_parser_step_kind step = (state)->operation.frame->step; \
        if (step != MV_OPERATION_STEP_##expected_step) {                     \
            PRINTF("[DEBUG] expected step %s but got step %s)\n",            \
                   STRING_STEP(MV_OPERATION_STEP_##expected_step),           \
                   STRING_STEP(step));                                       \
            mv_raise(INVALID_STATE);                                         \
        }                                                                    \
    } while (0)

/**
 * @brief Try to read an optionnal field
 *
 *        If the field is present, ask to read it.
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_option(mv_parser_state *state)
{
    ASSERT_STEP(state, OPTION);
    mv_operation_state *op = &state->operation;
    uint8_t             present;
    mv_must(mv_parser_read(state, &present));
    if (!present) {
        if (op->frame->step_option.display_none) {
            if (op->frame->step_option.field->skip) {
                mv_raise(INVALID_STATE);
            }
            op->frame->step           = MV_OPERATION_STEP_PRINT;
            op->frame->step_print.str = (char *)unset_message;
        } else {
            mv_must(pop_frame(state));
        }
    } else {
        op->frame->step             = MV_OPERATION_STEP_FIELD;
        op->frame->step_field.field = op->frame->step_option.field;
    }
    mv_continue;
}

/**
 * @brief Ask to read remaining fields of a tuple field
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_tuple(mv_parser_state *state)
{
    ASSERT_STEP(state, TUPLE);
    mv_operation_state                  *op    = &state->operation;
    mv_parser_regs                      *regs  = &state->regs;
    const mv_operation_field_descriptor *field = PIC(
        &op->frame->step_tuple.fields[op->frame->step_tuple.field_index]);

    // Remaining content from previous section - display this first.
    if (regs->oofs > 0) {
        mv_stop(IM_FULL);
    }

    if (field->kind == MV_OPERATION_FIELD_END) {
        // is_field_complex is reset after reaching the last field
        state->field_info.is_field_complex = false;
        mv_must(pop_frame(state));
    } else {
        op->frame->step_tuple.field_index++;
        mv_must(push_frame(state, MV_OPERATION_STEP_FIELD));
        op->frame->step_field.field = field;
    }
    mv_continue;
}

/* Update the state in order to read an operation or a micheline expression
 * based on a magic byte */
/**
 * @brief Read a magic byte and plan nexts steps
 *
 *        The magic byte identifies if the data to read is a micheline
 *        expression or a batch of operations
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_magic(mv_parser_state *state)
{
    ASSERT_STEP(state, MAGIC);
    mv_operation_state *op = &state->operation;
    uint8_t             b;
    mv_must(mv_parser_read(state, &b));
    switch (b) {
    case 3:  // manager/anonymous operation
        STRLCPY(state->field_info.field_name, "Branch");
        op->stack[0].step = MV_OPERATION_STEP_BRANCH;
        push_frame(state,
                   MV_OPERATION_STEP_READ_BYTES);  // ignore result,
                                                   //  assume success
        op->frame->step_read_bytes.kind = MV_OPERATION_FIELD_BH;
        op->frame->step_read_bytes.skip = true;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 32;
        break;
    case 5:  // micheline expression
        op->frame->step = MV_OPERATION_STEP_READ_MICHELINE;
        op->frame->step_read_micheline.inited = 0;
        op->frame->step_read_micheline.skip   = false;
        op->frame->step_read_micheline.name   = (char *)PIC(expression_name);
        op->frame->stop                       = 0;
        break;
    default:
        mv_raise(INVALID_TAG);
    }
    mv_continue;
}

/**
 * @brief Read a 4-bytes size
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_size(mv_parser_state *state)
{
    ASSERT_STEP(state, SIZE);
    mv_operation_state *op = &state->operation;
    uint8_t             b;
    mv_must(mv_parser_read(state, &b));
    if (op->frame->step_size.size > 255) {
        mv_raise(TOO_LARGE);  // enforce 16-bit restriction
    }
    op->frame->step_size.size = (op->frame->step_size.size << 8) | b;
    op->frame->step_size.size_len--;
    if (op->frame->step_size.size_len == 0) {
        op->frame[-1].stop = state->ofs + op->frame->step_size.size;
        mv_must(pop_frame(state));
    }
    mv_continue;
}

/**
 * @brief Find the operation associated to the operation tag and ask
 *        to read its fields
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_tag(mv_parser_state *state)
{
    ASSERT_STEP(state, TAG);
    mv_operation_state            *op = &state->operation;
    const mv_operation_descriptor *d;
    uint8_t                        t;
    mv_must(mv_parser_read(state, &t));
#ifdef HAVE_SWAP
    op->last_tag = t;
    if (t == MV_OPERATION_TAG_REVEAL) {
        op->nb_reveal++;
    }
#endif  // HAVE_SWAP
    for (d = mv_operation_descriptors; d->tag != MV_OPERATION_TAG_END; d++) {
        if (d->tag == t) {
            op->frame->step                   = MV_OPERATION_STEP_TUPLE;
            op->frame->step_tuple.fields      = d->fields;
            op->frame->step_tuple.field_index = 0;
            mv_must(push_frame(state, MV_OPERATION_STEP_PRINT));
            snprintf(state->field_info.field_name, 30, "Operation (%d)",
                     op->batch_index);
            op->frame->step_print.str = d->name;
            mv_continue;
        }
    }
    mv_raise(INVALID_TAG);
}

/**
 * @brief Read a micheline expression
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_micheline(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_MICHELINE);
    mv_operation_state *op   = &state->operation;
    mv_parser_regs     *regs = &state->regs;
    if (!op->frame->step_read_micheline.inited) {
        op->frame->step_read_micheline.inited = 1;
        STRLCPY(state->field_info.field_name,
                op->frame->step_read_micheline.name);
        mv_micheline_parser_init(state);
    }
    mv_micheline_parser_step(state);
    if (state->errno == MV_BLO_DONE) {
        if (state->micheline.is_unit) {
            state->field_info.is_field_complex = false;
        }
        if ((op->frame->stop != 0) && (state->ofs != op->frame->stop)) {
            mv_raise(TOO_LARGE);
        }
        mv_must(pop_frame(state));
        if (regs->oofs > 0) {
            mv_stop(IM_FULL);
        } else {
            mv_continue;
        }
    }
    mv_reraise;
}

/**
 * @brief Format a string as an amount
 *
 * @param str: string to format
 */
static void
mv_format_amount(char *str)
{
    int len = 0;
    while (str[len]) {
        len++;
    }
    if ((len == 1) && (str[0] == 0)) {
        // just 0
        goto add_currency;
    }
    if (len < 7) {
        // less than one tez, pad left up to the '0.'
        int j;
        int pad = 7 - len;
        for (j = len; j >= 0; j--) {
            str[j + pad] = str[j];
        }
        for (j = 0; j < pad; j++) {
            str[j] = '0';
        }
        len = 7;
    }
    int no_decimals = 1;
    for (int i = 0; i < 6; i++) {
        no_decimals &= (str[len - 1 - i] == '0');
    }
    if (no_decimals) {
        // integral value, don't include the decimal part (no '.'_
        str[len - 6] = 0;
        len -= 6;
    } else {
        // more than one tez, add the '.'
        for (int i = 0; i < 6; i++) {
            str[len - i] = str[len - i - 1];
        }
        str[len - 6] = '.';
        len++;
        str[len] = 0;
        // drop trailing non significant zeroes
        while (str[len - 1] == '0') {
            len--;
            str[len] = 0;
        }
    }
add_currency:
    str[len]     = ' ';
    str[len + 1] = 'M';
    str[len + 2] = 'V';
    str[len + 3] = 'R';
    str[len + 4] = 'K';
    len += 5;
    str[len] = 0;
}

/**
 * @brief Read a number
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_num(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_NUM);
    mv_operation_state *op = &state->operation;
    uint8_t             b;
    mv_must(mv_parser_read(state, &b));
    mv_must(mv_parse_num_step(&state->buffers.num,
                              &op->frame->step_read_num.state, b,
                              op->frame->step_read_num.natural));
    if (op->frame->step_read_num.state.stop) {
        uint64_t value;
        if (!mv_string_to_mumav(state->buffers.num.decimal, &value)) {
            mv_raise(INVALID_DATA);
        }
        switch (op->frame->step_read_num.kind) {
        case MV_OPERATION_FIELD_AMOUNT:
            op->total_amount += value;
            break;
        case MV_OPERATION_FIELD_FEE:
            op->total_fee += value;
            break;
        default:
            break;
        }
        if (op->frame->step_read_num.skip) {
            mv_must(pop_frame(state));
            mv_continue;
        }
        char *str       = state->buffers.num.decimal;
        op->frame->step = MV_OPERATION_STEP_PRINT;
        switch (op->frame->step_read_num.kind) {
        case MV_OPERATION_FIELD_INT:
        case MV_OPERATION_FIELD_NAT:
            break;
        case MV_OPERATION_FIELD_FEE:
        case MV_OPERATION_FIELD_AMOUNT: {
            mv_format_amount(str);
            break;
        }
        default:
            mv_raise(INVALID_STATE);
        }
        op->frame->step_print.str = str;
    }
    mv_continue;
}

/**
 * @brief Read an int32
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_int32(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_INT32);
    mv_operation_state *op = &state->operation;
    uint8_t             b;
    int32_t            *value = &op->frame->step_read_int32.value;
    if (op->frame->step_read_int32.ofs < 4) {
        mv_must(mv_parser_read(state, &b));
        *value = (*value << 8) | b;
        op->frame->step_read_int32.ofs++;
    } else {
        snprintf((char *)CAPTURE, sizeof(CAPTURE), "%d", *value);
        op->frame->step_read_string.skip = op->frame->step_read_int32.skip;
        mv_must(mv_print_string(state));
    }
    mv_continue;
}

/**
 * @brief Read bytes
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_bytes(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_BYTES);
    mv_operation_state *op = &state->operation;
    if (op->frame->step_read_bytes.ofs < op->frame->step_read_bytes.len) {
        uint8_t *c;
        c = &CAPTURE[op->frame->step_read_bytes.ofs];
        mv_must(mv_parser_read(state, c));
        op->frame->step_read_bytes.ofs++;
    } else {
        if (op->frame->step_read_num.skip) {
            mv_must(pop_frame(state));
            mv_continue;
        }
        switch (op->frame->step_read_bytes.kind) {
        case MV_OPERATION_FIELD_SOURCE:
            memcpy(op->source, CAPTURE, 22);
            __attribute__((fallthrough));
        case MV_OPERATION_FIELD_PKH:
            if (mv_format_pkh(CAPTURE, 21, (char *)CAPTURE,
                              sizeof(CAPTURE))) {
                mv_raise(INVALID_TAG);
            }
            break;
        case MV_OPERATION_FIELD_PK:
            if (mv_format_pk(CAPTURE, op->frame->step_read_bytes.len,
                             (char *)CAPTURE, sizeof(CAPTURE))) {
                mv_raise(INVALID_TAG);
            }
            break;
        case MV_OPERATION_FIELD_SR:
            if (mv_format_base58check("sr1", CAPTURE, 20, (char *)CAPTURE,
                                      sizeof(CAPTURE))) {
                mv_raise(INVALID_TAG);
            }
            break;
        case MV_OPERATION_FIELD_SRC:
            if (mv_format_base58check("src1", CAPTURE, 32, (char *)CAPTURE,
                                      sizeof(CAPTURE))) {
                mv_raise(INVALID_TAG);
            }
            break;
        case MV_OPERATION_FIELD_PROTO:
            if (mv_format_base58check("proto", CAPTURE, 32, (char *)CAPTURE,
                                      sizeof(CAPTURE))) {
                mv_raise(INVALID_TAG);
            }
            break;
        case MV_OPERATION_FIELD_DESTINATION:
            memcpy(op->destination, CAPTURE, 22);
            if (mv_format_address(CAPTURE, 22, (char *)CAPTURE,
                                  sizeof(CAPTURE))) {
                mv_raise(INVALID_TAG);
            }
            break;
        case MV_OPERATION_FIELD_OPH:
            if (mv_format_oph(CAPTURE, 32, (char *)CAPTURE,
                              sizeof(CAPTURE))) {
                mv_raise(INVALID_TAG);
            }
            break;
        case MV_OPERATION_FIELD_BH:
            if (mv_format_bh(CAPTURE, 32, (char *)CAPTURE, sizeof(CAPTURE))) {
                mv_raise(INVALID_TAG);
            }
            break;
        default:
            mv_raise(INVALID_STATE);
        }
        op->frame->step           = MV_OPERATION_STEP_PRINT;
        op->frame->step_print.str = (char *)CAPTURE;
    }
    mv_continue;
}

/**
 * @brief Plan the steps to read a batch of operations
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_branch(mv_parser_state *state)
{
    ASSERT_STEP(state, BRANCH);
    mv_operation_state *op = &state->operation;
    op->frame->step        = MV_OPERATION_STEP_BATCH;
    mv_must(push_frame(state, MV_OPERATION_STEP_TAG));
    mv_continue;
}

/**
 * @brief Ask to read remaining operations of a batch of operations
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_batch(mv_parser_state *state)
{
    ASSERT_STEP(state, BATCH);
    mv_operation_state *op = &state->operation;
    op->batch_index++;
    if (state->ofs == op->frame->stop) {
        mv_must(pop_frame(state));
    } else if (state->ofs > op->frame->stop) {
        mv_raise(TOO_LARGE);
    } else {
        mv_must(push_frame(state, MV_OPERATION_STEP_TAG));
    }
    mv_continue;
}

/**
 * @brief Read a string
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_string(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_STRING);
    mv_operation_state *op = &state->operation;
    if (state->ofs == op->frame->stop) {
        CAPTURE[op->frame->step_read_string.ofs] = 0;
        mv_must(mv_print_string(state));
    } else {
        uint8_t b;
        mv_must(mv_parser_read(state, &b));
        CAPTURE[op->frame->step_read_string.ofs] = b;
        op->frame->step_read_string.ofs++;
    }
    mv_continue;
}

/**
 * @brief Read a binary
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_binary(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_BINARY);
    mv_operation_state *op = &state->operation;
    if (state->ofs == op->frame->stop) {
        CAPTURE[op->frame->step_read_string.ofs] = 0;
        mv_must(mv_print_string(state));
    } else if ((op->frame->step_read_string.ofs + 2)
               >= MV_CAPTURE_BUFFER_SIZE) {
        CAPTURE[op->frame->step_read_string.ofs] = 0;
        op->frame->step_read_string.ofs          = 0;
        if (!op->frame->step_read_string.skip) {
            mv_must(push_frame(state, MV_OPERATION_STEP_PARTIAL_PRINT));
            op->frame->step_print.str = (char *)CAPTURE;
        }
    } else {
        uint8_t b;
        mv_must(mv_parser_read(state, &b));
        char *buf = (char *)CAPTURE + op->frame->step_read_string.ofs;
        snprintf(buf, 4, "%02x", b);
        op->frame->step_read_string.ofs += 2;
    }
    mv_continue;
}

/**
 * @brief Read a smart entrypoint
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_smart_entrypoint(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_SMART_ENTRYPOINT);
    mv_operation_state *op = &state->operation;
    uint8_t             b;
    mv_must(mv_parser_read(state, &b));
    switch (b) {
    case 0:
        strlcpy((char *)CAPTURE, "default", sizeof(CAPTURE));
        mv_must(mv_print_string(state));
        break;
    case 1:
        strlcpy((char *)CAPTURE, "root", sizeof(CAPTURE));
        mv_must(mv_print_string(state));
        break;
    case 2:
        strlcpy((char *)CAPTURE, "do", sizeof(CAPTURE));
        mv_must(mv_print_string(state));
        break;
    case 3:
        strlcpy((char *)CAPTURE, "set_delegate", sizeof(CAPTURE));
        mv_must(mv_print_string(state));
        break;
    case 4:
        strlcpy((char *)CAPTURE, "remove_delegate", sizeof(CAPTURE));
        mv_must(mv_print_string(state));
        break;
    case 5:
        strlcpy((char *)CAPTURE, "deposit", sizeof(CAPTURE));
        mv_must(mv_print_string(state));
        break;
    case 6:
        strlcpy((char *)CAPTURE, "stake", sizeof(CAPTURE));
        mv_must(mv_print_string(state));
        break;
    case 7:
        strlcpy((char *)CAPTURE, "unstake", sizeof(CAPTURE));
        mv_must(mv_print_string(state));
        break;
    case 8:
        strlcpy((char *)CAPTURE, "finalize_unstake", sizeof(CAPTURE));
        mv_must(mv_print_string(state));
        break;
    case 9:
        strlcpy((char *)CAPTURE, "set_delegate_parameters", sizeof(CAPTURE));
        mv_must(mv_print_string(state));
        break;
    case 0xFF:
        op->frame->step                 = MV_OPERATION_STEP_READ_STRING;
        op->frame->step_read_string.ofs = 0;
        mv_must(push_frame(state, MV_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 1;
        break;
    default:
        mv_raise(INVALID_TAG);
    }
    mv_continue;
}

/**
 * @brief Plan the steps required to read the current operation field
 *
 *        Update the current field info only if the field is not
 *        ignored
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_field(mv_parser_state *state)
{
    ASSERT_STEP(state, FIELD);
    mv_operation_state                  *op    = &state->operation;
    const mv_operation_field_descriptor *field = op->frame->step_field.field;
    const char                          *name  = PIC(field->name);

    // is_field_complex is reset after reaching MV_OPERATION_FIELD_END
    if (!field->skip) {
        STRLCPY(state->field_info.field_name, name);
        state->field_info.is_field_complex = field->complex;
        state->field_info.field_index++;
    }

    switch (field->kind) {
    case MV_OPERATION_FIELD_OPTION: {
        op->frame->step              = MV_OPERATION_STEP_OPTION;
        op->frame->step_option.field = PIC(field->field_option.field);
        op->frame->step_option.display_none
            = field->field_option.display_none;
        break;
    }
    case MV_OPERATION_FIELD_TUPLE: {
        op->frame->step                   = MV_OPERATION_STEP_TUPLE;
        op->frame->step_tuple.fields      = field->field_tuple.fields;
        op->frame->step_tuple.field_index = 0;
        break;
    }
    case MV_OPERATION_FIELD_BINARY: {
        op->frame->step                  = MV_OPERATION_STEP_READ_BINARY;
        op->frame->step_read_string.ofs  = 0;
        op->frame->step_read_string.skip = field->skip;
        mv_must(push_frame(state, MV_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case MV_OPERATION_FIELD_SOURCE:
    case MV_OPERATION_FIELD_PKH: {
        op->frame->step                 = MV_OPERATION_STEP_READ_BYTES;
        op->frame->step_read_bytes.kind = field->kind;
        op->frame->step_read_bytes.skip = field->skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 21;
        break;
    }
    case MV_OPERATION_FIELD_PK: {
        op->frame->step                 = MV_OPERATION_STEP_READ_PK;
        op->frame->step_read_bytes.skip = field->skip;
        break;
    }
    case MV_OPERATION_FIELD_SR: {
        op->frame->step                 = MV_OPERATION_STEP_READ_BYTES;
        op->frame->step_read_bytes.kind = field->kind;
        op->frame->step_read_bytes.skip = field->skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 20;
        break;
    }
    case MV_OPERATION_FIELD_SRC: {
        op->frame->step                 = MV_OPERATION_STEP_READ_BYTES;
        op->frame->step_read_bytes.kind = field->kind;
        op->frame->step_read_bytes.skip = field->skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 32;
        break;
    }
    case MV_OPERATION_FIELD_PROTO: {
        op->frame->step                 = MV_OPERATION_STEP_READ_BYTES;
        op->frame->step_read_bytes.kind = field->kind;
        op->frame->step_read_bytes.skip = field->skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 32;
        break;
    }
    case MV_OPERATION_FIELD_PROTOS: {
        op->frame->step                 = MV_OPERATION_STEP_READ_PROTOS;
        op->frame->step_read_list.name  = name;
        op->frame->step_read_list.index = 0;
        op->frame->step_read_list.skip  = field->skip;
        mv_must(push_frame(state, MV_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case MV_OPERATION_FIELD_DESTINATION: {
        op->frame->step                 = MV_OPERATION_STEP_READ_BYTES;
        op->frame->step_read_bytes.kind = field->kind;
        op->frame->step_read_bytes.skip = field->skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 22;
        break;
    }
    case MV_OPERATION_FIELD_NAT:
    case MV_OPERATION_FIELD_FEE:
    case MV_OPERATION_FIELD_AMOUNT: {
        op->frame->step = MV_OPERATION_STEP_READ_NUM;
        mv_parse_num_state_init(&state->buffers.num,
                                &op->frame->step_read_num.state);
        op->frame->step_read_num.kind    = field->kind;
        op->frame->step_read_num.skip    = field->skip;
        op->frame->step_read_num.natural = 1;
        break;
    }
    case MV_OPERATION_FIELD_INT: {
        op->frame->step = MV_OPERATION_STEP_READ_NUM;
        mv_parse_num_state_init(&state->buffers.num,
                                &op->frame->step_read_num.state);
        op->frame->step_read_num.kind    = field->kind;
        op->frame->step_read_num.skip    = field->skip;
        op->frame->step_read_num.natural = 0;
        break;
    }
    case MV_OPERATION_FIELD_INT32: {
        op->frame->step                  = MV_OPERATION_STEP_READ_INT32;
        op->frame->step_read_int32.value = 0;
        op->frame->step_read_int32.ofs   = 0;
        op->frame->step_read_int32.skip  = field->skip;
        break;
    }
    case MV_OPERATION_FIELD_SMART_ENTRYPOINT: {
        op->frame->step = MV_OPERATION_STEP_READ_SMART_ENTRYPOINT;
        op->frame->step_read_string.ofs  = 0;
        op->frame->step_read_string.skip = field->skip;
        break;
    }
    case MV_OPERATION_FIELD_EXPR: {
        op->frame->step = MV_OPERATION_STEP_READ_MICHELINE;
        op->frame->step_read_micheline.inited = 0;
        op->frame->step_read_micheline.skip   = field->skip;
        op->frame->step_read_micheline.name   = name;
        mv_must(push_frame(state, MV_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case MV_OPERATION_FIELD_STRING: {
        op->frame->step                  = MV_OPERATION_STEP_READ_STRING;
        op->frame->step_read_string.ofs  = 0;
        op->frame->step_read_string.skip = field->skip;
        mv_must(push_frame(state, MV_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case MV_OPERATION_FIELD_SORU_MESSAGES: {
        op->frame->step                = MV_OPERATION_STEP_READ_SORU_MESSAGES;
        op->frame->step_read_list.name = name;
        op->frame->step_read_list.index = 0;
        op->frame->step_read_list.skip  = field->skip;
        mv_must(push_frame(state, MV_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case MV_OPERATION_FIELD_SORU_KIND: {
        op->frame->step                  = MV_OPERATION_STEP_READ_SORU_KIND;
        op->frame->step_read_string.skip = field->skip;
        break;
    }
    case MV_OPERATION_FIELD_PKH_LIST: {
        op->frame->step                 = MV_OPERATION_STEP_READ_PKH_LIST;
        op->frame->step_read_list.name  = name;
        op->frame->step_read_list.index = 0;
        op->frame->step_read_list.skip  = field->skip;
        mv_must(push_frame(state, MV_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
        break;
    }
    case MV_OPERATION_FIELD_BALLOT: {
        op->frame->step                  = MV_OPERATION_STEP_READ_BALLOT;
        op->frame->step_read_string.skip = field->skip;
        break;
    }
    default:
        mv_raise(INVALID_STATE);
    }
    mv_continue;
}

/**
 * @brief Read a public key
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_pk(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_PK);
    mv_operation_state *op = &state->operation;
    uint8_t             b;
    mv_must(mv_parser_peek(state, &b));
    op->frame->step_read_bytes.kind = MV_OPERATION_FIELD_PK;
    op->frame->step_read_bytes.ofs  = 0;
    switch (b) {
    case 0:  // edpk
        op->frame->step_read_bytes.len = 33;
        break;
    case 1:  // sppk
        op->frame->step_read_bytes.len = 34;
        break;
    case 2:  // p2pk
        op->frame->step_read_bytes.len = 34;
        break;
    case 3:  // BLpk
        op->frame->step_read_bytes.len = 49;
        break;
    default:
        mv_raise(INVALID_TAG);
    }
    op->frame->step = MV_OPERATION_STEP_READ_BYTES;
    mv_continue;
}

/**
 * @brief Read a list of public key hash
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_pkh_list(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_PKH_LIST);
    mv_operation_state *op    = &state->operation;
    mv_parser_regs     *regs  = &state->regs;
    uint8_t             skip  = op->frame->step_read_list.skip;
    const char         *name  = op->frame->step_read_list.name;
    uint16_t            index = op->frame->step_read_list.index;

    // Remaining content from previous public key hash - display this first.
    if (regs->oofs > 0) {
        mv_stop(IM_FULL);
    }

    if (op->frame->stop == state->ofs) {
        mv_must(pop_frame(state));
    } else {
        op->frame->step_read_list.index++;
        mv_must(push_frame(state, MV_OPERATION_STEP_READ_BYTES));
        snprintf(state->field_info.field_name, MV_FIELD_NAME_SIZE, "%s (%d)",
                 name, index);
        op->frame->step_read_bytes.kind = MV_OPERATION_FIELD_PKH;
        op->frame->step_read_bytes.skip = skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 21;
    }
    mv_continue;
}

/**
 * @brief Read soru messages
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_soru_messages(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_SORU_MESSAGES);
    mv_operation_state *op    = &state->operation;
    mv_parser_regs     *regs  = &state->regs;
    uint8_t             skip  = op->frame->step_read_list.skip;
    const char         *name  = op->frame->step_read_list.name;
    uint16_t            index = op->frame->step_read_list.index;

    // Remaining content from previous message - display this first.
    if (regs->oofs > 0) {
        mv_stop(IM_FULL);
    }

    if (op->frame->stop == state->ofs) {
        mv_must(pop_frame(state));
    } else {
        op->frame->step_read_list.index++;
        mv_must(push_frame(state, MV_OPERATION_STEP_READ_BINARY));
        snprintf(state->field_info.field_name, MV_FIELD_NAME_SIZE, "%s (%d)",
                 name, index);
        op->frame->step_read_string.ofs  = 0;
        op->frame->step_read_string.skip = skip;
        mv_must(push_frame(state, MV_OPERATION_STEP_SIZE));
        op->frame->step_size.size     = 0;
        op->frame->step_size.size_len = 4;
    }
    mv_continue;
}

/**
 * @brief Read a soru kind
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_soru_kind(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_SORU_KIND);
    uint8_t b;
    mv_must(mv_parser_read(state, &b));
    switch (b) {
    case 0:
        strlcpy((char *)CAPTURE, "arith", sizeof(CAPTURE));
        break;
    case 1:
        strlcpy((char *)CAPTURE, "wasm_2_0_0", sizeof(CAPTURE));
        break;
    case 2:  /// Present in encoding, not activated in Oxford
        strlcpy((char *)CAPTURE, "riscv", sizeof(CAPTURE));
        break;
    default:
        mv_raise(INVALID_TAG);
    }
    mv_must(mv_print_string(state));
    mv_continue;
}

/**
 * @brief Read a ballot
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_ballot(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_BALLOT);
    uint8_t b;
    mv_must(mv_parser_read(state, &b));
    switch (b) {
    case 0:
        strlcpy((char *)CAPTURE, "yay", sizeof(CAPTURE));
        break;
    case 1:
        strlcpy((char *)CAPTURE, "nay", sizeof(CAPTURE));
        break;
    case 2:
        strlcpy((char *)CAPTURE, "pass", sizeof(CAPTURE));
        break;
    default:
        mv_raise(INVALID_TAG);
    }
    mv_must(mv_print_string(state));
    mv_continue;
}

/**
 * @brief Read a protocol list
 *
 * @param state: parser state
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_read_protos(mv_parser_state *state)
{
    ASSERT_STEP(state, READ_PROTOS);
    mv_operation_state *op    = &state->operation;
    mv_parser_regs     *regs  = &state->regs;
    uint8_t             skip  = op->frame->step_read_list.skip;
    const char         *name  = op->frame->step_read_list.name;
    uint16_t            index = op->frame->step_read_list.index;

    // Remaining content from previous proto - display this first.
    if (regs->oofs > 0) {
        mv_stop(IM_FULL);
    }

    if (op->frame->stop == state->ofs) {
        mv_must(pop_frame(state));
    } else {
        op->frame->step_read_list.index++;
        mv_must(push_frame(state, MV_OPERATION_STEP_READ_BYTES));
        snprintf(state->field_info.field_name, 30, "%s (%d)", name, index);
        op->frame->step_read_bytes.kind = MV_OPERATION_FIELD_PROTO;
        op->frame->step_read_bytes.skip = skip;
        op->frame->step_read_bytes.ofs  = 0;
        op->frame->step_read_bytes.len  = 32;
    }
    mv_continue;
}

/**
 * @brief Print a string
 *
 * @param state: parser state
 * @param partial: If partial true, then the string is not yet
 *                 complete
 * @return mv_parser_result: parser result
 */
static mv_parser_result
mv_step_print(mv_parser_state *state, bool partial)
{
    if ((state->operation.frame->step != MV_OPERATION_STEP_PRINT)
        && (state->operation.frame->step
            != MV_OPERATION_STEP_PARTIAL_PRINT)) {
        PRINTF("[DEBUG] expected step %s or step %s but got step %s)\n",
               STRING_STEP(MV_OPERATION_STEP_PRINT),
               STRING_STEP(MV_OPERATION_STEP_PARTIAL_PRINT),
               STRING_STEP(state->operation.frame->step));
        mv_raise(INVALID_STATE);
    }
    mv_operation_state *op  = &state->operation;
    const char         *str = PIC(op->frame->step_print.str);
    if (*str) {
        mv_must(mv_parser_put(state, *str));
        op->frame->step_print.str++;
    } else {
        mv_must(pop_frame(state));
        if (!partial) {
            mv_stop(IM_FULL);
        }
    }
    mv_continue;
}

mv_parser_result
mv_operation_parser_step(mv_parser_state *state)
{
    mv_operation_state *op = &state->operation;

    // cannot restart after error
    if (MV_IS_ERR(state->errno)) {
        mv_reraise;
    }

    // nothing else to do
    if (op->frame == NULL) {
        mv_stop(DONE);
    }

    PRINTF(
        "[DEBUG] operation(frame: %d, offset:%d/%d, ilen: %d, olen: %d, "
        "step: %s, errno: %s)\n",
        (int)(op->frame - op->stack), (int)state->ofs, (int)op->stack[0].stop,
        (int)state->regs.ilen, (int)state->regs.oofs,
        STRING_STEP(op->frame->step), mv_parser_result_name(state->errno));

    switch (op->frame->step) {
    case MV_OPERATION_STEP_OPTION:
        mv_must(mv_step_option(state));
        break;
    case MV_OPERATION_STEP_TUPLE:
        mv_must(mv_step_tuple(state));
        break;
    case MV_OPERATION_STEP_MAGIC:
        mv_must(mv_step_magic(state));
        break;
    case MV_OPERATION_STEP_SIZE:
        mv_must(mv_step_size(state));
        break;
    case MV_OPERATION_STEP_TAG:
        mv_must(mv_step_tag(state));
        break;
    case MV_OPERATION_STEP_READ_MICHELINE:
        mv_must(mv_step_read_micheline(state));
        break;
    case MV_OPERATION_STEP_READ_NUM:
        mv_must(mv_step_read_num(state));
        break;
    case MV_OPERATION_STEP_READ_INT32:
        mv_must(mv_step_read_int32(state));
        break;
    case MV_OPERATION_STEP_READ_BYTES:
        mv_must(mv_step_read_bytes(state));
        break;
    case MV_OPERATION_STEP_BRANCH:
        mv_must(mv_step_branch(state));
        break;
    case MV_OPERATION_STEP_BATCH:
        mv_must(mv_step_batch(state));
        break;
    case MV_OPERATION_STEP_READ_STRING:
        mv_must(mv_step_read_string(state));
        break;
    case MV_OPERATION_STEP_READ_BINARY:
        mv_must(mv_step_read_binary(state));
        break;
    case MV_OPERATION_STEP_READ_SMART_ENTRYPOINT:
        mv_must(mv_step_read_smart_entrypoint(state));
        break;
    case MV_OPERATION_STEP_FIELD:
        mv_must(mv_step_field(state));
        break;
    case MV_OPERATION_STEP_READ_PK:
        mv_must(mv_step_read_pk(state));
        break;
    case MV_OPERATION_STEP_READ_SORU_MESSAGES:
        mv_must(mv_step_read_soru_messages(state));
        break;
    case MV_OPERATION_STEP_READ_SORU_KIND:
        mv_must(mv_step_read_soru_kind(state));
        break;
    case MV_OPERATION_STEP_READ_BALLOT:
        mv_must(mv_step_read_ballot(state));
        break;
    case MV_OPERATION_STEP_READ_PROTOS:
        mv_must(mv_step_read_protos(state));
        break;
    case MV_OPERATION_STEP_READ_PKH_LIST:
        mv_must(mv_step_read_pkh_list(state));
        break;
    case MV_OPERATION_STEP_PRINT:
    case MV_OPERATION_STEP_PARTIAL_PRINT:
        mv_must(mv_step_print(
            state, op->frame->step == MV_OPERATION_STEP_PARTIAL_PRINT));
        break;
    default:
        mv_raise(INVALID_STATE);
    }
    mv_continue;
}
