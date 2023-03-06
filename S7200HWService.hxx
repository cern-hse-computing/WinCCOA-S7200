/** © Copyright 2022 CERN
 *
 * This software is distributed under the terms of the
 * GNU Lesser General Public Licence version 3 (LGPL Version 3),
 * copied verbatim in the file “LICENSE”
 *
 * In applying this licence, CERN does not waive the privileges
 * and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 *
 * Author: Adrien Ledeul (HSE)
 *
 **/


#ifndef S7200HWSERVICE_H_
#define S7200HWSERVICE_H_

#include <HWService.hxx>
#include <memory>
#include "S7200LibFacade.hxx"

#include "Common/Logger.hxx"
#include <queue>
#include <chrono>
#include <thread>
#include <unordered_map>
#include<set>

class S7200HWService : public HWService
{
  public:
    S7200HWService();
    virtual PVSSboolean initialize(int argc, char *argv[]);
    virtual PVSSboolean start();
    virtual void stop();
    virtual void workProc();
    virtual PVSSboolean writeData(HWObject *objPtr);
    std::map < std::string , std::vector< std::pair<std::string, void*> > > writeQueueForIP;
    std::set<std::string> IPAddressList;
    int CheckIP(std::string);

private:
    void handleConsumerConfigError(const std::string&, int, const std::string&);

    void handleConsumeNewMessage(const std::string&, const std::string&, const std::string&, char*);
    void handleNewIPAddress(const std::string& ip);

    errorCallbackConsumer _configErrorConsumerCB{[this](const std::string& ip, int err, const std::string& reason) { this->handleConsumerConfigError(ip, err, reason);}};
    consumeCallbackConsumer  _configConsumeCB{[this](const std::string& ip, const std::string& var, const std::string& pollTime, char* payload){this->handleConsumeNewMessage(ip, var, pollTime,std::move(payload));}};
    std::function<void(const std::string&)> _newIPAddressCB{[this](const std::string& ip){this->handleNewIPAddress(ip);}};

    //Common
    void insertInDataToDp(CharString&& address, char* value);
    std::mutex _toDPmutex;
    
    std::map < std::string, int > DisconnectsPerIP;
    std::queue<std::pair<CharString,char*>> _toDPqueue;

    enum
    {
       ADDRESS_OPTIONS_IP = 0,
       ADDRESS_OPTIONS_VAR,
       ADDRESS_OPTIONS_POLLTIME,
       ADDRESS_OPTIONS_SIZE
    } ADDRESS_OPTIONS;

    std::vector<std::thread> _pollingThreads;

    std::map<std::string, S7200LibFacade*> _facades;
};


void handleSegfault(int signal_code);

#endif
