// Copyright 2020 Fancapital Inc.  All rights reserved.
#include "hts_utils.h"
#include "hts_broker.h"

#define SPLITFLAG "-"
#define MAX_QUERY_NUM 200

namespace co {
    co::HtsBroker* MidBridge::broker_ = nullptr;

    HtsBroker::HtsBroker() {
    }

    HtsBroker::~HtsBroker() {
    }

    void HtsBroker::Start() {
        OnInit();
    }

    void HtsBroker::OnInit() {
        Singleton<Config>::Instance();
        Singleton<Config>::GetInstance()->Init();
        investor_id_ = Singleton<Config>::GetInstance()->account();
        custom_id_ = Singleton<Config>::GetInstance()->custom();
        key_name_ = Singleton<Config>::GetInstance()->system_name();
        pwd_ = Singleton<Config>::GetInstance()->password();
        MemTradeAccount acc;
        acc.type = kTradeTypeSpot;
        strncpy(acc.fund_id, investor_id_.c_str(), investor_id_.length());
        acc.batch_order_size = 100;
        AddAccount(acc);
        InputReverseRepurchase();
        int64_t micro_second = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now().time_since_epoch()).count();
        start_index_ = (x::RawTime() * 1000LL + micro_second % 1000LL) * 1000LL;
        batch_start_index_ = x::RawTime() / 1000;
        date_ = x::RawDate();
        LOG_INFO << "Start RequestID: " << start_index_ << ", batch_start_index: " << batch_start_index_;

        string itpdk_path = Singleton<Config>::GetInstance()->itpdk_path();
        string wtfs = Singleton<Config>::GetInstance()->wtfs();
        string node = Singleton<Config>::GetInstance()->node();
        SECITPDK_SetProfilePath(itpdk_path.c_str());
        SECITPDK_Init(0);
        SECITPDK_SetWTFS(wtfs.c_str());
        SECITPDK_SetNode(node.c_str());
        SECITPDK_SetWriteLog(true);
        SECITPDK_SetFixWriteLog(true);

        MidBridge::broker_ = this;
        SECITPDK_SetMsgCallback(MidBridge::SetMsgCallback);
        SECITPDK_SetFuncCallback(MidBridge::SetFuncCallback);
        SECITPDK_SetConnEventCallback(MidBridge::SetConnEventCallback);

