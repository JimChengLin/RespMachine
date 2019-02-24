#include <cassert>
#include <string>

#include "resp_machine.h"

/* Convert a string into a long long. Returns 1 if the string could be parsed
 * into a (non-overflowing) long long, 0 otherwise. The value will be set to
 * the parsed value when appropriate.
 *
 * Note that this function demands that the string strictly represents
 * a long long: no spaces or other characters before or after the string
 * representing the number are accepted, nor zeroes at the start if not
 * for the string "0" representing the zero number.
 *
 * Because of its strictness, it is safe to use this function to check if
 * you can convert a string into a long long, and obtain back the string
 * from the number without any loss in the string representation. */
static int string2ll(const char * s, size_t slen, long long * value) {
    const char * p = s;
    size_t plen = 0;
    int negative = 0;
    unsigned long long v;

    /* A zero length string is not a valid number. */
    if (plen == slen)
        return 0;

    /* Special case: first and only digit is 0. */
    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return 1;
    }

    /* Handle negative numbers: just set a flag and continue like if it
     * was a positive number. Later convert into negative. */
    if (p[0] == '-') {
        negative = 1;
        p++;
        plen++;

        /* Abort on only a negative sign. */
        if (plen == slen)
            return 0;
    }

    /* First digit should be 1-9, otherwise the string should just be 0. */
    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0] - '0';
        p++;
        plen++;
    } else {
        return 0;
    }

    /* Parse all the other digits, checking for overflow at every step. */
    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        if (v > (ULLONG_MAX / 10)) /* Overflow. */
            return 0;
        v *= 10;

        if (v > (ULLONG_MAX - (p[0] - '0'))) /* Overflow. */
            return 0;
        v += p[0] - '0';

        p++;
        plen++;
    }

    /* Return if not all bytes were used. */
    if (plen < slen)
        return 0;

    /* Convert to negative if needed, and do the final overflow check when
     * converting from unsigned long long to long long. */
    if (negative) {
        if (v > ((unsigned long long) (-(LLONG_MIN + 1)) + 1)) /* Overflow. */
            return 0;
        if (value != NULL) *value = -v;
    } else {
        if (v > LLONG_MAX) /* Overflow. */
            return 0;
        if (value != NULL) *value = v;
    }
    return 1;
}

/* Return the number of digits of 'v' when converted to string in radix 10.
 * See ll2string() for more information. */
static uint32_t digits10(uint64_t v) {
    if (v < 10) return 1;
    if (v < 100) return 2;
    if (v < 1000) return 3;
    if (v < 1000000000000UL) {
        if (v < 100000000UL) {
            if (v < 1000000) {
                if (v < 10000) return 4;
                return 5 + (v >= 100000);
            }
            return 7 + (v >= 10000000UL);
        }
        if (v < 10000000000UL) {
            return 9 + (v >= 1000000000UL);
        }
        return 11 + (v >= 100000000000UL);
    }
    return 12 + digits10(v / 1000000000000UL);
}

/* Convert a long long into a string. Returns the number of
 * characters needed to represent the number.
 * If the buffer is not big enough to store the string, 0 is returned.
 *
 * Based on the following article (that apparently does not provide a
 * novel approach but only publicizes an already used technique):
 *
 * https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920
 *
 * Modified in order to handle signed integers since the original code was
 * designed for unsigned integers. */
static int ll2string(char * dst, size_t dstlen, long long svalue) {
    static const char digits[201] =
            "0001020304050607080910111213141516171819"
            "2021222324252627282930313233343536373839"
            "4041424344454647484950515253545556575859"
            "6061626364656667686970717273747576777879"
            "8081828384858687888990919293949596979899";
    int negative;
    unsigned long long value;

    /* The main loop works with 64bit unsigned integers for simplicity, so
     * we convert the number here and remember if it is negative. */
    if (svalue < 0) {
        if (svalue != LLONG_MIN) {
            value = -svalue;
        } else {
            value = ((unsigned long long) LLONG_MAX) + 1;
        }
        negative = 1;
    } else {
        value = svalue;
        negative = 0;
    }

    /* Check length. */
    uint32_t const length = digits10(value) + negative;
    if (length >= dstlen) return 0;

    /* Null term. */
    uint32_t next = length;
    dst[next] = '\0';
    next--;
    while (value >= 100) {
        int const i = (value % 100) * 2;
        value /= 100;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
        next -= 2;
    }

    /* Handle last 1-2 digits. */
    if (value < 10) {
        dst[next] = '0' + (uint32_t) value;
    } else {
        int i = (uint32_t) value * 2;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
    }

    /* Add sign. */
    if (negative) dst[0] = '-';
    return length;
}

size_t RespMachine::Input(const char * s, size_t n) {
    state_ = kProcess;
    if (req_type_ == kUnknown) {
        req_type_ = (*s == '*' ? kMultiBulk : kInline);
    }
    if (req_type_ == kMultiBulk) {
        return ProcessMultiBulkInput(s, n);
    } else {
        assert(req_type_ == kInline);
        return ProcessInlineInput(s, n);
    }
}

