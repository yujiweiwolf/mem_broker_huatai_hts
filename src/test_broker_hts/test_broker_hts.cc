//#include <iostream>
//#include <stdexcept>
//#include <sstream>
//#include <string>
//#include <boost/program_options.hpp>
#include "x/x.h"
#include "coral/coral.h"
#include "mem_broker/mem_struct.h"
#include <regex>
#include <thread>
#include "../libbroker_hts/hts_broker.h"

using namespace std;
using namespace co;

#define NUM_ORDER 1

string fund_id;
string mem_dir;
string mem_req_file;
string mem_rep_file;

void write_order(shared_ptr<MemBroker> broker) {
    int total_order_num = 1;
    string id = x::UUID();
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->bs_flag = kBsFlagBuy;
    msg->items_size = total_order_num;
    MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
    for (int i = 0; i < total_order_num; i++) {
        MemTradeOrder* order = item + i;
        order->volume = 100;
        order->market = co::kMarketSZ;
        order->price = 10.01 + 0.01 * i;
        order->price_type = kQOrderTypeLimit;
        sprintf(order->code, "00000%d.SZ", i + 1);
        LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
    }
    msg->timestamp = x::RawDateTime();
    broker->SendTradeOrder(msg);
}

void withdraw(shared_ptr<MemBroker> broker) {
    string id = x::UUID();
    MemTradeWithdrawMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id.c_str());
    cout << "please input order_no" << endl;
    cin >> msg.order_no;
    LOG_INFO << "send withdraw, fund_id: " << msg.fund_id << ", order_no: " << msg.order_no;
    msg.timestamp = x::RawDateTime();
    broker->SendTradeWithdraw(&msg);
}

// BUY 600000.SH 100 9.9
// BUY 159920.SZ 1000 1.119
// SELL 159920.SZ 500 1.100
// BUY 510900.SH 1000 0.800
// SELL 510900.SH 100 0.767
void order(shared_ptr<MemBroker> broker) {
    int total_order_num = 1;
    string id = x::UUID();
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->bs_flag = kBsFlagBuy;
    msg->items_size = total_order_num;
    MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
    for (int i = 0; i < total_order_num; i++) {
        MemTradeOrder* order = item + i;
        getchar();
        cout << "please input : BUY 600000.SH 100 9.9\n";
        std::string input;
        getline(std::cin, input);
        std::cout << "your input is: # " << input << " #" << std::endl;
        std::smatch result;
        if (regex_match(input, result, std::regex("^(BUY|SELL|CREATE|REDEEM) ([0-9]{1,10})\.(SH|SZ) ([0-9]{1,10}) ([.0-9]{1,10})$")))
        {
            string command = result[1].str();
            string instrument = result[2].str();
            string market = result[3].str();
            string volume = result[4].str();
            string price = result[5].str();
            if (command == "BUY") {
                msg->bs_flag = kBsFlagBuy;
            } else if (command == "SELL") {
                msg->bs_flag = kBsFlagSell;
            }

            string code;
            if (market == "SH") {
                code = instrument + ".SH";
            } else if (market == "SZ") {
                code = instrument + ".SZ";
            }
            strcpy(order->code, code.c_str());
            order->volume = atoll(volume.c_str());
            order->price = atof(price.c_str());
        }
        LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
    }
    msg->timestamp = x::RawDateTime();
    broker->SendTradeOrder(msg);
}

void feeder_order(std::shared_ptr<MemBroker> broker) {
    string dir = "/home/work/sys/develop/feeder_huatai_insight/data/feed.stock.mem";
    LOG_INFO << "mem_feeder_dir: " << dir;
    if (dir.empty()) {
        return;
    }
    const void* data = nullptr;
    x::MMapReader feeder_reader_;
    feeder_reader_.Open(dir, "meta");
    feeder_reader_.Open(dir, "data");
    int64_t last_order_time = x::RawDateTime();
    std::set<string> all_code;
    std::map<string, MemQContract> all_contract;
    while (true) {
        int32_t type = feeder_reader_.Next(&data);
        if (type == kMemTypeQTick) {
            x::Sleep(10);
            MemQTick *tick = (MemQTick *) data;
            // LOG_INFO << "收到, " << ToString(tick);
            string code = tick->code;
            auto it = all_code.find(code);
            if (it == all_code.end()) {
                auto itor = all_contract.find(code);
                if (itor != all_contract.end()) {
                    if (itor->second.dtype == kDTypeStock) {
                        char first = code.at(0);
                        if (first == '0' || first == '3' || first == '6') {
                                 LOG_INFO << "code: " << code << ", timestamp: " << tick->timestamp << ", order index: "
                                          << all_code.size();
                                all_code.insert(code);
                            /////////////////////////////////////////////////
                            double order_price = 0;
                            for (int i = 0; i < 10; i++) {
                                if (tick->ap[i] > order_price) {
                                    order_price = tick->ap[i];
                                }
                            }
                            int total_order_num = 1;
                            string id = x::UUID();
                            int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
                            char buffer[length] = "";
                            MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
                            strncpy(msg->id, id.c_str(), id.length());
                            strcpy(msg->fund_id, fund_id.c_str());
                            msg->bs_flag = kBsFlagBuy;
                            msg->items_size = total_order_num;
                            MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
                            for (int i = 0; i < total_order_num; i++) {
                                MemTradeOrder* order = item + i;
                                order->volume = 100;
                                order->market = itor->second.market;
                                order->price = order_price;
                                order->price_type = kQOrderTypeLimit;
                                sprintf(order->code, "00000%d.SZ", i + 1);
                                // LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
                            }
                            msg->timestamp = x::RawDateTime();
                            broker->SendTradeOrder(msg);
                        }
                    }
                }
            }
        } else if (type == kMemTypeQContract){
            MemQContract *contract = (MemQContract *) data;
            all_contract.insert(std::make_pair(contract->code, *contract));
        }
    }
}

