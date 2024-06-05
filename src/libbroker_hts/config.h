// Copyright 2020 Fancapital Inc.  All rights reserved.
#pragma once
#include <string>
#include "mem_broker/mem_base_broker.h"

using namespace std;

namespace co {
    class Config {
    public:
        inline string system_name() {
            return system_name_;
        }
        inline string itpdk_path() {
            return itpdk_path_;
        }
        inline string account() {
            return account_;
        }
        inline string password() {
            return password_;
        }
        inline string custom() {
            return custom_;
        }
        inline string wtfs() {
            return wtfs_;
        }
        inline string node() {
            return node_;
        }

        inline MemBrokerOptionsPtr options() {
            return options_;
        }

        void Init();

    private:
        MemBrokerOptionsPtr options_;
        string system_name_;
        string itpdk_path_;
        string account_;
        string custom_;
        string password_;
        string wtfs_;
        string node_;
    };
}  // namespace co
