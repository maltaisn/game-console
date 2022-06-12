#  Copyright 2022 Nicolas Maltais
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import re
import sys

SYMBOL_TYPES = "bdrt"

# symbols matching regexes in the blacklist won't be output,
# except if they match one of those in the whitelist.
SYMBOL_BLACKLIST = [
    "[._].*",
]
SYMBOL_WHITELIST = [
    "__div.*",
    "__udiv.*",
    "__mul.*",
    "__umul.*",
    "__prologue_saves__",
    "__epilogue_restores__",
]


def main():
    symbols_str = sys.stdin.read()

    blacklist_comp = [re.compile(e) for e in SYMBOL_BLACKLIST]
    whitelist_comp = [re.compile(e) for e in SYMBOL_WHITELIST]

    # build a dictionnary of all symbols and their address
    # keep only certain types (functions, variables, etc)
    symbols = {}
    for line in symbols_str.splitlines():
        match = re.match(r"^([\da-f]+)\s+(\w)\s+(.*)$", line)
        if match:
            symbol_type = match.group(2).lower()
            if symbol_type not in SYMBOL_TYPES:
                continue

            symbol_name = match.group(3)
            if any((r.match(symbol_name) for r in blacklist_comp)) and not \
                    any((r.match(symbol_name) for r in whitelist_comp)):
                continue

            symbols[symbol_name] = int(match.group(1), 16)

    # output symbols in name order
    names_sorted = list(symbols.keys())
    names_sorted.sort()
    for name in names_sorted:
        print(f"{name} = 0x{symbols[name]:06x};", file=sys.stdout)


if __name__ == '__main__':
    main()
