// Copyright 2020 Fancapital Inc.  All rights reserved.
#include <boost/program_options.hpp>
#include "../libbroker_hts/config.h"
#include "../libbroker_hts/hts_broker.h"
#include "../libbroker_hts/singleton.h"

using namespace std;
using namespace co;
namespace po = boost::program_options;

const string kVersion = "v2.0.1";

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
        const std::vector<std::shared_ptr<RiskOptions>>& risk_opts = Singleton<Config>::GetInstance()->risk_opt();
        MemBrokerServer server;
        shared_ptr<MemBroker> broker = make_shared<HtsBroker>();
        server.Init(options, risk_opts, broker);
        server.Run();
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
