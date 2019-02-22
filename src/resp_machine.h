#pragma once
#ifndef RESPMACHINE_RESP_MACHINE_H
#define RESPMACHINE_RESP_MACHINE_H

#include <cstddef>
#include <string_view>
#include <vector>

class RespMachine {
public:
    enum State {
        kError = -1,
        kSuccess = 0,
        kInit,
        kProcess,
    };

    enum ReqType {
        kInline,
        kMultiBulk,
        kUnknown,
    };

public:
    size_t Input(const char * s, size_t n);

    State GetState() const { return state_; }

    ReqType GetReqType() const { return req_type_; }

    const std::vector<std::string_view> &
    GetArgv() const { return argv_; }

    void Reset();

private:
    size_t ProcessInlineInput(const char * s, size_t n);

    size_t ProcessMultiBulkInput(const char * s, size_t n);

private:
    State state_ = kInit;
    ReqType req_type_ = kUnknown;
    std::vector<std::string_view> argv_;
};

#endif //RESPMACHINE_RESP_MACHINE_H