        ReqLogin();
        ReqQueryAccount();
        LOG_INFO << "init hts account finished.";
    }

    void HtsBroker::ReqLogin() {
        try {
            login_flag_.store(false);
            while (true) {
                LOG_INFO << "key_name: " << key_name_ << ", pwd: " << pwd_ << ", custom_id: " << custom_id_;
                int64 nRet = SECITPDK_TradeLogin(key_name_.c_str(), custom_id_.c_str(), pwd_.c_str());
                LOG_INFO << "SECITPDK_TradeLogin ";
                if (nRet <= 0) {
                    char msg[1024];
                    SECITPDK_GetLastError(msg);
                    LOG_ERROR << "Login failed. Msg: " << ConvertCharacters(msg);
                } else {
                    LOG_INFO << "Login success";
                    break;
                }
                x::Sleep(3000);
            }
            login_flag_.store(true);
        } catch (std::exception& e) {
            LOG_ERROR << "ReqLogin failed, " << e.what();
        }
    }

    void HtsBroker::ReqQueryAccount() {
        std::vector<ITPDK_GDH> arGDH(10);
        while (true) {
            arGDH.clear();
            int64 nRet = SECITPDK_QueryAccInfo(custom_id_.c_str(), arGDH);
            if (nRet < 0) {
                char msg[1024];
                SECITPDK_GetLastError(msg);
                LOG_ERROR << "query account failed. Msg: " << ConvertCharacters(msg);
            } else {
                break;
            }
            x::Sleep(3000);
        }

        for (int index = 0; index < static_cast<int>(arGDH.size()); ++index) {
            LOG_INFO << "AccountId:" << arGDH[index].AccountId << ", SecuAccount:" << arGDH[index].SecuAccount
                << "; Market:" << arGDH[index].Market;
            int64_t market = Market2Std(arGDH[index].Market);
            if (market > 0) {
                accounts_[market] = arGDH[index];
            }
        }
    }

    void HtsBroker::OnQueryTradeAsset(MemGetTradeAssetMessage* req) {
        try {
            vector<ITPDK_ZJZH> arZJZH(10);
            std::vector<MemTradeAsset> tmp_record;
            string error;
            if (login_flag_) {
                int64 nRet = SECITPDK_QueryFundInfo(custom_id_.c_str(), arZJZH);
                if (nRet < 0) {
                    char _error_msg[1024];
                    SECITPDK_GetLastError(_error_msg);
                    error = "query asset failed! error:" + ConvertCharacters(_error_msg);
                } else {
                    for (auto& it : arZJZH) {
                        LOG_ERROR << "AccountId: " << it.AccountId
                               << ", CurrentBalance: " << it.CurrentBalance
                               << ", FrozenBalance: " << it.FrozenBalance
                               << ", FundAvl: " << it.FundAvl
                               << ", TotalAsset: " << it.TotalAsset
                               << ", MarketValue: " << it.MarketValue;
                        MemTradeAsset asset;
                        memset(&asset, 0, sizeof(asset));
                        if (strcmp(it.MoneyType, "RMB") == 0) {
                            asset.timestamp = x::RawDateTime();
                            strcpy(asset.fund_id, investor_id_.c_str());
                            asset.balance = it.CurrentBalance;
                            asset.usable = it.FundAvl;
                            asset.frozen = it.FrozenBalance;
                            tmp_record.push_back(asset);
                        }
                    }
                }
            } else {
                error = "query spot asset failed: not login in.";
            }
            int total_num = tmp_record.size();
            int length = sizeof(MemGetTradeAssetMessage) + sizeof(MemTradeAsset) * total_num;
            char buffer[length] = "";
            MemGetTradeAssetMessage* rep = (MemGetTradeAssetMessage*)buffer;
            memcpy(rep, req, sizeof(MemGetTradeAssetMessage));
            rep->items_size = total_num;
            if (total_num) {
                MemTradeAsset* first = (MemTradeAsset*)((char*)buffer + sizeof(MemGetTradeAssetMessage));
                for (int i = 0; i < total_num; i++) {
                    MemTradeAsset* item = first + i;
                    memcpy(item, &tmp_record[i], sizeof(MemTradeAsset));
                }
            }
            if (!error.empty()) {
                strcpy(rep->error, error.c_str());
            }
            SendRtnMessage(string(buffer, length), kMemTypeQueryTradeAssetRep);
        } catch (std::exception& e) {
            LOG_ERROR << "OnQueryTradeAsset: " << e.what();
        }
    }

    void HtsBroker::OnQueryTradePosition(MemGetTradePositionMessage* req) {
        try {
            string error;
            std::vector<MemTradePosition> all_pos;
            if (login_flag_) {
                int64 nBrowindex = 0;
                vector<ITPDK_ZQGL> arZqgl;
                vector<ITPDK_ZQGL> temp_vec;
                while (true) {
                    temp_vec.clear();
                    int64 nRet = SECITPDK_QueryPositions(custom_id_.c_str(), SORT_TYPE_AES, 0, nBrowindex,
                                                         "", "", "", 1, temp_vec);
                    if (nRet < 0) {
                        char _error_msg[1024];
                        SECITPDK_GetLastError(_error_msg);
                        error = "query position failed! error:" + ConvertCharacters(_error_msg);
                        break;
                    } else if (nRet == 0) {
                        LOG_INFO << "query position record num is zero, nBrowindex: " << nBrowindex;
                        break;
                    } else {
                        if (static_cast<int> (temp_vec.size()) != nRet) {
                            LOG_ERROR << "query size:" << temp_vec.size() << "is not equal return value: " << nRet;
                        }
                        for (auto& it : temp_vec) {
                            arZqgl.push_back(it);
                            nBrowindex = it.BrowIndex + 1;
                        }
                        LOG_INFO << "query position success. record num:" << nRet << ",  nBrowindex: " << nBrowindex;
                        if (nRet < MAX_QUERY_NUM) {
                            break;
                        }
                    }
                }

                for (auto& it : arZqgl) {
//                    __info << "StockCode:" << it.StockCode
//                        << ", CurrentQty:" << it.CurrentQty
//                        << ", QtyAvl:" << it.QtyAvl
//                        << ", MarketValue:" << it.MarketValue
//                        << ", BrowIndex:" << it.BrowIndex;
                    MemTradePosition item;
                    memset(&item, 0, sizeof(item));
                    item.type = co::kTradeTypeSpot;
                    strcpy(item.fund_id, custom_id_.c_str());
                    item.market = Market2Std(it.Market);
                    string code = x::Trim(it.StockCode) + "." + x::Trim(it.Market);
                    strcpy(item.code, code.c_str());
                    string name = ConvertCharacters(it.StockName);
                    strcpy(item.name, name.c_str());
                    if (IsShReverseRepurchase(code) || IsShConvertibleBond(code)) {
                        item.long_volume = it.CurrentQty * 10;
                        item.long_can_close = it.QtyAvl * 10;
                    } else {
                        item.long_volume = it.CurrentQty;
                        item.long_can_close = it.QtyAvl;
                    }
                    item.long_volume = it.CurrentQty;
                    item.long_can_close = it.QtyAvl;
                    item.long_market_value = it.MarketValue;
                    all_pos.push_back(item);
                }
            } else {
                error = "query spot position failed: not login in.";
            }

            int total_num = all_pos.size();
            int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_num;
            char buffer[length] = "";
            MemGetTradePositionMessage* rep = (MemGetTradePositionMessage*)buffer;
            memcpy(rep, req, sizeof(MemGetTradePositionMessage));
            rep->items_size = total_num;
            if (total_num) {
                MemTradePosition *first = (MemTradePosition * )((char *)buffer + sizeof(MemGetTradePositionMessage));
                for (int i = 0; i < total_num; i++) {
                    MemTradePosition *pos = first + i;
                    memcpy(pos, &all_pos[i], sizeof(MemTradePosition));
                }
            }
            if (!error.empty()) {
                strcpy(rep->error, error.c_str());
            }
            SendRtnMessage(string(buffer, length), kMemTypeQueryTradePositionRep);
        } catch (std::exception& e) {
            LOG_ERROR << "OnQueryTradePosition: " << e.what();
        }
    }

    void HtsBroker::OnQueryTradeKnock(MemGetTradeKnockMessage* req) {
        try {
            string error;
            std::vector<MemTradeKnock> all_knock;
            int64 nBrowindex = 0;
            if (login_flag_) {
                for (int index = 0; index < 5; index++) {
                    if (index == 0) {
                        if (strlen(req->next_cursor)) {
                            nBrowindex = atoi(req->next_cursor);
                        }
                    }
                    LOG_INFO << "query cursor: " << nBrowindex;

                    vector<ITPDK_SSCJ> arSscj(MAX_QUERY_NUM);
                    int64 nRet = SECITPDK_QueryMatchs(custom_id_.c_str(), 0, SORT_TYPE_AES, MAX_QUERY_NUM, nBrowindex, "", "", 0, arSscj);
                    if (nRet < 0) {
                        char _error_msg[1024];
                        SECITPDK_GetLastError(_error_msg);
                        error = "query knock failed! error:" + ConvertCharacters(_error_msg);
                        break;
                    } else if (nRet == 0) {
                        LOG_INFO << "query knock record num is zero";
                        break;
                    } else {
                        if (static_cast<int> (arSscj.size()) != nRet) {
                            LOG_ERROR << "query knock size: " << arSscj.size() << "is not equal return value, nRet: " << nRet;
                        }
                        for (auto& it : arSscj) {
//                            __info << "OrderId:" << it.OrderId
//                                << ", StockCode:" << it.StockCode
//                                << ", Market:" << it.Market
//                                << ", MatchQty:" << it.MatchQty
//                                << ", BrowIndex:" << it.BrowIndex
//                                << ", BatchNo:" << it.BatchNo
//                                << ", WithdrawFlag:" << it.WithdrawFlag
//                                << ", MatchTime:" << it.MatchTime;

                            int64_t market = Market2Std(it.Market);
                            string suffix = MarketToSuffix(market).data();
                            string code = x::Trim(it.StockCode) + suffix;
                            string order_no = CreateStandardOrderNo(market, to_string(it.OrderId));
                            string name = ConvertCharacters(x::Trim(it.StockName));
                            int64_t match_volume = abs(it.MatchQty);
                            if (IsShReverseRepurchase(code)) {
                                match_volume = match_volume * 10;
                            } else if (IsShConvertibleBond(code)) {
                                match_volume = match_volume * 10;
                            }

                            MemTradeKnock item;
                            memset(&item, 0, sizeof(item));
                            item.timestamp = GetTimeStamp(date_, it.MatchTime);
                            strcpy(item.fund_id, investor_id_.c_str());
                            item.market = market;
                            strcpy(item.code, code.c_str());
                            strcpy(item.name, name.c_str());
                            strcpy(item.order_no, order_no.c_str());
                            item.bs_flag = (BsFlag2Std(it.EntrustType));
                            if (it.WithdrawFlag[0] == 'O') {
                                if (it.MatchQty >= 0) {
                                    item.match_type = kMatchTypeOK;
                                    item.match_volume = match_volume;
                                    item.match_price = it.MatchPrice;
                                    item.match_amount = it.MatchAmt;
                                    strcpy(item.match_no, it.MatchSerialNo);
                                } else {
                                    item.match_type = kMatchTypeFailed;
                                    item.match_volume = match_volume;
                                    item.match_price = 0;
                                    item.match_amount = 0;
                                    string match_no = "_" + order_no;
                                    strcpy(item.match_no, match_no.c_str());
                                }
                            } else if (it.WithdrawFlag[0] == 'W') {
                                item.match_type = kMatchTypeWithdrawOK;
                                item.match_volume = match_volume;
                                item.match_price = 0;
                                item.match_amount = 0;
                                string match_no = "_" + order_no;
                                strcpy(item.match_no, match_no.c_str());
                            }

                            if (it.BatchNo > 0) {
                                string batch_no = CreateStandardBatchNo(market, it.BatchNo % 1000, std::to_string(it.BatchNo));
                                strncpy(item.batch_no, batch_no.c_str(), batch_no.length());
                            }
                            all_knock.push_back(item);

                            if (nBrowindex <= it.BrowIndex) {
                                nBrowindex = it.BrowIndex + 1;
                            }
                        }
                        LOG_INFO << "knock size: " << arSscj.size() << ", next_cursor: " << nBrowindex;
                        if (nRet < MAX_QUERY_NUM) {
                            LOG_INFO << "query knock record num is " << nRet;
                            break;
                        }
                    }
                }
            } else {
                error = "query spot position failed: not login in.";
            }
            int total_num = all_knock.size();
            int length = sizeof(MemGetTradeKnockMessage) + sizeof(MemTradeKnock) * total_num;
            char buffer[length] = "";
            MemGetTradeKnockMessage* rep = (MemGetTradeKnockMessage*)buffer;
            memcpy(rep, req, sizeof(MemGetTradeKnockMessage));
            rep->items_size = total_num;
            if (total_num) {
                MemTradeKnock* first = (MemTradeKnock*)((char*)buffer + sizeof(MemGetTradeKnockMessage));
                for (int i = 0; i < total_num; i++) {
                    MemTradeKnock* knock = first + i;;
                    memcpy(knock, &all_knock[i], sizeof(MemTradeKnock));
                }
            }
            if (!error.empty()) {
                strcpy(rep->error, error.c_str());
            }
            SendRtnMessage(string(buffer, length), kMemTypeQueryTradeKnockRep);
        } catch (std::exception& e) {
            LOG_ERROR << "OnQueryTradeKnock: " << e.what();
        }
    }

    void HtsBroker::OnTradeOrder(MemTradeOrderMessage* req) {
        try {
            string error_msg;
            int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
            auto items = (MemTradeOrder*)((char*)req + sizeof(MemTradeOrderMessage));
            if (login_flag_.load()) {
                int item_size = req->items_size;
                bool is_batch = item_size > 1 || strcmp(req->token, "__batch__") == 0 ? true : false;
                if (is_batch) {
                    vec_batchinfo_.clear();
                    for (int i = 0; i < item_size; i++) {
                        MemTradeOrder* order = items + i;
                        BatchOrderInfo batchorder;
                        memset(&batchorder, 0, sizeof (batchorder));

                        string std_security = string(order->code).substr(0, 6);
                        int64_t std_market = order->market;
                        int order_direct = StdBsFlag2Hts(req->bs_flag);
                        auto it = accounts_.find(std_market);
                        if (it != accounts_.end()) {
                            memcpy(batchorder.Gdh, it->second.SecuAccount, strlen(it->second.SecuAccount));
                        }
                        string std_jys = std_market == co::kMarketSH ? "SH" : "SZ";
                        strncpy(batchorder.Jys, std_jys.c_str(), std_jys.length());
                        strncpy(batchorder.Zqdm, std_security.c_str(), std_security.length());
                        batchorder.Jylb = order_direct;
                        batchorder.Wtjg = order->price;
                        batchorder.Wtsl = order->volume;
                        batchorder.Ddlx = DDLX_XJWT;
                        vec_batchinfo_.push_back(batchorder);
                    }
                    int64 nWTPCH = (++batch_start_index_) * 1000 + item_size;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        req_msg_.emplace(std::make_pair(nWTPCH, string((char*)req, length)));
                    }
                    int64 nRet = SECITPDK_BatchOrderEntrust_ASync(custom_id_.c_str(), vec_batchinfo_, nWTPCH);
                    LOG_INFO << "SECITPDK_BatchOrderEntrust, nWTPCH: " << nWTPCH << ", nRet: " << nRet;
                    if (nRet < 0) {
                        char _msg[1024];
                        SECITPDK_GetLastError(_msg);
                        error_msg = (GetErrorMsg(-5, ConvertCharacters(_msg).c_str()));
                        std::unique_lock<std::mutex> lock(mutex_);
                        req_msg_.erase(nWTPCH);
                    }
                } else {
                    if (item_size == 1) {
                        MemTradeOrder* order = items;
                        //string std_security = DropCodeSuffix(order->code).data();
                        string std_security = string(order->code).substr(0, 6);
                        int64_t std_market = order->market;
                        int order_direct = StdBsFlag2Hts(req->bs_flag);
                        //// 根据代码判断逆回购
                        int64_t std_volume = order->volume;
                        if (order_direct == JYLB_SALE) {
                            if (IsShReverseRepurchase(order->code)) {
                                order_direct = JYLB_HGRQ;
                                std_volume = order->volume / 10; // 上海逆回购的单位是手，一手=10张
                            } else if (IsSzReverseRepurchase(order->code)) {
                                order_direct = JYLB_HGRQ;
                            }
                        }
                        //// 根据代码判断可转债
                        if (std_market == kMarketSH && IsShConvertibleBond(order->code)) {
                            std_volume = order->volume / 10; // 上海可转债的单位是手，一手=10张
                        }

                        auto it = accounts_.find(std_market);
                        if (it != accounts_.end()) {
                            int64 nKFSBDBH = GetRequestID();
                            {
                                std::unique_lock<std::mutex> lock(mutex_);
                                req_msg_.emplace(std::make_pair(nKFSBDBH, string((char*)req, length)));
                            }
                            int64 nRet = 0;
                            switch (order_direct) {
                                case JYLB_ETFSG:
                                case JYLB_ETFSH:
                                    nRet = SECITPDK_ETFEntrust_ASync(custom_id_.c_str(), it->second.Market, std_security.c_str(),
                                                                     order_direct, order->volume, it->second.SecuAccount, "", nKFSBDBH);
                                    break;
                                case JYLB_HGRZ:
                                case JYLB_HGRQ:
                                    nRet = SECITPDK_ZQHGEntrust_ASync(custom_id_.c_str(), it->second.Market, std_security.c_str(),
                                                                      order_direct, std_volume, order->price, DDLX_XJWT,
                                                                      it->second.SecuAccount, nKFSBDBH);
                                    break;
                                default:
                                    nRet = SECITPDK_OrderEntrust_ASync(custom_id_.c_str(), it->second.Market, std_security.c_str(),
                                                                       order_direct, std_volume, order->price, DDLX_XJWT,
                                                                       it->second.SecuAccount, nKFSBDBH);
                            }
                            LOG_INFO << "Entrust_ASync, nKFSBDBH: " << nKFSBDBH << ", nRet: " << nRet;
                            if (nRet < 0) {
                                char _msg[1024];
                                SECITPDK_GetLastError(_msg);
                                error_msg = (GetErrorMsg(-1, ConvertCharacters(_msg).c_str()));
                                std::unique_lock<std::mutex> lock(mutex_);
                                req_msg_.erase(nKFSBDBH);
                            }
                        } else {
                            error_msg = "not find market account";
                        }
                    } else {
                        error_msg = "order item is empty";
                    }
                }
            } else {
                error_msg = "account not login in , send order is not allowed.";
            }
            if (error_msg.length() > 0) {
                int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
                char buffer[length] = "";
                memcpy(buffer, req, length);
                MemTradeOrderMessage* rep = (MemTradeOrderMessage*)buffer;
                strcpy(rep->error, error_msg.c_str());
                SendRtnMessage(string(buffer, length), kMemTypeTradeOrderRep);
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnTradeOrder: " << e.what();
        }
    }

    void HtsBroker::OnTradeWithdraw(MemTradeWithdrawMessage* req) {
        try {
            string error_msg;
            if (login_flag_.load()) {
                if (strlen(req->batch_no) > 0) {
                    vector<string> vec_info;
                    boost::split(vec_info, req->batch_no, boost::is_any_of(SPLITFLAG), boost::token_compress_on);
                    if (vec_info.size() == 3) {
                        int64 lsh = atoll(vec_info[2].c_str());
                        {
                            std::unique_lock<std::mutex> lock(mutex_);
                            req_msg_.emplace(std::make_pair(lsh, string((char *) req, sizeof(MemTradeWithdrawMessage))));
                        }
                        int64 nRet = SECITPDK_BatchOrderWithdraw_ASync(custom_id_.c_str(), lsh);
                        if (nRet < 0) {
                            char msg[1024];
                            SECITPDK_GetLastError(msg);
                            error_msg = ConvertCharacters(msg).c_str();
                            std::unique_lock<std::mutex> lock(mutex_);
                            req_msg_.erase(lsh);
                        }
                    } else {
                        error_msg = "not valid batch_no: " + string(req->batch_no);
                    }
                } else if (strlen(req->order_no) > 0) {
                    vector<string> vec_info;
                    boost::split(vec_info, req->order_no, boost::is_any_of(SPLITFLAG), boost::token_compress_on);
                    if (vec_info.size() == 2) {
                        int64 nRet = 0;
                        int64 nKFSBDBH = GetRequestID();
                        {
                            std::unique_lock<std::mutex> lock(mutex_);
                            req_msg_.emplace(std::make_pair(nKFSBDBH, string((char*)req, sizeof(MemTradeWithdrawMessage))));
                        }
                        string jys = (vec_info[0].at(0) == '1') ? "SH" : "SZ";
                        if (JudgeReverseRepurchase(req->order_no)) {
                            nRet = SECITPDK_ZQHGWithdraw_ASync(custom_id_.c_str(), jys.c_str(), atol(vec_info[1].c_str()), nKFSBDBH);
                        } else {
                            nRet = SECITPDK_OrderWithdraw_ASync(custom_id_.c_str(), jys.c_str(), atol(vec_info[1].c_str()), nKFSBDBH);
                        }
                        LOG_INFO << "Withdraw_ASync, nKFSBDBH: " << nKFSBDBH << ", nRet: " << nRet;
                        if (nRet < 0) {
                            char _msg[1024];
                            SECITPDK_GetLastError(_msg);
                            error_msg = ConvertCharacters(_msg).c_str();
                            std::unique_lock<std::mutex> lock(mutex_);
                            req_msg_.erase(nKFSBDBH);
                        }
                    } else {
                        error_msg = "not valid order_no: " + string(req->order_no);
                    }
                }
            } else {
                error_msg = "account not login in , withdraw is not allowed.";
            }
            if (error_msg.length() > 0) {
                int length = sizeof(MemTradeWithdrawMessage);
                char buffer[length] = "";
                memcpy(buffer, req, length);
                MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*)buffer;
                strcpy(rep->error, error_msg.c_str());
                SendRtnMessage(string(buffer, length), kMemTypeTradeWithdrawRep);
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnTradeWithdraw: " << e.what();
        }
    }

    void HtsBroker::SetMsgCallback(const char* pTime, const char* pMsg, int nType) {
        switch (nType) {
        case 8:
            OnRtnOrder(pTime, pMsg, nType);
            break;
        case 9:
            OnRtnCancel(pTime, pMsg, nType);
            break;
        case 10:
            OnRtnTrade(pTime, pMsg, nType);
            break;
        case 11:
            OnRtnFailedOrder(pTime, pMsg, nType);
            break;
        default:
            break;
        }
    }

    // 此函数只有nType = 14
    void HtsBroker::SetFuncCallback(const char* pTime, const char* pMsg, int nType) {
        switch (nType) {
        case 14:
            OnRspASync(pTime, pMsg, nType);
            break;
        default:
            break;
        }
    }

    // 自动重连, 不需要重新登陆, nEvent, 1是断开, 0是恢复
    void HtsBroker::SetConnEventCallback(int nEvent) {
        if (nEvent == 1) {
            login_flag_.store(false);
            LOG_ERROR << "login out.";
        } else if (nEvent == 0) {
            login_flag_.store(true);
            LOG_INFO << "re login in.";
        }
    }

    void HtsBroker::OnRspASync(const char* pTime, const char* pMsg, int nType) {
        try {
            rapidjson::Document dom;
            if (!dom.Parse(pMsg).HasParseError()) {
                string _req_message;
                flatbuffers::FlatBufferBuilder _rsp_fbb;
                int64_t _wtpch = 0;
                int64_t _kfsbdbh = 0;
                int64_t _jylx = 0;
                if (dom.HasMember("WTPCH") && dom["WTPCH"].IsInt64()) {
                    _wtpch = dom["WTPCH"].GetInt64();
                }
                if (dom.HasMember("KFSBDBH") && dom["KFSBDBH"].IsInt64()) {
                    _kfsbdbh = dom["KFSBDBH"].GetInt64();
                }
                if (dom.HasMember("JYLX") && dom["JYLX"].IsInt64()) {
                    _jylx = dom["JYLX"].GetInt64();
                }

                if (_kfsbdbh) {
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        auto it = req_msg_.find(_kfsbdbh);
                        if (it != req_msg_.end()) {
                            _req_message = it->second;
                            req_msg_.erase(it);
                        } else {
                            LOG_ERROR << "single order, not find KFSBDBH: " << _kfsbdbh;
                            return;
                        }
                    }
                    if (_req_message.empty()) {
                        LOG_ERROR << "_req_message is empty. ";
                        return;
                    }
                    int64_t wth;
                    if (dom.HasMember("WTH") && dom["WTH"].IsInt64()) {
                        wth = dom["WTH"].GetInt64();
                    }
                    int64_t ret_code = 0;
                    string ret_note;
                    if (dom.HasMember("RETCODE") && dom["RETCODE"].IsInt64()) {
                        ret_code = dom["RETCODE"].GetInt64();
                    }
                    if (dom.HasMember("RETNOTE") && dom["RETNOTE"].IsString()) {
                        ret_note = dom["RETNOTE"].GetString();
                    }
                    LOG_INFO << "OnRspASync, _wtpch: " << _wtpch << ", _kfsbdbh: " << _kfsbdbh << ", _jylx: " << _jylx;
                    if (_jylx == 30 || _jylx == 31 || _jylx == 32 || _jylx == 33) {
                        MemTradeOrderMessage* req = (MemTradeOrderMessage*)_req_message.data();
                        int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
                        char buffer[length] = "";
                        memcpy(buffer, req, length);
                        MemTradeOrderMessage* rep = (MemTradeOrderMessage*)buffer;
                        if (wth > 0) {
                            auto order = (MemTradeOrder*)((char*)rep + sizeof(MemTradeOrderMessage));
                            string order_no = CreateStandardOrderNo(order->market, std::to_string(wth));
                            strcpy(order->order_no, order_no.c_str());
                            if (IsShReverseRepurchase(order->code) || IsSzReverseRepurchase(order->code)) {
                                CollectReverseRepurchase(order_no);
                            }
                        } else {
                            string error = GetErrorMsg(ret_code, ConvertCharacters(ret_note).c_str());
                            strcpy(rep->error, error.c_str());
                        }
                        SendRtnMessage(string(buffer, length), kMemTypeTradeOrderRep);
                    } else if (_jylx == 40 || _jylx == 41 || _jylx == 42 || _jylx == 43) {
                        MemTradeWithdrawMessage* req = (MemTradeWithdrawMessage*)_req_message.data();
                        int length = sizeof(MemTradeWithdrawMessage);
                        char buffer[length] = "";
                        memcpy(buffer, req, length);
                        MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*)buffer;
                        if (wth <= 0 || ret_code <= 0) {
                            string error = GetErrorMsg(ret_code, ConvertCharacters(ret_note).c_str());
                            strcpy(rep->error, error.c_str());
                        }
                        SendRtnMessage(string(buffer, length), kMemTypeTradeWithdrawRep);
                    } else {
                        LOG_ERROR << "not valid jylx: " << _jylx;
                    }
                } else if (_wtpch > 0) {
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        auto it = req_msg_.find(_wtpch);
                        if (it != req_msg_.end()) {
                            _req_message = it->second;
                            req_msg_.erase(it);
                        } else {
                            LOG_ERROR << "batch order, not find WTPCH: " << _wtpch;
                            return;
                        }
                    }
                    if (_req_message.empty()) {
                        LOG_ERROR << "_req_message is empty. ";
                        return;
                    }

                    MemTradeOrderMessage* req = (MemTradeOrderMessage*)_req_message.data();
                    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
                    char buffer[length] = "";
                    memcpy(buffer, req, length);
                    MemTradeOrderMessage* rep = (MemTradeOrderMessage*)buffer;
                    auto items = (MemTradeOrder*)((char*)rep + sizeof(MemTradeOrderMessage));

                    int64_t std_market = 0;
                    if (dom.HasMember("orders")) {
                        const rapidjson::Value& childIter = dom["orders"];
                        for (rapidjson::Value::ConstMemberIterator itor = childIter.MemberBegin(); itor != childIter.MemberEnd(); ++itor) {
                            string num = itor->name.GetString();
                            int _index = atoi(num.c_str());
                            MemTradeOrder* order = items + (_index - 1);
                            std_market = order->market;
                            int _wth = 0;
                            string _note;
                            if (itor->value.IsObject()) {
                                _wth = itor->value.GetObject()["WTH"].GetInt();
                                _note = itor->value.GetObject()["NOTE"].GetString();
                            }
                            if (_wth > 0) {
                                string order_no = CreateStandardOrderNo(order->market, std::to_string(_wth));;
                                strcpy(order->order_no, order_no.c_str());
                            } else {
                                string error = ConvertCharacters(_note);
                                strcpy(order->error, error.c_str());
                            }
                        }
                    }
                    string batch_no = CreateStandardBatchNo(std_market, _wtpch % 1000, std::to_string(_wtpch));
                    strcpy(rep->batch_no, batch_no.c_str());
                    SendRtnMessage(string(buffer, length), kMemTypeTradeOrderRep);
                }
            }
        } catch (std::exception& e) {
            LOG_ERROR << "rec OnRspASync failed: " << e.what();
        }
    }

    void HtsBroker::OnRtnOrder(const char* pTime, const char* pMsg, int nType) {
        try {
            rapidjson::Document dom;
            if (!dom.Parse(pMsg).HasParseError()) {
                int64_t market = 0;
                string jys;
                if (dom.HasMember("JYS") && dom["JYS"].IsString()) {
                    jys = dom["JYS"].GetString();
                    market = Market2Std(const_cast<char*> (jys.c_str()));
                }

                string code;
                if (dom.HasMember("ZQDM") && dom["ZQDM"].IsString()) {
                    string SecurityID = dom["ZQDM"].GetString();
                    string suffix = string(MarketToSuffix(market).data());
                    code = x::Trim(SecurityID) + suffix;
                }

                if (IsShReverseRepurchase(code) || IsSzReverseRepurchase(code)) {
                    if (dom.HasMember("WTH") && dom["WTH"].IsInt64()) {
                        int64_t wth = dom["WTH"].GetInt64();
                        string order_no = order_no = CreateStandardOrderNo(market, to_string(wth));
                        CollectReverseRepurchase(order_no);
                    }
                }
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnRtnOrder: " << e.what();
        }
    }

    void HtsBroker::OnRtnFailedOrder(const char* pTime, const char* pMsg, int nType) {
        try {
            rapidjson::Document dom;
            flatbuffers::FlatBufferBuilder _push_fbb;
            if (!dom.Parse(pMsg).HasParseError()) {

                int64_t timestamp = 0;
                if (dom.HasMember("CJSJ") && dom["CJSJ"].IsString()) {
                    string strTemp = dom["CJSJ"].GetString();
                    x::Trim(strTemp);
                    timestamp = GetTimeStamp(date_, const_cast<char*> (strTemp.c_str()));
                }


                int64_t market = 0;
                string jys;
                if (dom.HasMember("JYS") && dom["JYS"].IsString()) {
                    jys = dom["JYS"].GetString();
                    market = Market2Std(const_cast<char*> (jys.c_str()));
                }

                string code;
                if (dom.HasMember("ZQDM") && dom["ZQDM"].IsString()) {
                    string SecurityID = dom["ZQDM"].GetString();
                    string suffix = string(MarketToSuffix(market).data());
                    code = x::Trim(SecurityID) + suffix;
                }

                string order_no;
                string match_no;
                if (dom.HasMember("WTH") && dom["WTH"].IsInt64()) {
                    int64_t wth = dom["WTH"].GetInt64();
                    order_no = CreateStandardOrderNo(market, to_string(wth));
                    match_no = ("_" + order_no);
                }

                string error;
                if (dom.HasMember("JGSM") && dom["JGSM"].IsString()) {
                    error = ConvertCharacters(dom["JGSM"].GetString());
                    LOG_ERROR << code << ", order_no: " << order_no << ", error: " << error;
                }

                int64_t bs_flag = 0;
                if (dom.HasMember("JYLB") && dom["JYLB"].IsInt64()) {
                    int64_t direct = dom["JYLB"].GetInt64();
                    bs_flag = BsFlag2Std(direct);
                }

                int64_t match_volume = 0;
                if (dom.HasMember("WTSL") && dom["WTSL"].IsInt64()) {
                    match_volume = dom["WTSL"].GetInt64();
                }

                string batch_no;
                if (dom.HasMember("LSH") && dom["LSH"].IsInt64()) {
                    int64_t lsh = dom["LSH"].GetInt64();
                    if (lsh > 0) {
                        batch_no = CreateStandardBatchNo(market, lsh % 1000, std::to_string(lsh));
                    }
                }
                int length = sizeof(MemTradeKnock);
                char buffer[length] = "";
                MemTradeKnock* knock = (MemTradeKnock*) buffer;
                knock->timestamp = timestamp;
                strcpy(knock->fund_id, investor_id_.c_str());
                strcpy(knock->code, code.c_str());
                strcpy(knock->order_no, order_no.c_str());
                strcpy(knock->match_no, match_no.c_str());
                knock->bs_flag = bs_flag;
                knock->match_volume = match_volume;
                knock->match_type = co::kMatchTypeFailed;
                knock->match_price = 0;
                knock->match_amount = 0;
                strcpy(knock->batch_no, batch_no.c_str());
                strcpy(knock->error, error.c_str());
                SendRtnMessage(string(buffer, length), kMemTypeTradeKnock);
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnRspQueryPositions: " << e.what();
        }
    }

    void HtsBroker::OnRtnTrade(const char* pTime, const char* pMsg, int nType) {
        try {
            rapidjson::Document dom;
            if (!dom.Parse(pMsg).HasParseError()) {
                int64_t timestamp = 0;
                if (dom.HasMember("CJSJ") && dom["CJSJ"].IsString()) {
                    string strTemp = dom["CJSJ"].GetString();
                    x::Trim(strTemp);
                    timestamp = (GetTimeStamp(date_, const_cast<char*> (strTemp.c_str())));
                }

                string match_no;
                if (dom.HasMember("CJBH") && dom["CJBH"].IsString()) {
                    string strTemp = dom["CJBH"].GetString();
                    match_no = x::Trim(strTemp);
                }

                int64_t market = 0;
                string jys;
                if (dom.HasMember("JYS") && dom["JYS"].IsString()) {
                    jys = dom["JYS"].GetString();
                    market = Market2Std(const_cast<char*> (jys.c_str()));
                }

                string code;
                if (dom.HasMember("ZQDM") && dom["ZQDM"].IsString()) {
                    string SecurityID = dom["ZQDM"].GetString();
                    string suffix = string(MarketToSuffix(market).data());
                    code = x::Trim(SecurityID) + suffix;
                }

                string order_no;
                if (dom.HasMember("WTH") && dom["WTH"].IsInt64()) {
                    int64_t wth = dom["WTH"].GetInt64();
                    order_no = CreateStandardOrderNo(market, to_string(wth));
                }

                int64_t bs_flag = 0;
                if (dom.HasMember("JYLB") && dom["JYLB"].IsInt64()) {
                    int64_t direct = dom["JYLB"].GetInt64();
                    bs_flag = BsFlag2Std(direct);
                }

                int64_t match_volume = 0;
                if (dom.HasMember("CJSL") && dom["CJSL"].IsInt64()) {
                    match_volume = dom["CJSL"].GetInt64();
                }

                double match_price = 0;
                if (dom.HasMember("CJJG") && dom["CJJG"].IsDouble()) {
                    match_price = dom["CJJG"].GetDouble();

                }

                double match_amount = 0;
                if (dom.HasMember("CJJE") && dom["CJJE"].IsDouble()) {
                    match_amount = dom["CJJE"].GetDouble();
                }

                string batch_no;
                if (dom.HasMember("LSH") && dom["LSH"].IsInt64()) {
                    int64_t lsh = dom["LSH"].GetInt64();
                    if (lsh > 0) {
                        batch_no = CreateStandardBatchNo(market, lsh % 1000, std::to_string(lsh));
                    }
                }

                int length = sizeof(MemTradeKnock);
                char buffer[length] = "";
                MemTradeKnock* knock = (MemTradeKnock*) buffer;
                knock->timestamp = timestamp;
                strcpy(knock->fund_id, investor_id_.c_str());
                strcpy(knock->code, code.c_str());
                strcpy(knock->order_no, order_no.c_str());
                strcpy(knock->match_no, match_no.c_str());
                knock->bs_flag = bs_flag;
                knock->match_volume = match_volume;
                knock->match_type = co::kMatchTypeOK;
                knock->match_price = match_price;
                knock->match_amount = match_amount;
                strcpy(knock->batch_no, batch_no.c_str());
                // LOG_INFO << "成交推送, " << ToString(knock);
                SendRtnMessage(string(buffer, length), kMemTypeTradeKnock);
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnRtnTrade: " << e.what();
        }
    }

    void HtsBroker::OnRtnCancel(const char* pTime, const char* pMsg, int nType) {        
        try {
            flatbuffers::FlatBufferBuilder _push_fbb;
            rapidjson::Document dom;
            if (!dom.Parse(pMsg).HasParseError()) {
                if (dom.HasMember("SBJG") && dom["SBJG"].IsInt()) {
                    int64_t sbjg = dom["SBJG"].GetInt();
                    if (sbjg == 9) {
                        LOG_INFO << "withdraw failed, sbjg is 9.";
                        return;
                    }
                }

                int64_t timestamp = 0;
                if (dom.HasMember("CJSJ") && dom["CJSJ"].IsString()) {
                    string strTemp = dom["CJSJ"].GetString();
                    x::Trim(strTemp);
                    timestamp = GetTimeStamp(date_, const_cast<char*> (strTemp.c_str()));
                }

                int64_t market = 0;
                string jys;
                if (dom.HasMember("JYS") && dom["JYS"].IsString()) {
                    jys = dom["JYS"].GetString();
                    market = Market2Std(const_cast<char*> (jys.c_str()));
                }

                string code;
                if (dom.HasMember("ZQDM") && dom["ZQDM"].IsString()) {
                    string SecurityID = dom["ZQDM"].GetString();
                    string suffix = string(MarketToSuffix(market).data());
                    code = x::Trim(SecurityID) + suffix;
                }

                string order_no;
                string match_no;
                if (dom.HasMember("CXWTH") && dom["CXWTH"].IsInt64()) {
                    int64_t wth = dom["CXWTH"].GetInt64();
                    order_no = CreateStandardOrderNo(market, to_string(wth));
                    match_no = "_" + order_no;
                }

                int64_t bs_flag = 0;
                if (dom.HasMember("JYLB") && dom["JYLB"].IsInt64()) {
                    int64_t direct = dom["JYLB"].GetInt64();
                    bs_flag = BsFlag2Std(direct);
                }

                int64_t match_volume = 0;
                if (dom.HasMember("CDSL") && dom["CDSL"].IsInt64()) {
                    match_volume = dom["CDSL"].GetInt64();
                }

                string batch_no;
                if (dom.HasMember("LSH") && dom["LSH"].IsInt64()) {
                    int64_t lsh = dom["LSH"].GetInt64();
                    if (lsh > 0) {
                        batch_no = CreateStandardBatchNo(market, lsh % 1000, std::to_string(lsh));
                    }
                }
                int length = sizeof(MemTradeKnock);
                char buffer[length] = "";
                MemTradeKnock* knock = (MemTradeKnock*) buffer;
                knock->timestamp = timestamp;
                strcpy(knock->fund_id, investor_id_.c_str());
                strcpy(knock->code, code.c_str());
                strcpy(knock->order_no, order_no.c_str());
                strcpy(knock->match_no, match_no.c_str());
                knock->bs_flag = bs_flag;
                knock->match_volume = match_volume;
                knock->match_type = co::kMatchTypeWithdrawOK;
                knock->match_price = 0;
                knock->match_amount = 0;
                strcpy(knock->batch_no, batch_no.c_str());
                SendRtnMessage(string(buffer, length), kMemTypeTradeKnock);
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnRspQueryPositions: " << e.what();
        }
    }

    void HtsBroker::InputReverseRepurchase() {
        sz_reverse_repurchase_.insert("131800.SZ");
        sz_reverse_repurchase_.insert("131801.SZ");
        sz_reverse_repurchase_.insert("131809.SZ");
        sz_reverse_repurchase_.insert("131810.SZ");
        sz_reverse_repurchase_.insert("131811.SZ");

        sh_reverse_repurchase_.insert("204001.SH");
        sh_reverse_repurchase_.insert("204002.SH");
        sh_reverse_repurchase_.insert("204003.SH");
        sh_reverse_repurchase_.insert("204004.SH");
        sh_reverse_repurchase_.insert("204007.SH");
    }

    bool HtsBroker::IsShReverseRepurchase(const string& code) {
        bool flag = false;
        auto it = sh_reverse_repurchase_.find(code);
        if (it != sh_reverse_repurchase_.end()) {
            flag = true;
        }
        return flag;
    }

    bool HtsBroker::IsSzReverseRepurchase(const string& code) {
        bool flag = false;
        auto it = sz_reverse_repurchase_.find(code);
        if (it != sz_reverse_repurchase_.end()) {
            flag = true;
        }
        return flag;
    }

    void HtsBroker::CollectReverseRepurchase(const string& order_no) {
        auto it = reverse_repurchase_orders_.find(order_no);
        if (it == reverse_repurchase_orders_.end()) {
            reverse_repurchase_orders_.insert(order_no);
            LOG_INFO << "add reverse repurchase order_no: " << order_no;
        }
    }

    // 债券同理，沪市11， 深市12开头
    bool HtsBroker::IsShConvertibleBond(const string& code) {
        if (code.length() < 3) {
            return false;
        }
        bool flag = false;
        int prefix = atol(code.substr(0, 2).c_str());
        if (prefix == 11) {
            flag = true;
        }
        return flag;
    }

    bool HtsBroker::JudgeReverseRepurchase(const string& order_no) {
        bool flag = false;
        auto it = reverse_repurchase_orders_.find(order_no);
        if (it != reverse_repurchase_orders_.end()) {
            flag = true;
        }
        return flag;
    }

    int64 HtsBroker::GetRequestID() {
        return ++start_index_;
    }
}  // namespace co
