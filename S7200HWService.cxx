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

#include <S7200HWService.hxx>

#include <DrvManager.hxx>
#include <PVSSMacros.hxx>     // DEBUG macros
#include "Common/Logger.hxx"
#include "Common/Constants.hxx"
#include "Common/Utils.hxx"

#include "S7200HWMapper.hxx"
#include "S7200LibFacade.hxx"

#include <signal.h>
#include <execinfo.h>
#include <exception>
#include <chrono>
#include <utility>
#include <thread>

static std::atomic<bool> _consumerRun{true};

//--------------------------------------------------------------------------------
// called after connect to data

S7200HWService::S7200HWService()
{
  signal(SIGSEGV, handleSegfault);
}

PVSSboolean S7200HWService::initialize(int argc, char *argv[])
{
  // use this function to initialize internals
  // if you don't need it, you can safely remove the whole method
  Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__,"start");

  // add callback for new ip addresses
  static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->setNewIPAddressCallback(_newIPAddressCB);

  Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__,"end");
  // To stop driver return PVSS_FALSE
  return PVSS_TRUE;
}

void S7200HWService::handleConsumerConfigError(const std::string& ip, int code, const std::string& str)
{
     Common::Logger::globalWarning(__PRETTY_FUNCTION__, CharString(ip.c_str(), ip.length()), str.c_str());
}

void S7200HWService::handleConsumeNewMessage(const std::string& ip, const std::string& var, char* payload)
{
    //Common::Logger::globalInfo(Common::Logger::L3, __PRETTY_FUNCTION__, (ip + ":" + var + ":" + payload).c_str());
    insertInDataToDp(std::move(CharString((ip+"$"+var).c_str())), payload);
}

void S7200HWService::handleNewIPAddress(const std::string& ip)
{
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "New IP:", ip.c_str());

    auto lambda = [&]
        {
          Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Inside polling thread");
            S7200LibFacade aFacade(ip, this->_configConsumeCB, this->_configErrorConsumerCB);
            if(!aFacade.isInitialized())
            {
                Common::Logger::globalError("Unable to initialize IP:", ip.c_str());
            }
            else
            {
                _facades[ip] = &aFacade;
                writeQueueForIP.insert(std::pair < std::string, std::vector < std::pair < std::string, void * > > > ( ip, std::vector<std::pair<std::string, void *> > ()));
                DisconnectsPerIP.insert(std::pair< std::string, int >( ip, 0));
                DisconnectsPerIP[ip] = 0;
                while(_consumerRun && DisconnectsPerIP[ip] < 20 && static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->checkIPExist(ip))
                {
                    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Polling");
                    //printf("Hi2 from printf for IP %s\n", ip.c_str());
                    auto pollingInterval = std::chrono::seconds(Common::Constants::getPollingInterval());
                    auto start = std::chrono::steady_clock::now();

                    auto vars = static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->getS7200Addresses();
                    if(vars.find(ip) != vars.end()){
                       //First do all the writes for this IP, then the reads
                        aFacade.write(writeQueueForIP[ip]);
                        writeQueueForIP[ip].clear();
                        aFacade.poll(vars[ip]);                         
                    }
                    auto end = std::chrono::steady_clock::now();
                    auto time_elapsed = end - start;
                   
                    // If we still have time left, then sleep
                    if(time_elapsed < pollingInterval)
                      std::this_thread::sleep_for(pollingInterval- time_elapsed);
                }

                Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Out of polling loop for the thread. Process stopped or max disconnects exceeded");
                aFacade.Disconnect();
                IPAddressList.erase(ip);
            }

        };    
    _pollingThreads.emplace_back(lambda);

    //Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "New IP polling started:", ip.c_str());
}

//--------------------------------------------------------------------------------
// called after connect to event

PVSSboolean S7200HWService::start()
{
  // use this function to start your hardware activity.  
   // Check if we need to launch consumer(s)
   // This list is automatically built by exisiting addresses sent at driver startup
   // new top
   for (const auto& ip : static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->getS7200IPs() )
   {
        IPAddressList.insert(ip);
        this->handleNewIPAddress(ip);
   }

  return PVSS_TRUE;
}

int S7200HWService::CheckIP(std::string IPAddress)
{
  return IPAddressList.count(IPAddress);
}
//--------------------------------------------------------------------------------

void S7200HWService::stop()
{
  // use this function to stop your hardware activity.
  Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__,"Stop");
  _consumerRun = false;

  for(auto& pt : _pollingThreads)
  {
    if(pt.joinable())
        pt.join();
  }
}

//--------------------------------------------------------------------------------

