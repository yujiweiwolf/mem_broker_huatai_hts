// Copyright 2020 Fancapital Inc.  All rights reserved.
#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <boost/algorithm/string.hpp>
#include "rapidjson/rapidjson.h"
#include "secitpdk/secitpdk.h"
#include "rapidjson/document.h"
#include "singleton.h"
#include "config.h"

#include "mem_broker/mem_base_broker.h"
#include "mem_broker/mem_server.h"


namespace co {
class HtsBroker : public MemBroker {
 public:
    HtsBroker();

    virtual ~HtsBroker();

    void Start();

    void OnInit();

    void ReqLogin();

    void ReqQueryAccount();

    void OnQueryTradeAsset(MemGetTradeAssetMessage* req);

    void OnQueryTradePosition(MemGetTradePositionMessage* req);

    void OnQueryTradeKnock(MemGetTradeKnockMessage* req);

    void OnTradeOrder(MemTradeOrderMessage* req);

    void OnTradeWithdraw(MemTradeWithdrawMessage* req);

    void SetMsgCallback(const char* pTime, const char* pMsg, int nType);
    void SetFuncCallback(const char* pTime, const char* pMsg, int nType);
    void SetConnEventCallback(int nEvent);

 private:
    void PrepareQuery();
    void UpdataMaxOrderParam();
    void InputReverseRepurchase();
    bool IsShReverseRepurchase(const string& code);
    bool IsSzReverseRepurchase(const string& code);
    bool JudgeReverseRepurchase(const string& order_no);
    bool IsShConvertibleBond(const string& code);
    void CollectReverseRepurchase(const string& order_no);
    void OnRspASync(const char* pTime, const char* pMsg, int nType);
    void OnRtnOrder(const char* pTime, const char* pMsg, int nType);
    void OnRtnFaildOrder(const char* pTime, const char* pMsg, int nType);
    void OnRtnTrade(const char* pTime, const char* pMsg, int nType);
    void OnRtnCancel(const char* pTime, const char* pMsg, int nType);
    int64 GetRequestID();
    
 private:
    int64_t date_ = 0;
    std::atomic<bool> login_flag_;
    string custom_id_;
    string investor_id_;
    string key_name_;
    string pwd_;
    std::shared_ptr<std::thread> thread_;
    int64_t pre_query_timestamp_ = 0;  // 上次查询的时间戳，用于进行流控控制

    std::set<std::string> set_reverse_repurchase;

    unordered_map<int64_t, ITPDK_GDH> accouts_;
    unordered_map<string, string> names_;
    vector<BatchOrderInfo> vec_batchinfo_;
    set<string> sh_reverse_repurchase_;
    set<string> sz_reverse_repurchase_;
    set<string> reverse_repurchase_orders_;

    int64 start_index_ = 0;
    std::mutex mutex_;
    flatbuffers::FlatBufferBuilder req_fbb_;
    // 批量报撤使用的是同一个是RequestID
    std::unordered_map<int64, std::string> req_msg_;  // key 是RequestID
};

class MidBridge {
 public:
    static co::HtsBroker* broker_;

    static void SetMsgCallback(const char* pTime, const char* pMsg, int nType) {
        LOG_INFO << "SECITPDK_SetMsgCallback,  pTime:" << pTime << ", nType:" << nType << "   " << pMsg;
        broker_->SetMsgCallback(pTime, pMsg, nType);
    }

    static void SetFuncCallback(const char* pTime, const char* pMsg, int nType) {
        LOG_INFO << "SECITPDK_SetFuncCallback,  pTime:" << pTime << ", nType:" << nType << "   " << pMsg;
        broker_->SetFuncCallback(pTime, pMsg, nType);
    }

    static void SetConnEventCallback(const char* pKhh, const char* pConnKey, int nEvent, int nType) {
        LOG_ERROR << "OnFrontDisconnected, " << "pKhh: " << pKhh << ", pConnKey:" << pConnKey
            << ", nEvent:" << nEvent << ", nType:" << nType;
        broker_->SetConnEventCallback(nEvent);
    }
};
}  // namespace co