void query_asset(shared_ptr<MemBroker> broker) {
    string id = x::UUID();
    MemGetTradeAssetMessage msg {};
    // memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id.c_str());
    msg.timestamp = x::RawDateTime();
    broker->SendQueryTradeAsset(&msg);
}

void query_position(shared_ptr<MemBroker> broker) {
    string id = x::UUID();
    MemGetTradePositionMessage msg {};
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id.c_str());
    msg.timestamp = x::RawDateTime();
    broker->SendQueryTradePosition(&msg);
}

void query_knock(shared_ptr<MemBroker> broker) {
    string id = x::UUID();
    MemGetTradeKnockMessage msg {};
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id.c_str());
    string cursor;
    cout << "please input cursor" << endl;
    cin >> cursor;
    strcpy(msg.cursor, cursor.c_str());
    msg.timestamp = x::RawDateTime();
    broker->SendQueryTradeKnock(&msg);
}

void order_create(shared_ptr<MemBroker> broker) {
    int total_order_num = 1;
    string id = x::UUID();
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->bs_flag = kBsFlagCreate;
    msg->items_size = total_order_num;
    MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
    for (int i = 0; i < total_order_num; i++) {
        MemTradeOrder* order = item + i;
        order->volume = 400000;
        order->price = 2.01;
        order->price_type = kQOrderTypeLimit;
        order->market =  co::kMarketSH;
        strcpy(order->code, "510500.SH");
        LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
    }
    msg->timestamp = x::RawDateTime();
    broker->SendTradeOrder(msg);
}

void order_redeem(shared_ptr<MemBroker> broker) {
    int total_order_num = 1;
    string id = x::UUID();
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->bs_flag = kBsFlagRedeem;
    msg->items_size = total_order_num;
    MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
    for (int i = 0; i < total_order_num; i++) {
        MemTradeOrder* order = item + i;
        order->volume = 400000;
        order->price = 2.01;
        order->price_type = kQOrderTypeLimit;
        order->market =  co::kMarketSH;
        strcpy(order->code, "510500.SH");
        LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
    }
    msg->timestamp = x::RawDateTime();
    broker->SendTradeOrder(msg);
}

void batch_order(shared_ptr<MemBroker> broker) {
    int total_order_num = 3;
    string id = x::UUID();
    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * total_order_num;
    char buffer[length] = "";
    MemTradeOrderMessage* msg = (MemTradeOrderMessage*) buffer;
    strncpy(msg->id, id.c_str(), id.length());
    strcpy(msg->fund_id, fund_id.c_str());
    msg->bs_flag = kBsFlagBuy;
    msg->items_size = total_order_num;
    MemTradeOrder* item = (MemTradeOrder*)((char*)buffer + sizeof(MemTradeOrderMessage));
    {
        MemTradeOrder* order = item + 0;
        order->volume = 3000;
        order->price = 6.01;
        order->price_type = kQOrderTypeLimit;
        order->market =  co::kMarketSH;
        strcpy(order->code, "600000.SH");
        LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
    }
    {
        MemTradeOrder* order = item + 1;
        order->volume = 4000;
        order->price = 12.01;
        order->price_type = kQOrderTypeLimit;
        order->market =  co::kMarketSZ;
        strcpy(order->code, "000001.SZ");
        LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
    }
    {
        MemTradeOrder* order = item + 2;
        order->volume = 5000;
        order->price = 21.01;
        order->price_type = kQOrderTypeLimit;
        order->market =  co::kMarketSH;
        strcpy(order->code, "600030.SH");
        LOG_INFO << "send order, code: " << order->code << ", volume: " << order->volume << ", price: " << order->price;
    }
    msg->timestamp = x::RawDateTime();
    broker->SendTradeOrder(msg);
}

void batch_withdraw(shared_ptr<MemBroker> broker) {
    string id = x::UUID();
    MemTradeWithdrawMessage msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.id, id.c_str(), id.length());
    strcpy(msg.fund_id, fund_id.c_str());
    cout << "please input batch_no" << endl;
    cin >> msg.batch_no;
    LOG_INFO << "send withdraw, fund_id: " << msg.fund_id << ", batch_no: " << msg.batch_no;
    msg.timestamp = x::RawDateTime();
    broker->SendTradeWithdraw(&msg);
}

