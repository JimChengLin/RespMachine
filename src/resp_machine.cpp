#include <cassert>
#include <string>

#include "resp_machine.h"

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

    argv_.clear();
    pos = 0;
    while (true) {
        auto space_pos = sv.find(' ', pos);
        if (space_pos == std::string::npos) {
            argv_.emplace_back(sv.data() + pos, sv.size() - pos);
            break;
        }
        argv_.emplace_back(sv.data() + pos, space_pos - pos);
        pos = space_pos + 1;
    }
    state_ = kSuccess;
    return consume_len;
}

size_t RespMachine::ProcessMultiBulkInput(const char * s, size_t n) {
    std::string_view sv(s, n);
}

void RespMachine::Reset() {
    state_ = kInit;
    req_type_ = kUnknown;
    argv_.clear();
}