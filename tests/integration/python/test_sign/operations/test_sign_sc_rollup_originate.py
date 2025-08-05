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

"""Gathering of tests related to Smart-rollup Originate operations."""

from utils.message import ScRollupOriginate
from .helper import Flow, Field, TestOperation, pytest_generate_tests


class TestScRollupOriginate(TestOperation):
    """Commun tests."""

    @property
    def op_class(self):
        return ScRollupOriginate

    flows = [
        Flow(
            'basic',
            kernel='0123456789ABCDEF',
            whitelist=['mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb']
        ),
        Flow('no-whitelist', whitelist=None),
        Flow('empty-whitelist', whitelist=[])
    ]

    fields = [
        Field("pvm_kind", "Kind", [
            Field.Case('arith', "arith"),
            Field.Case('wasm_2_0_0', "wasm_2_0_0"),
            Field.Case('riscv', "riscv"),
        ]),
        Field("kernel", "Kernel", [
            Field.Case('', 'empty'),
            Field.Case('0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF', 'long'),
        ]),
        Field("parameters_ty", "Parameters", [
            Field.Case({'prim': 'unit'}, "unit"),
            Field.Case({'prim': 'or', 'args': [{'prim': 'int'}, {'prim': 'string'}]}, "basic"),
            # More test about Micheline in micheline tests
        ]),
        Field("whitelist", "Whitelist", [
            Field.Case([
                'mv1P8PBEjfFUb7EeHNBUa92vNHnJmvzUmhxb',
                'mv2WfoqcNkcNmzWPndNxXqSe6r7vUY5jXt1w',
                'mv3ReSbkBZcGNKR8sbZv4iUCAJptUkS92aYf',
                'mv4Rp1nbkBhZ2WHtgSVFG96CHTFfodV2dHcN',
                'mv1GQ3j1iCWq1qw3YMhg6bMUTwH9Rdp3SE8e',
                'mv2X4QWis6hXdqvo75GLK7r8CHLcUgVrQH85',
                'mv3EkfVEWmznKKUhWzUApYJXonS2BxawQBKY',
                'mv4dLGDmFMvszYjc4SVEakhkWpfeXZeHCZn3',
                'mv1JL5z9RtcHNw2s7ooUhAmbUQgsSVuVjWBN',
                'mv2gtGTc4STZHSs7jmDRSpEYNDVnaHpdxMDg',
                'mv3CvMoFaDEmMA6mufFv7TmxvdtVLkneSngc',
                'mv4SBEBRnXy5Luqcs3VnWVGNDRWrJD6QYxJR',
                'mv1LWniiXs5uisueup31x9Ygwya1kYNtfwCT',
                'mv2N3deVPxxcEHKhkjvKvkX8zWEnwGHWAim9',
                'mv3GCpKNVBmRjdWNo36AGnRNxcV9GJGqwvNk',
                'mv4QPPcbvjDmt2ENtEPWhabpAzqypkwaUGrn',
                'mv1CJUu68v9z7Ai7Mmgntb8zRhzqDdHSgHaH',
                'mv2M8TeVU9NCfkdzzdxZJhZmTiuMiFoTu3rs',
                'mv3QCpWQGACyThRMof29RTZEpMMWgeKRmvUh',
                'mv4Vmjo5GCph8uQi1QYgjZG73u1YC2xeFHBk',
            ], "many"),  # Max 4096
            Field.Case([
                'mv1BffkEZbfk39B41da8eZCBBebKZPWugUDX',
                'mv2Pi3UC5kypRZU1Hm8Uomsv7vEwBJNearFT',
                'mv3Thf4FrSnZvQxuep5zM5BcKDZJy6zKZ5VW',
                'mv4Mw77H7vPfQT57FdbH3gd7BRaWuof4uNtG',
            ], "long-hash"),
        ]),
    ]
