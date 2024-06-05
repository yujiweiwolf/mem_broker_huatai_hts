// Copyright 2020 Fancapital Inc.  All rights reserved.
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <boost/program_options.hpp>
#include "../libbroker_hts/config.h"
#include "../libbroker_hts/hts_broker.h"
#include "../libbroker_hts/singleton.h"

using namespace std;
using namespace co;
namespace po = boost::program_options;

const string kVersion = "v1.0.3";

int main(int argc, char* argv[]) {
    try {
        po::options_description desc("[hts_broker] Usage");
        desc.add_options()
            ("help,h", "show help message")
            ("version,v", "show version information")
            ("passwd", po::value<std::string>(), "encode plain password");
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        if (vm.count("passwd")) {
            cout << co::EncodePassword(vm["passwd"].as<std::string>()) << endl;
            return 0;
        } else if (vm.count("help")) {
            cout << desc << endl;
            return 0;
        } else if (vm.count("version")) {
            cout << kVersion << endl;
            return 0;
        }
        LOG_INFO << "kVersion: " << kVersion;
        Singleton<Config>::Instance();
        Singleton<Config>::GetInstance()->Init();
        MemBrokerOptionsPtr options = Singleton<Config>::GetInstance()->options();
        MemBrokerServer server;
        shared_ptr<MemBroker> broker = make_shared<HtsBroker>();
        server.Init(options, broker);
        server.Run();
        while (true) {
            x::Sleep(1000);
        }
        LOG_INFO << "server is stopped.";
    }
    catch (x::Exception& e) {
        LOG_ERROR << "server is crashed, " << e.what();
        return 1;
    }
    catch (std::exception& e) {
        LOG_ERROR << "server is crashed, " << e.what();
        return 2;
    }
    catch (...) {
        LOG_ERROR << "server is crashed, unknown reason";
        return 3;
    }
    return 0;
}
