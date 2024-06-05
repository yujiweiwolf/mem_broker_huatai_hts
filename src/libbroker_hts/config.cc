// Copyright 2020 Fancapital Inc.  All rights reserved.
#include "./config.h"
#include <coral/coral.h>
#include <boost/filesystem.hpp>
#include "yaml-cpp/yaml.h"

namespace co {
    void Config::Init() {
        auto getStr = [&](const YAML::Node& node, const std::string& name) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<std::string>() : "";
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto getStrings = [&](std::vector<std::string>* ret, const YAML::Node& node, const std::string& name, bool drop_empty = false) {
            try {
                if (node[name] && !node[name].IsNull()) {
                    for (auto item : node[name]) {
                        std::string s = x::Trim(item.as<std::string>());
                        if (!drop_empty || !s.empty()) {
                            ret->emplace_back(s);
                        }
                    }
                }
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto getInt = [&](const YAML::Node& node, const std::string& name, const int64_t& default_value = 0) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<int64_t>() : default_value;
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto getBool = [&](const YAML::Node& node, const std::string& name) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<bool>() : false;
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };

        auto filename = x::FindFile("broker.yaml");
        YAML::Node root = YAML::LoadFile(filename);
        options_ = MemBrokerOptions::Load(filename);

        auto broker = root["hts"];
        system_name_ = getStr(broker, "system_name");
        boost::filesystem::path _p(filename);
        itpdk_path_ = _p.parent_path().string() + "/";
        account_ = getStr(broker, "account");
        custom_ = getStr(broker, "custom");
        string __password_ = getStr(broker, "password");
        password_ = DecodePassword(__password_);
        wtfs_ = getStr(broker, "wtfs");
        node_ = getStr(broker, "node");

        stringstream ss;
        ss << "+-------------------- configuration begin --------------------+" << endl;
        ss << options_->ToString() << endl;
        ss << endl;
        ss << "hts:" << endl
            << "  system_name: " << system_name_ << endl
            << "  itpdk_path: " << itpdk_path_ << endl
            << "  account: " << account_ << endl
            << "  custom: " << custom_ << endl
            << "  password: " << string(password_.length(), '*') << endl
            << "  wtfs: " << wtfs_ << endl
            << "  node: " << node_ << endl;

        ss << "+-------------------- configuration end   --------------------+";
        LOG_INFO << endl << ss.str();
    }
}  // namespace co
