// Copyright 2020 Fancapital Inc.  All rights reserved.
#pragma once
#include <string>
#include <chrono>
#include "boost/algorithm/string.hpp"
#include "coral/coral.h"

namespace co {

    string ConvertCharacters(const string& msg);

    string GetErrorMsg(int code, const char* msg);

    int64_t Market2Std(char* exchange);

    int64_t GetTimeStamp(int64_t date, char* time);

    int64_t BsFlag2Std(int64_t type);

    int64_t OrderStatus2Std(int order_state);

    int64_t OrderPriceType2Std(int price_type);

    int StdBsFlag2Hts(int64_t bs_flag);
}  // namespace co