void S7200HWService::workProc()
{

  for (const auto& ip : static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->getS7200IPs() )
   {
        if(IPAddressList.count(ip) == 0)
        {
          Common::Logger::globalInfo(Common::Logger::L1,"Calling HandleNewIP() from workProc()");
          IPAddressList.insert(ip);
          this->handleNewIPAddress(ip);
        }
   }

  HWObject obj;
  //Common::Logger::globalInfo(Common::Logger::L1,"Inside WorkProc");
  // TODO somehow receive a message from your device
  std::lock_guard<std::mutex> lock{_toDPmutex};
  //Common::Logger::globalInfo(Common::Logger::L1,"Get lock on DPmutex");
  Common::Logger::globalInfo(Common::Logger::L3,__PRETTY_FUNCTION__,"Size", CharString(_toDPqueue.size()));
  while (!_toDPqueue.empty())
  {
    auto pair = std::move(_toDPqueue.front());
    _toDPqueue.pop();
    obj.setAddress(pair.first);

//    // a chance to see what's happening
//    if ( Resources::isDbgFlag(Resources::DBG_DRV_USR1) )
//      obj.debugPrint();
    std::vector<std::string> addressOptions = Common::Utils::split(pair.first.c_str());
    // find the HWObject via the periphery address in the HWObject list,
    HWObject *addrObj = DrvManager::getHWMapperPtr()->findHWObject(&obj);
    
    // ok, we found it; now send to the DPEs
    if ( addrObj )
    {
        Common::Logger::globalInfo(Common::Logger::L2,__PRETTY_FUNCTION__, pair.first, pair.second);
        //addrObj->debugPrint();
        obj.setOrgTime(TimeVar());  // current time
        int dataLengh = S7200LibFacade::getByteSizeFromAddress(Common::Utils::split(pair.first.c_str())[1]);

        //printf("-->send to WinCCOA '%s' :'%s', size:'%d'\n", pair.first.c_str(), pair.second, dataLengh);

        obj.setDlen(dataLengh); // lengh
        obj.setData((PVSSchar*)pair.second); //data
        obj.setObjSrcType(srcPolled);

        if( DrvManager::getSelfPtr()->toDp(&obj, addrObj) != PVSS_TRUE) {
          Common::Logger::globalInfo(Common::Logger::L1,"Problem in sending item's value to PVSS");
        }
    } else {
        Common::Logger::globalInfo(Common::Logger::L1,"Problem in getting HWObject for the address, increasing disconnect count");
        DisconnectsPerIP[addressOptions[ADDRESS_OPTIONS_IP]]++;
    }

  }
}

void S7200HWService::insertInDataToDp(CharString&& address, char* item)
{

    std::lock_guard<std::mutex> lock{_toDPmutex};
    _toDPqueue.push(std::move(std::make_pair<CharString, char*>(std::move(address), std::move(item))));
}

//--------------------------------------------------------------------------------
// we get data from PVSS and shall send it to the periphery

PVSSboolean S7200HWService::writeData(HWObject *objPtr)
{
//  Common::Logger::globalInfo(Common::Logger::L2,__PRETTY_FUNCTION__,"Incoming obj address",objPtr->getAddress());

  std::vector<std::string> addressOptions = Common::Utils::split(objPtr->getAddress().c_str());

  // CONFIG DPs have just 1
  if(addressOptions.size() == 1)
  {
      try
      {
        Common::Logger::globalInfo(Common::Logger::L2,"Incoming CONFIG address",objPtr->getAddress(), objPtr->getInfo() );
        Common::Constants::GetParseMap().at(std::string(objPtr->getAddress().c_str()))((const char*)objPtr->getData());
      }
      catch (std::exception& e)
      {
          Common::Logger::globalWarning(__PRETTY_FUNCTION__," No configuration handling for address:", objPtr->getAddress().c_str());
      }
  }
  else if (addressOptions.size() == ADDRESS_OPTIONS_SIZE) // Output
  {

    if(!addressOptions[ADDRESS_OPTIONS_IP].length())
    {
        Common::Logger::globalWarning(__PRETTY_FUNCTION__,"Empty IP");
        return PVSS_FALSE;
    }

    if(_facades.find(addressOptions[ADDRESS_OPTIONS_IP]) != _facades.end()){
        if(!S7200LibFacade::S7200AddressIsValid(addressOptions[ADDRESS_OPTIONS_VAR])){
          Common::Logger::globalInfo(Common::Logger::L1,"Incoming CONFIG address",objPtr->getAddress(), objPtr->getInfo() );
          Common::Logger::globalWarning(__PRETTY_FUNCTION__,"Not a valid Var");
          return PVSS_FALSE;
        }
        else{
          auto wrQueue = writeQueueForIP.find(addressOptions[ADDRESS_OPTIONS_IP]);
          int length = (int)objPtr->getDlen();

          if(length == 2) {
            char *correctval = (char *)malloc(sizeof(int16_t));
            memcpy(correctval, objPtr->getDataPtr(), sizeof(int16_t));
            wrQueue->second.push_back( std::make_pair( addressOptions[ADDRESS_OPTIONS_VAR], correctval));
          } else if(length == 4){
            char *correctval = (char*)malloc(sizeof(float));
            memcpy(correctval, objPtr->getDataPtr(), sizeof(sizeof(float)));
            wrQueue->second.push_back( std::make_pair( addressOptions[ADDRESS_OPTIONS_VAR], correctval));
          } else {
            char *correctval = (char*)malloc(length);
            memcpy(correctval, objPtr->getDataPtr(), length);
            wrQueue->second.push_back( std::make_pair( addressOptions[ADDRESS_OPTIONS_VAR], correctval));
          }

          Common::Logger::globalInfo(Common::Logger::L1,"Added write request to queue",objPtr->getAddress(), objPtr->getInfo() );
        }
    }
    else{
        Common::Logger::globalWarning(__PRETTY_FUNCTION__,"Connection not found");
        return PVSS_FALSE;
    }
  }
  else
  {
      Common::Logger::globalWarning(__PRETTY_FUNCTION__," Invalid address options size for address: ", objPtr->getAddress().c_str());
  }

  return PVSS_TRUE;
}

//--------------------------------------------------------------------------------


void handleSegfault(int signal_code){
    void *array[50];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 50);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", signal_code);
    Common::Logger::globalWarning("S7200HWService suffered a segmentation fault, code " + CharString(signal_code));
    backtrace_symbols_fd(array, size, STDERR_FILENO);

    // restore and trigger default handle (to get the core dump)
    signal(signal_code, SIG_DFL);
    kill(getpid(), signal_code);

    exit(1);
}
