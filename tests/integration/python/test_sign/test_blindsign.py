#!/usr/bin/env python3
# Copyright 2024 Functori <contact@functori.com>
# Copyright 2024 Trilitech <contact@trili.tech>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Gathering of tests related to Blindsign."""

from pathlib import Path
from typing import List, Union

import pytest

from ragger.firmware import Firmware
from ragger.navigator import NavIns, NavInsID

from utils.account import Account
from utils.backend import MavrykBackend, StatusCode
from utils.message import (
    MichelineExpr,
    Proposals,
    OperationGroup,
    Reveal,
    Transaction,
    Delegation,
    RegisterGlobalConstant,
    SetDepositLimit,
    ScRollupAddMessage
)
from utils.navigator import MavrykNavigator


### Too long operation ###

BASIC_OPERATION = OperationGroup([
    Reveal(
        source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
        fee = 1000000,
        counter = 11,
        gas_limit = 1,
        storage_limit = 4,
        public_key = 'p2pk65YHEfEbWo7iMrz7JNjBvaYZNFBHU8vzCQEhw8rmbvAKuiGGiXS'
    ),
    Transaction(
        source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
        fee = 2000000,
        counter = 12,
        gas_limit = 1,
        storage_limit = 7,
        destination = 'mv3Thf4FrSnZvQxuep5zM5BcKDZJy6zKZ5VW',
        amount = 3000000,
        entrypoint = 'update_config',
        parameter = {'prim': 'Pair', 'args': [ {'int': 5}, {'prim': 'True'} ]}
    ),
    Delegation(
        source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
        fee = 3000000,
        counter = 13,
        gas_limit = 1,
        storage_limit = 5,
        delegate = 'mv2WfoqcNkcNmzWPndNxXqSe6r7vUY5jXt1w'
    ),
    ScRollupAddMessage(
        source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
        fee = 4000000,
        counter = 14,
        gas_limit = 1,
        storage_limit = 6,
        message = [
            bytes.fromhex('6d65737361676530'),
            bytes.fromhex('6d65737361676531'),
            bytes.fromhex('6d65737361676532'),
            bytes.fromhex('6d65737361676533'),
            bytes.fromhex('6d65737361676534'),
            bytes.fromhex('6d65737361676535'),
            bytes.fromhex('6d65737361676536'),
            bytes.fromhex('6d65737361676537'),
            bytes.fromhex('6d65737361676538'),
            bytes.fromhex('6d65737361676539'),
            bytes.fromhex('6d6573736167653130'),
            bytes.fromhex('6d6573736167653131'),
            bytes.fromhex('6d6573736167653132'),
            bytes.fromhex('6d6573736167653133'),
            bytes.fromhex('6d6573736167653134'),
            bytes.fromhex('6d6573736167653135'),
            bytes.fromhex('6d6573736167653136'),
            bytes.fromhex('6d6573736167653137'),
            bytes.fromhex('6d6573736167653138'),
            bytes.fromhex('6d6573736167653139')
        ]
    ),
    SetDepositLimit(
        source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
        fee = 1000000,
        counter = 15,
        gas_limit = 1,
        storage_limit = 3,
        limit = 10000000
    )
])