size_t RespMachine::ProcessInlineInput(const char * s, size_t n) {
    std::string_view sv(s, n);

    /* Search for end of line */
    auto pos = sv.find('\n');

    /* Nothing to do without a \r\n */
    if (pos == std::string::npos) {
        return 0;
    }
    size_t consume_len = pos + 1;

    sv = {s, pos};
    /* Handle the \r\n case. */
    if (!sv.empty() && sv.back() == '\r') {
        sv.remove_suffix(1);
    }

    pos = 0;
    while (true) {
        auto next_pos = sv.find(' ', pos);
        if (next_pos == std::string::npos) {
            argv_.emplace_back(sv.data() + pos, sv.size() - pos);
            break;
        }
        argv_.emplace_back(sv.data() + pos, next_pos - pos);
        pos = next_pos + 1;
    }
    state_ = kSuccess;
    return consume_len;
}

size_t RespMachine::ProcessMultiBulkInput(const char * s, size_t n) {
    std::string_view sv(s, n);

    size_t consume_len = 0;
    if (multi_bulk_len_ == 0) {
        /* Multi bulk length cannot be read without a \r\n
         * Buffer should also contain \n */
        auto pos = sv.find("\r\n");
        if (pos == std::string::npos) {
            return 0;
        }

        /* skip prefix */
        sv = {s + 1, pos - 1};
        long long ll;
        int ok = string2ll(sv.data(), sv.size(), &ll);
        if (!ok) {
            state_ = kInvalidMultiBulkLengthError;
            return 0;
        }
        consume_len = pos + 2;

        if (ll <= 0) {
            state_ = kSuccess;
            return consume_len;
        }
        multi_bulk_len_ = static_cast<int>(ll);
    }

    while (multi_bulk_len_ != 0) {
        assert(multi_bulk_len_ > 0);

        /* Read bulk length if unknown */
        if (bulk_len_ == -1) {
            sv = {s + consume_len, n - consume_len};
            auto pos = sv.find("\r\n");
            if (pos == std::string::npos) {
                return consume_len;
            }

            sv = {sv.data(), pos};
            if (sv.front() != '$') {
                state_ = kDollarSignNotFoundError;
                return 0;
            }
            sv.remove_prefix(1);

            long long ll;
            int ok = string2ll(sv.data(), sv.size(), &ll);
            if (!ok || ll < 0) {
                state_ = kInvalidBulkLength;
                return 0;
            }

            consume_len += pos + 2;
            bulk_len_ = static_cast<int>(ll);
        }

        /* Read bulk argument */
        sv = {s + consume_len, n - consume_len};
        int bulk_read_len = bulk_len_ + 2; /* +2 == trailing \r\n */
        if (sv.size() < bulk_read_len) {
            break;
        } else {
            argv_.emplace_back(sv.data(), bulk_len_);
            consume_len += bulk_read_len;
            bulk_len_ = -1;
            --multi_bulk_len_;
        }
    }

    if (multi_bulk_len_ == 0) {
        state_ = kSuccess;
    }
    return consume_len;
}

void RespMachine::Reset() {
    state_ = kInit;
    req_type_ = kUnknown;
    argv_.clear();
    multi_bulk_len_ = 0;
    bulk_len_ = -1;
}

void RespMachine::AppendSimpleString(std::string * buf, const char * s, size_t n) {
    buf->push_back('+');
    buf->append(s, n);
    buf->append("\r\n");
}

void RespMachine::AppendError(std::string * buf, const char * s, size_t n) {
    buf->push_back('-');
    buf->append(s, n);
    buf->append("\r\n");
}

void RespMachine::AppendInteger(std::string * buf, long long ll) {
    buf->push_back(':');
    char lls[32];
    int lls_len = ll2string(lls, sizeof(lls), ll);
    buf->append(lls, static_cast<size_t>(lls_len));
    buf->append("\r\n");
}

void RespMachine::AppendBulkString(std::string * buf, const char * s, size_t n) {
    buf->push_back('$');
    char lls[32];
    int lls_len = ll2string(lls, sizeof(lls), static_cast<long long>(n));
    buf->append(lls, static_cast<size_t>(lls_len));
    buf->append("\r\n");
    buf->append(s, n);
    buf->append("\r\n");
}

void RespMachine::AppendArrayLength(std::string * buf, long long len) {
    buf->push_back('*');
    char lls[32];
    int lls_len = ll2string(lls, sizeof(lls), len);
    buf->append(lls, static_cast<size_t>(lls_len));
    buf->append("\r\n");
}

void RespMachine::AppendNullBulkString(std::string * buf) {
    buf->append("$-1\r\n");
}

void RespMachine::AppendNullArray(std::string * buf) {
    buf->append("*-1\r\n");
}