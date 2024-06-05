// Copyright 2020 Fancapital Inc.  All rights reserved.
#include "hts_utils.h"
#include "broker/broker.h"
#include "itpdk/itpdk_dict.h"

namespace co {
    string ConvertCharacters(const string& msg) {
        return x::GBKToUTF8(msg);
    }

    string GetErrorMsg(int code, const char* msg) {
        stringstream ss;
        ss << code << "-";
        if (msg) {
            ss << msg;
        }
        return ss.str();
    }

    int64_t Market2Std(char* exchange) {
        int64_t std_market = 0;
        if (strcmp(exchange, "SH") == 0) {
            std_market = co::kMarketSH;
        } else if (strcmp(exchange, "SZ") == 0) {
            std_market = co::kMarketSZ;
        } else {
            LOG_ERROR << "not parse market: " << exchange;
        }
        return static_cast<int64_t>(std_market);
    }

    int64_t GetTimeStamp(int64_t date, char* time) {
        string s_time = time;
        boost::algorithm::replace_all(s_time, ":", "");
        boost::algorithm::replace_all(s_time, ".", "");
        int64_t i_time = atoll(s_time.c_str());
        int64_t timestamp = date * 1000000000LL + i_time % 1000000000LL;
        return timestamp;
    }

    int64_t BsFlag2Std(int64_t type) {
        int64_t std_bs_flag = 0;
        switch (type) {
        case JYLB_BUY:  // 买
            std_bs_flag = co::kBsFlagBuy;
            break;
        case JYLB_SALE:  // 卖
            std_bs_flag = co::kBsFlagSell;
            break;
        case JYLB_HGRQ:  // 逆回购
            std_bs_flag = co::kBsFlagSell;
            break;
        case JYLB_ETFSG:  // 申购
            std_bs_flag = co::kBsFlagCreate;
            break;
        case JYLB_ETFSH:  // 赎回
            std_bs_flag = co::kBsFlagRedeem;
            break;
        case JYLB_PSSG:  // 配售申购
            std_bs_flag = co::kBsFlagBuy;
            break;
        case JYLB_RGFX:  // 认购发行
            std_bs_flag = co::kBsFlagBuy;
            break;
        default:
            LOG_ERROR << "not parse bs type: " << type;
            break;
        }
        return std_bs_flag;
    }

    int64_t OrderStatus2Std(int order_state) {
        // 委托状态：0：未申报，1：申报中，2：已申报，3：非法委托，4：资金申请中，(5：部分成交)，(6：全部成交)，(7：部成部撤)，(8：全部撤单)，(9：撤单未成)
        int64_t std_order_state = 0;
        switch (order_state) {
        case 6:  /// 全部成交
            std_order_state = kOrderFullyKnocked;
            break;
        case 5:  /// 部分成交还在队列中
            std_order_state = kOrderPartlyKnocked;
            break;
        case 7:  /// 部分成交不在队列中，即“部成部撤”
            std_order_state = kOrderPartlyCanceled;
            break;
        case 8:  /// 撤单
                std_order_state = kOrderFullyCanceled;
            break;
        case 9:  /// 撤单未成
            std_order_state = kOrderCreated;
            break;
        case 0:
        case 1:
        case 4:
            std_order_state = kOrderNotSend;
            break;
        case 2:
            std_order_state = kOrderCreated;
            break;
        case 3:
            std_order_state = kOrderFailed;
            break;
        default:
            LOG_ERROR << "not parse order_state: " << order_state;
            break;
        }
        return std_order_state;
    }

    int64_t OrderPriceType2Std(int price_type) {
        int64_t ret = 0;
//        switch (price_type) {
//        case kQOrderTypeMarket:
//            ret = kQOrderTypeMarket;  /// 市价
//            break;
//        case DDLX_XJWT:
//            ret = kQOrderTypeLimit;   /// 限价
//            break;
//        case TORA_TSTP_OPT_HomeBestPrice:
//            ret = kQOrderTypeSelfMarket;  /// 本方最优
//            break;
//        default:
//            LOG_ERROR << "not parse order price type: " << price_type;
//            break;
//        }
        return ret;
    }

    int StdBsFlag2Hts(int64_t bs_flag) {
        int hts_bs_flag = 0;
        switch (bs_flag) {
        case kBsFlagBuy:
            hts_bs_flag = JYLB_BUY;
            break;
        case kBsFlagSell:
            hts_bs_flag = JYLB_SALE;
            break;
        case kBsFlagCreate:
            hts_bs_flag = JYLB_ETFSG;
            break;
        case kBsFlagRedeem:
            hts_bs_flag = JYLB_ETFSH;
            break;
        default:
            xthrow() << "unsupported std bs_flag for hts: " << bs_flag;
            break;
        }
        return hts_bs_flag;
    }
}  // namespace co