def test_sign_basic_too_long_operation(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check sign too long operation"""

    message = BASIC_OPERATION

    mavryk_navigator.toggle_expert_mode()
    mavryk_navigator.toggle_blindsign()

    with backend.sign(account, message, with_hash=True) as result:
        if firmware.is_nano:
            mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "clear_n_too_long_warning")
        else:
            mavryk_navigator.skip_sign(snap_path=snapshot_dir / "skip")
            mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "blindsign_warning")
        mavryk_navigator.accept_sign(snap_path=snapshot_dir / "summary")

    account.check_signature(
        message=message,
        with_hash=True,
        data=result.value
    )

def test_reject_basic_too_long_operation_at_warning(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check reject too long operation at warning"""

    mavryk_navigator.toggle_expert_mode()
    mavryk_navigator.toggle_blindsign()

    with StatusCode.REJECT.expected():
        with backend.sign(account, BASIC_OPERATION):
            if firmware.is_nano:
                mavryk_navigator.refuse_sign_blindsign_risk(snap_path=snapshot_dir / "clear_n_too_long_warning")
            else:
                mavryk_navigator.skip_sign(snap_path=snapshot_dir / "skip")
                mavryk_navigator.refuse_sign_blindsign_risk(snap_path=snapshot_dir / "blindsign_warning")

def test_reject_basic_too_long_operation_at_summary(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check reject too long operation at summary"""

    mavryk_navigator.toggle_expert_mode()
    mavryk_navigator.toggle_blindsign()

    with StatusCode.REJECT.expected():
        with backend.sign(account, BASIC_OPERATION):
            if firmware.is_nano:
                mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "clear_n_too_long_warning")
            else:
                mavryk_navigator.skip_sign(snap_path=snapshot_dir / "skip")
                mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "blindsign_warning")
            mavryk_navigator.reject_sign(snap_path=snapshot_dir / "summary")

@pytest.mark.use_on_device("touch")
def test_reject_at_skip(
        backend: MavrykBackend,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check reject at skip."""

    mavryk_navigator.toggle_expert_mode()
    mavryk_navigator.toggle_blindsign()

    with backend.sign(account, BASIC_OPERATION):
        mavryk_navigator.skip_reject(
            snap_path=snapshot_dir,
            screen_change_after_last_instruction=True,
        )
        # Just to end the flow
        mavryk_navigator.accept_sign(
            screen_change_before_first_instruction=False,
        )


### Different kind of too long operation ###

def test_sign_too_long_operation_with_only_transactions(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check sign too long operation that contains only transaction"""
    message = OperationGroup([
        Transaction(
            source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
            fee = 0,
            counter = 11,
            gas_limit = 1,
            storage_limit = 0,
            destination = 'mv1GQ3j1iCWq1qw3YMhg6bMUTwH9Rdp3SE8e',
            amount = 10000000
        ),
        Transaction(
            source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
            fee = 1000000,
            counter = 12,
            gas_limit = 1,
            storage_limit = 1,
            destination = 'mv2M8TeVU9NCfkdzzdxZJhZmTiuMiFoTu3rs',
            amount = 1000000
        ),
        Transaction(
            source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
            fee = 2000000,
            counter = 13,
            gas_limit = 1,
            storage_limit = 2,
            destination = 'mv1BffkEZbfk39B41da8eZCBBebKZPWugUDX',
            amount = 2000000
        ),
        Transaction(
            source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
            fee = 3000000,
            counter = 14,
            gas_limit = 1,
            storage_limit = 3,
            destination = 'mv3EkfVEWmznKKUhWzUApYJXonS2BxawQBKY',
            amount = 3000000
        ),
        Transaction(
            source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
            fee = 4000000,
            counter = 15,
            gas_limit = 1,
            storage_limit = 4,
            destination = 'mv3CvMoFaDEmMA6mufFv7TmxvdtVLkneSngc',
            amount = 4000000
        ),
        Transaction(
            source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
            fee = 5000000,
            counter = 16,
            gas_limit = 1,
            storage_limit = 5,
            destination = 'mv2X4QWis6hXdqvo75GLK7r8CHLcUgVrQH85',
            amount = 5000000
        )
    ])

    mavryk_navigator.toggle_expert_mode()
    mavryk_navigator.toggle_blindsign()

    with backend.sign(account, message, with_hash=True) as result:
        if firmware.is_nano:
            mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "clear_n_too_long_warning")
        else:
            mavryk_navigator.skip_sign(snap_path=snapshot_dir / "skip")
            mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "blindsign_warning")
        mavryk_navigator.accept_sign(snap_path=snapshot_dir / "summary")

    account.check_signature(
        message=message,
        with_hash=True,
        data=result.value
    )

def test_sign_too_long_operation_without_fee_or_amount(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check sign too long operation that doesn't have fees or amount"""
    message = Proposals(
        source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
        proposals = [
            'ProtoDemoNoopsDemoNoopsDemoNoopsDemoNoopsDemo6XBoYp',
            'ProtoGenesisGenesisGenesisGenesisGenesisGenesk612im',
            'PtAtLasomUEW99aVhVTrqjCHjJSpFUa8uHNEAEamx9v2SNeTaNp',
            'Ps8tUpcuzKw4cTeFT2wJXNCLa9pxkBUWZFDAvb9CXmnAuRE4bzF',
            'ProtoALphaALphaALphaALphaALphaALphaALphaALphaDdp3zK'
        ],
        period = 32
    )

    mavryk_navigator.toggle_expert_mode()
    mavryk_navigator.toggle_blindsign()

    with backend.sign(account, message, with_hash=True) as result:
        if firmware.is_nano:
            mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "clear_n_too_long_warning")
        else:
            mavryk_navigator.skip_sign(snap_path=snapshot_dir / "skip")
            mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "blindsign_warning")
        mavryk_navigator.accept_sign(snap_path=snapshot_dir / "summary")

    account.check_signature(
        message=message,
        with_hash=True,
        data=result.value
    )


### Too long operation containing a too large number ###

OPERATION_WITH_TOO_LARGE = OperationGroup([
    ScRollupAddMessage(
        source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
        fee = 4000000,
        counter = 11,
        gas_limit = 1,
        storage_limit = 6,
        message = [
            bytes.fromhex('6d65737361676530'),
            bytes.fromhex('6d65737361676531'),
            bytes.fromhex('6d65737361676532'),
            bytes.fromhex('6d65737361676533'),
            bytes.fromhex('6d65737361676534'),
            bytes.fromhex('6d65737361676535'),
            bytes.fromhex('6d65737361676536'),
            bytes.fromhex('6d65737361676537'),
            bytes.fromhex('6d65737361676538'),
            bytes.fromhex('6d65737361676539'),
            bytes.fromhex('6d6573736167653130'),
            bytes.fromhex('6d6573736167653131'),
            bytes.fromhex('6d6573736167653132'),
            bytes.fromhex('6d6573736167653133'),
            bytes.fromhex('6d6573736167653134'),
            bytes.fromhex('6d6573736167653135'),
            bytes.fromhex('6d6573736167653136'),
            bytes.fromhex('6d6573736167653137'),
            bytes.fromhex('6d6573736167653138'),
            bytes.fromhex('6d6573736167653139'),
            bytes.fromhex('6d6573736167653230'),
            bytes.fromhex('6d6573736167653231'),
            bytes.fromhex('6d6573736167653232'),
            bytes.fromhex('6d6573736167653233'),
            bytes.fromhex('6d6573736167653234'),
            bytes.fromhex('6d6573736167653235'),
            bytes.fromhex('6d6573736167653236'),
            bytes.fromhex('6d6573736167653237'),
            bytes.fromhex('6d6573736167653238'),
            bytes.fromhex('6d6573736167653239')
        ]
    ),
    RegisterGlobalConstant(
        source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
        fee = 5000000,
        counter = 12,
        gas_limit = 1,
        storage_limit = 3,
        value = {'int': 115792089237316195423570985008687907853269984665640564039457584007913129639936}
    )
])

def test_sign_too_long_operation_with_too_large(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check sign too long operation that will also fail the parsing"""

    message = OPERATION_WITH_TOO_LARGE

    mavryk_navigator.toggle_expert_mode()
    mavryk_navigator.toggle_blindsign()

    with backend.sign(account, message, with_hash=True) as result:
        if firmware.is_nano:
            mavryk_navigator.accept_sign_error_risk(snap_path=snapshot_dir / "clear_n_too_large_warning")
        else:
            mavryk_navigator.skip_sign(snap_path=snapshot_dir / "skip")
            mavryk_navigator.accept_sign_error_risk(snap_path=snapshot_dir / "too_large_warning")
            mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "blindsign_warning")
        mavryk_navigator.accept_sign(snap_path=snapshot_dir / "blindsigning")

    account.check_signature(
        message=message,
        with_hash=True,
        data=result.value
    )

def test_reject_too_long_operation_with_too_large_at_too_large_warning(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check reject too long operation that will also fail the parsing at too large warning"""

    mavryk_navigator.toggle_expert_mode()
    mavryk_navigator.toggle_blindsign()

    with StatusCode.PARSE_ERROR.expected():
        with backend.sign(account, OPERATION_WITH_TOO_LARGE):
            if firmware.is_nano:
                mavryk_navigator.refuse_sign_error_risk(snap_path=snapshot_dir / "clear_n_too_large_warning")
            else:
                mavryk_navigator.skip_sign(snap_path=snapshot_dir / "skip")
                mavryk_navigator.refuse_sign_error_risk(snap_path=snapshot_dir / "too_large_warning")

def test_reject_too_long_operation_with_too_large_at_blindsigning(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check reject too long operation that will also fail the parsing at blindsigning"""

    mavryk_navigator.toggle_expert_mode()
    mavryk_navigator.toggle_blindsign()

    if firmware.is_nano:
        error = StatusCode.REJECT
    else:
        error = StatusCode.PARSE_ERROR

    with error.expected():
        with backend.sign(account, OPERATION_WITH_TOO_LARGE):
            if firmware.is_nano:
                mavryk_navigator.accept_sign_error_risk(snap_path=snapshot_dir / "clear_n_too_large_warning")
            else:
                mavryk_navigator.skip_sign(snap_path=snapshot_dir / "skip")
                mavryk_navigator.accept_sign_error_risk(snap_path=snapshot_dir / "too_large_warning")
                mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "blindsigning_warning")
            mavryk_navigator.reject_sign(snap_path=snapshot_dir / "blindsigning")

@pytest.mark.use_on_device("touch")
def test_reject_too_long_operation_with_too_large_at_blindsigning_warning(
        backend: MavrykBackend,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check reject too long operation that will also fail the parsing at blindsigning"""

    mavryk_navigator.toggle_expert_mode()
    mavryk_navigator.toggle_blindsign()

    with StatusCode.PARSE_ERROR.expected():
        with backend.sign(account, OPERATION_WITH_TOO_LARGE):
            mavryk_navigator.skip_sign(snap_path=snapshot_dir / "skip")
            mavryk_navigator.accept_sign_error_risk(snap_path=snapshot_dir / "too_large_warning")
            mavryk_navigator.refuse_sign_blindsign_risk(snap_path=snapshot_dir / "blindsigning_warning")

def test_blindsign_too_deep(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path):
    """Check blindsigning on too deep expression"""

    expression = MichelineExpr([[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[{'int':42}]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]])

    with backend.sign(account, expression, with_hash=True) as result:
        if firmware == Firmware.NANOS:
            ### Simulate `navigate_review` up to `ACCEPT_RISK` because the nanos screen can look like it hasn't changed.

            instructions: List[Union[NavIns, NavInsID]] = [
                # 'Review operation'
                NavInsID.RIGHT_CLICK,  # 'Expression {{{...{{{'
                NavInsID.RIGHT_CLICK,  # 'Expression {{{...{{{'
                NavInsID.RIGHT_CLICK,  # 'The transaction cannot be trusted.'
                NavInsID.RIGHT_CLICK,  # 'Parsing error ERR_TOO_DEEP'
                NavInsID.RIGHT_CLICK,  # 'Learn More: bit.ly/ledger-tez'
                NavInsID.RIGHT_CLICK,  # 'Accept risk'
                NavInsID.BOTH_CLICK,
            ]

            mavryk_navigator.unsafe_navigate(
                instructions=instructions,
                screen_change_before_first_instruction=True,
                screen_change_after_last_instruction=False,
                snap_path=snapshot_dir / "clear",
            )
        else:
            mavryk_navigator.accept_sign_error_risk(snap_path=snapshot_dir / "clear")
            if not firmware.is_nano:
                mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "blindsigning_warning")

        mavryk_navigator.accept_sign(snap_path=snapshot_dir / "blind")

    account.check_signature(
        message=expression,
        with_hash=True,
        data=result.value
    )

def test_blindsign_too_large(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check blindsigning on too large expression"""

    message = MichelineExpr({'int':12345678901234567890123456789012345678901234567890123456789012345678901234567890})

    with backend.sign(account, message, with_hash=True) as result:
        mavryk_navigator.accept_sign_error_risk(snap_path=snapshot_dir / "clear")
        if not firmware.is_nano:
            mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "blindsigning_warning")
        mavryk_navigator.accept_sign(snap_path=snapshot_dir / "blind")

    account.check_signature(
        message=message,
        with_hash=True,
        data=result.value
    )

def test_blindsign_reject_from_clear(
        backend: MavrykBackend,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check blindsigning rejection"""

    expression = MichelineExpr({'int':12345678901234567890123456789012345678901234567890123456789012345678901234567890})

    with StatusCode.PARSE_ERROR.expected():
        with backend.sign(account, expression):
            mavryk_navigator.refuse_sign_error_risk(snap_path=snapshot_dir)

def test_blindsign_reject_from_blind(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check blindsigning rejection"""

    expression = MichelineExpr({'int':12345678901234567890123456789012345678901234567890123456789012345678901234567890})

    if firmware.is_nano:
        error = StatusCode.REJECT
    else:
        error = StatusCode.PARSE_ERROR

    with error.expected():
        with backend.sign(account, expression, with_hash=False):
            mavryk_navigator.accept_sign_error_risk(snap_path=snapshot_dir / "clear")
            if not firmware.is_nano:
                mavryk_navigator.accept_sign_blindsign_risk(snap_path=snapshot_dir / "blind_warning")
            mavryk_navigator.reject_sign(snap_path=snapshot_dir / "blind")

def test_ensure_always_clearsign(
        backend: MavrykBackend,
        firmware: Firmware,
        mavryk_navigator: MavrykNavigator,
        account: Account,
        snapshot_dir: Path
):
    """Check clear signing never blindsign"""

    mavryk_navigator.toggle_expert_mode()
    if not firmware.is_nano:
        mavryk_navigator.toggle_blindsign()

    message = Transaction(
        source = 'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
        fee = 10000,
        counter = 2,
        gas_limit = 3,
        storage_limit = 4,
        destination = 'KT18amZmM5W7qDWVt2pH6uj7sCEd3kbzLrHT',
        amount = 0,
        entrypoint = 'root',
        parameter = [{'prim':'pair','args':[{'string':"["},{'prim':'pair','args':[{'string':"Z"},{'prim':'pair','args':[{'string':"Y"},{'prim':'pair','args':[{'string':"X"},{'prim':'pair','args':[{'string':"W"},{'prim':'pair','args':[{'string':"V"},{'prim':'pair','args':[{'string':"U"},{'prim':'pair','args':[{'string':"T"},{'prim':'pair','args':[{'string':"S"},{'prim':'pair','args':[{'string':"R"},{'prim':'pair','args':[{'string':"Q"},{'prim':'pair','args':[{'string':"P"},{'prim':'pair','args':[{'string':"O"},{'prim':'pair','args':[{'string':"N"},{'prim':'pair','args':[{'string':"M"},{'prim':'pair','args':[{'string':"L"},{'prim':'pair','args':[{'string':"K"},{'prim':'pair','args':[{'string':"J"},{'prim':'pair','args':[{'string':"I"},{'prim':'pair','args':[{'string':"H"},{'prim':'pair','args':[{'string':"G"},{'prim':'pair','args':[{'string':"F"},{'prim':'pair','args':[{'string':"E"},{'prim':'pair','args':[{'string':"D"},{'prim':'pair','args':[{'string':"C"},{'prim':'pair','args':[{'string':"B"},[]]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]},{'prim':'pair','args':[{'int':10},{'prim':'pair','args':[{'int':9},{'prim':'pair','args':[{'int':8},{'prim':'pair','args':[{'int':7},{'prim':'pair','args':[{'int':6},{'prim':'pair','args':[{'int':5},{'prim':'pair','args':[{'int':4},{'prim':'pair','args':[{'int':3},{'prim':'pair','args':[{'int':2},{'prim':'pair','args':[{'int':1},[]]}]}]}]}]}]}]}]}]}]}]
    )

    with backend.sign(account, message, with_hash=True) as result:
        mavryk_navigator.accept_sign(snap_path=snapshot_dir)

    account.check_signature(
        message=message,
        with_hash=True,
        data=result.value
    )