void ReadRep() {
    const void* data = nullptr;
    x::MMapReader common_reader;
    common_reader.Open(mem_dir, mem_rep_file, true);
    while (true) {
        x::Sleep(1000);
        int32_t type = common_reader.Next(&data);
        // LOG_INFO << "type: " << type;
        if (type == kMemTypeTradeOrderRep) {
            MemTradeOrderMessage* rep = (MemTradeOrderMessage*)data;
            LOG_INFO << "收到报单响应, " << ToString(rep);
        } else if (type == kMemTypeTradeWithdrawRep) {
            MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*) data;
            LOG_INFO << "收到撤单响应, " << ToString(rep);
        } else if (type == kMemTypeTradeKnock) {
            MemTradeKnock* msg = (MemTradeKnock*) data;
            LOG_INFO << "成交推送, " << ToString(msg);
        } else if (type == kMemTypeTradeAsset) {
            MemTradeAsset *asset = (MemTradeAsset*) data;
            LOG_INFO << "资金变更, fund_id: " << asset->fund_id
                     << ", timestamp: " << asset->timestamp
                     << ", balance: " << asset->balance
                     << ", usable: " << asset->usable
                     << ", margin: " << asset->margin
                     << ", equity: " << asset->equity
                     << ", long_margin_usable: " << asset->long_margin_usable
                     << ", short_margin_usable: " << asset->short_margin_usable
                     << ", short_return_usable: " << asset->short_return_usable;
        } else if (type == kMemTypeTradePosition) {
            MemTradePosition* position = (MemTradePosition*) data;
            LOG_INFO << "持仓变更, code: " << position->code
                     << ", fund_id: " << position->fund_id
                     << ", timestamp: " << position->timestamp
                     << ", long_volume: " << position->long_volume
                     << ", long_market_value: " << position->long_market_value
                     << ", long_can_close: " << position->long_can_close
                     << ", short_volume: " << position->short_volume
                     << ", short_market_value: " << position->short_market_value
                     << ", short_can_open: " << position->short_can_open;
        } else if (type == kMemTypeMonitorRisk) {
            MemMonitorRiskMessage* msg = (MemMonitorRiskMessage*) data;
            LOG_ERROR << "Risk, " << msg->error << ", timestamp: " << msg->timestamp;
        } else if (type == kMemTypeHeartBeat) {
            HeartBeatMessage* msg = (HeartBeatMessage*) data;
            LOG_ERROR << "心跳, " << msg->fund_id << ", timestamp: " << msg->timestamp;
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        Singleton<Config>::Instance();
        Singleton<Config>::GetInstance()->Init();
        MemBrokerOptionsPtr options = Singleton<Config>::GetInstance()->options();
        std::shared_ptr<HtsBroker> broker = std::make_shared<HtsBroker>();
        MemBrokerServer server;
        server.Init(options, broker);
        server.Start();

        fund_id = Singleton<Config>::GetInstance()->account();
        mem_dir = options->mem_dir();
        mem_req_file = options->mem_req_file();
        mem_rep_file = options->mem_rep_file();
        std::thread t1(ReadRep);

        string usage("\nTYPE  'q' to quit program\n");
        usage += "      '1' to order_sh\n";
        usage += "      '2' to order_sz\n";
        usage += "      '3' to withdraw\n";
        usage += "      '4' to order\n";
        usage += "      '5' to query asset\n";
        usage += "      '6' to query position\n";
        usage += "      '7' to query order\n";
        usage += "      '8' to query knock\n";
        usage += "      '9' to 行情报单\n";
        usage += "      'a' to kBsFlag Create\n";
        usage += "      'b' to kBsFlag Redeem\n";
        usage += "      'c' to BatchOrder\n";
        usage += "      'd' to withdraw BatchOrder\n";
        cerr << (usage);

        char c;
        while ((c = getchar()) != 'q') {
            switch (c) {
                case '1':
                {
                    write_order(broker);
                    break;
                }
                case '2':
                {
                    break;
                }
                case '3':
                {
                    withdraw(broker);
                    break;
                }
                case '4':
                {
                    order(broker);
                    break;
                }
                case '5':
                {
                    query_asset(broker);
                    break;
                }
                case '6':
                {
                    query_position(broker);
                    break;
                }
                case '7':
                {
                    break;
                }
                case '8':
                {
                    query_knock(broker);
                    break;
                }
                case '9':
                {
                    feeder_order(broker);
                    break;
                }
                case 'a':
                {
                    order_create(broker);
                    break;
                }

                case 'b':
                {
                    order_redeem(broker);
                    break;
                }
                case 'c':
                {
                    batch_order(broker);
                    break;
                }
                case 'd':
                {
                    batch_withdraw(broker);
                    break;
                }
                default:
                    break;
            }
        }
    } catch (std::exception& e) {
        LOG_FATAL << "server is crashed, " << e.what();
        throw e;
    }
    return 0;
}
