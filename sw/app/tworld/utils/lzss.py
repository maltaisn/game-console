from collections import deque

# TODO https://michaeldipperstein.github.io/lzss.html
#  - optimize dictionnary search

# Simple LZSS implementation
# - Token type is indicated by a byte, with a 0 bit indicating a raw byte and a 1 bit indicating
#   a back reference. The first token of the stream is always a token type byte.
#   When 8 tokens have been read, a new type token is placed.
# - Two back reference encodings (number in parentheses indicate value subtracted from real value,
#     all fields separated by commas are concataned to form 1 or 2 bytes):
#    - 2 bytes: 8-bit distance (-1), 7-bit length (-3), 1-bit flag = 0x1
#    - 1 byte:  5-bit distance (-1), 2-bit length (-2), 1-bit flag = 0x0
# - 256 bytes window size

DISTANCE_BITS1 = 5
DISTANCE_BITS2 = 8

LENGTH_BITS1 = 7 - DISTANCE_BITS1
LENGTH_BITS2 = 15 - DISTANCE_BITS2

WINDOW_SIZE = 2 ** max(DISTANCE_BITS1, DISTANCE_BITS2)

BREAKEVEN1 = 2
BREAKEVEN2 = 3

MAX_DISTANCE1 = 2 ** DISTANCE_BITS1
MAX_LENGTH1 = 2 ** LENGTH_BITS1 - 1 + BREAKEVEN1

MAX_DISTANCE2 = 2 ** DISTANCE_BITS2
MAX_LENGTH2 = 2 ** LENGTH_BITS2 - 1 + BREAKEVEN2

LENGTH_MASK1 = 2 ** LENGTH_BITS1 - 1
LENGTH_MASK2 = 2 ** LENGTH_BITS2 - 1

def encode(data: bytes) -> bytes:
    out = bytearray()
    window = deque()

    type_bits = 8
    type_pos = 0

    def append_token_type(type: int) -> None:
        nonlocal type_bits, type_pos
        if type_bits == 8:
            type_bits = 0
            type_pos = len(out)
            out.append(0)
        out[type_pos] = out[type_pos] | (type << type_bits)
        type_bits += 1

    i = 0

    while i < len(data):
        # find maximum subsequence length within window
        max_seq_pos = -1
        max_seq = 0
        for j in reversed(range(len(window))):
            start_pos = j
            seq_len = 0
            while window[j] == data[i]:
                i += 1
                j += 1
                seq_len += 1
                if seq_len == MAX_LENGTH2 or i == len(data):
                    break
                if j == len(window):
                    j = start_pos
            i -= seq_len
            if seq_len > max_seq:
                max_seq_pos = start_pos
                max_seq = seq_len
                if seq_len == len(data) - i:
                    # reached end of data, no sequence will be longer
                    break

        distance = len(window) - max_seq_pos
        single_byte_backref = max_seq <= MAX_LENGTH1 and distance <= MAX_DISTANCE1
        if single_byte_backref and max_seq >= BREAKEVEN1 or max_seq >= BREAKEVEN2:
            # append back ref token
            append_token_type(1)
            if single_byte_backref:
                # single byte encoding
                backref = (distance - 1) << (LENGTH_BITS1 + 1) | (max_seq - BREAKEVEN1) << 1 | 0x0
                out.append(backref)
            else:
                # two bytes encoding
                backref = (distance - 1) << (LENGTH_BITS2 + 1) | (max_seq - BREAKEVEN2) << 1 | 0x1
                out += backref.to_bytes(2, "little")
            # update window
            for j in range(max_seq):
                b = window[max_seq_pos + j]
                window.append(b)
            i += max_seq
        else:
            # append byte token
            append_token_type(0)
            b = data[i]
            out.append(b)
            # update window
            window.append(b)
            i += 1
        while len(window) > WINDOW_SIZE:
            window.popleft()

    return out


def decode(data: bytes) -> bytes:
    out = bytearray()

    type_byte = 0
    type_bits = 0
    i = 0
    while i < len(data):
        if type_bits == 0:
            type_byte = data[i]
            type_bits = 8
            i += 1

        if type_byte & 1:
            # back reference
            if data[i] & 0x1:
                # two bytes encoding
                backref = (data[i] | data[i + 1] << 8) >> 1
                length = (backref & LENGTH_MASK2) + BREAKEVEN2
                distance = (backref >> LENGTH_BITS2) + 1
                i += 2
            else:
                # single byte encoding
                backref = data[i] >> 1
                length = (backref & LENGTH_MASK1) + BREAKEVEN1
                distance = (backref >> LENGTH_BITS1) + 1
                i += 1
            start_pos = len(out) - distance
            for j in range(length):
                out.append(out[start_pos + j])
        else:
            # byte
            out.append(data[i])
            i += 1
        type_byte >>= 1
        type_bits -= 1

    return out
