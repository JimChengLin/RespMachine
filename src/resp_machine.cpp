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
    if (!sv.empty() && sv.back() == '\t') {
        sv.remove_suffix(1);
    }

    pos = 0;
    while (true) {
        auto next_pos = sv.find(' ', pos);
        if (next_pos == std::string::npos) {
            argv_.emplace_back(sv.data() + pos, sv.size() - 1 - pos);
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