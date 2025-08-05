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

"""Gathering of tests related to Set-consensus-key operations."""

from utils.message import UpdateConsensusKey
from .helper import Flow, Field, TestOperation, pytest_generate_tests


class TestUpdateConsensusKey(TestOperation):
    """Commun tests."""

    @property
    def op_class(self):
        return UpdateConsensusKey

    flows = [Flow('basic')]

    fields = [
        Field("pk", "Public key", [
            Field.Case('edpkvMUjmJu9CYyKBAjUV3jtU8Y89TemDAcD29bSNh393Bc8z8BH3t', "mv1"),
            Field.Case('sppk7ZT8R42AGSy672NHz9ps6Q4idqWYejAgMwqTWnyYAeq9XZEqWvZ', "mv2"),
            Field.Case('p2pk665znpiyPRWEwpu8tZ7JdNPipkfYpGUhYALjaS4Tm7F7wcx1iRs', "mv3"),
            Field.Case('BLpk1koaE6qJifAmUjjeukrgUdZaHCWWcHj6fBqrQLSWvVwHfqNcKKCSv5GxxVHhGirQbjHFsTTk', "mv4"),
            Field.Case('edpkuWUfaAWqaxJoG9QKgQRQUHMWfsN1EmoMMXWMwYoE8kjWMWUGDk', "long-hash"),
        ]),
    ]
