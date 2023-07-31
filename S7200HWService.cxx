/** © Copyright 2023 CERN
 *
 * This software is distributed under the terms of the
 * GNU Lesser General Public Licence version 3 (LGPL Version 3),
 * copied verbatim in the file “LICENSE”
 *
 * In applying this licence, CERN does not waive the privileges
 * and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 *
 * Author: Adrien Ledeul (HSE), Richi Dubey (HSE)
 *
 **/

#include <S7200HWService.hxx>
#include "S7200Resources.hxx"

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
  // To stop driver return PVSS_FALSE
  return PVSS_TRUE;
}

void S7200HWService::handleConsumerConfigError(const std::string& ip, int code, const std::string& str)
{
     Common::Logger::globalWarning(__PRETTY_FUNCTION__, CharString(ip.c_str(), ip.length()), str.c_str());
}

void S7200HWService::handleConsumeNewMessage(const std::string& ip, const std::string& var, const std::string& pollTime, char* payload)
{
  if(ip.compare("_VERSION") == 0)  {
    insertInDataToDp(std::move(CharString((ip).c_str())), payload);  //Config DPs do not have a polling time or an IP address associated with them in the address.
  }
  else if(var.compare("_Error") == 0)
    insertInDataToDp(std::move(CharString((ip + "$" + var ).c_str())), payload);  //Config DPs do not have a polling time associated with them in the address.
  else 
    //Common::Logger::globalInfo(Common::Logger::L3, __PRETTY_FUNCTION__, (ip + ":" + var + ":" + payload).c_str());
    insertInDataToDp(std::move(CharString((ip + "$" + var + "$" + pollTime).c_str())), payload);
}

void S7200HWService::handleNewIPAddress(const std::string& ip)
{ 
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "New IP:", ip.c_str());
    static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->isIPrunning.insert(std::pair<std::string, bool>(ip, true));
    static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->isIPrunning[ip] = true;

    auto lambda = [&]
        {
          std::string IP_FIXED = ip.c_str();
          Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Inside polling thread");
          S7200LibFacade aFacade(ip, this->_configConsumeCB, this->_configErrorConsumerCB);
          _facades[ip] = &aFacade;
          writeQueueForIP.insert(std::pair < std::string, std::vector < std::pair < std::string, void * > > > ( ip, std::vector<std::pair<std::string, void *> > ()));
          aFacade.Connect();

          if(!aFacade.isInitialized())
          {
              Common::Logger::globalInfo(Common::Logger::L1, "Unable to initialize IP:", IP_FIXED.c_str());
              Common::Logger::globalInfo(Common::Logger::L1, "Trying to connect again in 5 seconds");
              
              std::this_thread::sleep_for(std::chrono::seconds(5));

              aFacade.S7200MarkDeviceConnectionError(IP_FIXED, true);
              
              do {
                //Try to connect again.
                aFacade.Reconnect();

                if(!aFacade.isInitialized()) {
                  Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Failure in re-connection. Trying again in 5 seconds");
                  aFacade.Disconnect();
                  std::this_thread::sleep_for(std::chrono::seconds(5));
                }
              } while(!aFacade.isInitialized()  && static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->checkIPExist(IP_FIXED) &&_consumerRun);
          }

          if(aFacade.isInitialized() && static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->checkIPExist(IP_FIXED) && _consumerRun) {
            std::this_thread::sleep_for(std::chrono::seconds(3)); //Give some time for the driver to load the addresses.

            aFacade.S7200MarkDeviceConnectionError(IP_FIXED, false);

            auto first_time = std::chrono::steady_clock::now();
            
            while(_consumerRun && static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->checkIPExist(IP_FIXED))
            {
              if(!S7200Resources::getDisableCommands()) {
                // The Server is Active (for redundant systems)
                
                Common::Logger::globalInfo(Common::Logger::L2,__PRETTY_FUNCTION__, "Polling");
                auto cycleInterval = std::chrono::seconds(1);
                auto start = std::chrono::steady_clock::now();

                auto vars = static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->getS7200Addresses();
                if(vars.find(IP_FIXED) != vars.end()){
                    //First do all the writes for this IP, then the reads
                    aFacade.write(writeQueueForIP[IP_FIXED]);
                    aFacade.markForNextRead(writeQueueForIP[IP_FIXED], first_time);
                    writeQueueForIP[IP_FIXED].clear();
                    aFacade.Poll(vars[IP_FIXED], start);                         
                }
                auto end = std::chrono::steady_clock::now();
                auto time_elapsed = end - start;
                
                // If we still have time left, then sleep
                if(time_elapsed < cycleInterval)
                  std::this_thread::sleep_for(cycleInterval- time_elapsed);

                if(aFacade.readFailures > 5) {
                  Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "More than 5 read failures, Disconnecting");

                  aFacade.S7200MarkDeviceConnectionError(IP_FIXED, true);

                  do {
                    //Disconnect and try to connect again.
                    aFacade.Disconnect();
                    aFacade.Reconnect();

                    if(!aFacade.isInitialized()) {
                      Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Failure in re-connection. Trying again in 5 seconds");
                      std::this_thread::sleep_for(std::chrono::seconds(5));
                    }
                  } while(!aFacade.isInitialized() && static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->checkIPExist(IP_FIXED) && _consumerRun);
                  aFacade.readFailures = 0;
                  aFacade.S7200MarkDeviceConnectionError(IP_FIXED, false);
                }
              } else {
                // The Server is Passive (for redundant systems)
                std::this_thread::sleep_for(std::chrono::seconds(1));
              }
            }
          }

          if( static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->checkIPExist(IP_FIXED) == false )
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Out of polling loop for the thread. IP removed from list. IP: ", IP_FIXED.c_str());
          else if(!_consumerRun)
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Out of polling loop for the thread. All threads were asked to stop. This looping thread was for IP: ", IP_FIXED.c_str()); 
          else
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Out of polling loop for the thread. A different reason. This looping thread was for IP: ", IP_FIXED.c_str()); 

          aFacade.Disconnect();
          IPAddressList.erase(IP_FIXED);
          static_cast<S7200HWMapper*>(DrvManager::getHWMapperPtr())->isIPrunning[IP_FIXED] = false;
          aFacade.clearLastWriteTimeList();

          Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Exiting Lambda Thread. IP: ", IP_FIXED.c_str());
        };    
    _pollingThreads.emplace_back(lambda);
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

  //Write Driver version
  char* DrvVersion = new char[Common::Constants::getDrvVersion().size()];
  std::strcpy(DrvVersion, Common::Constants::getDrvVersion().c_str());
  Common::Logger::globalInfo(Common::Logger::L1, "Sent Driver version: ", DrvVersion);
  handleConsumeNewMessage("_VERSION", "", "", DrvVersion);

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
  //Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__,"Size", CharString(_toDPqueue.size()));
  while (!_toDPqueue.empty())
  {
    //Common::Logger::globalInfo(Common::Logger::L3,__PRETTY_FUNCTION__, CharString("There are ") + (to_string((_toDPqueue.size()))).c_str() + CharString(" elements to process"));
    auto pair = std::move(_toDPqueue.front());
    _toDPqueue.pop();
    //        Common::Logger::globalInfo(Common::Logger::L1,"For Request, First element is ", pair.first);
    //    Common::Logger::globalInfo(Common::Logger::L1,"For Request, Second element is ", pair.second);
    std::vector<std::string> addressOptions = Common::Utils::split(pair.first.c_str());
    obj.setAddress(pair.first);

    if(strcmp(pair.first.c_str(), "_VERSION") == 0) {
        Common::Logger::globalInfo(Common::Logger::L2,"For driver version, writing to WinCCOA value ", pair.second);
    }

//    // a chance to see what's happening
//    if ( Resources::isDbgFlag(Resources::DBG_DRV_USR1) )
//      obj.debugPrint();
    
    // find the HWObject via the periphery address in the HWObject list,
    HWObject *addrObj = DrvManager::getHWMapperPtr()->findHWObject(&obj);

    // ok, we found it; now send to the DPEs
    if ( addrObj )
    {
        //Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, pair.first, pair.second);
        //addrObj->debugPrint();
        obj.setOrgTime(TimeVar());  // current time
        
        if(strcmp(pair.first.c_str(), "_VERSION") == 0) {
          obj.setDlen(4);
          Common::Logger::globalInfo(Common::Logger::L2,"AddrObj found, For driver version, writing to WinCCOA value ", pair.second);
        } else {
          int dataLengh = S7200LibFacade::getByteSizeFromAddress(Common::Utils::split(pair.first.c_str())[1]);

          //   Common::Logger::globalInfo(Common::Logger::L1, "-->send to WinCCOA first ", pair.first.  c_str());
          //   Common::Logger::globalInfo(Common::Logger::L1, "-->send to WinCCOA second ", pair.second );
          //   Common::Logger::globalInfo(Common::Logger::L1, "-->send to WinCCOA thirds ", std::to_string(dataLengh).c_str());
          //Common::Logger::globalInfo(Common::Logger::L1,"Data length is ", std::to_string(dataLengh).c_str());
          obj.setDlen(dataLengh); // lengh
        }

        obj.setData((PVSSchar*)pair.second); //data
        obj.setObjSrcType(srcPolled);

        if( DrvManager::getSelfPtr()->toDp(&obj, addrObj) != PVSS_TRUE) {
          Common::Logger::globalInfo(Common::Logger::L1,"Problem in sending item's value to PVSS");
        }
    } else {
        Common::Logger::globalInfo(Common::Logger::L1,"Problem in getting HWObject for the address: " + pair.first);   
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
{  Common::Logger::globalInfo(Common::Logger::L2,__PRETTY_FUNCTION__,"Incoming obj address",objPtr->getAddress());

  std::vector<std::string> addressOptions = Common::Utils::split(objPtr->getAddress().c_str());

  // CONFIG DPs have just 1
  if(addressOptions.size() == 1)
  {
      try
      {
        Common::Logger::globalInfo(Common::Logger::L1,"Incoming CONFIG address",objPtr->getAddress(), objPtr->getInfo() );
        
        if(addressOptions[ADDRESS_OPTIONS_IP].compare("_DEBUGLVL") == 0) {
          int16_t* retVal = new int16_t;
          char *IntToConvert = ( char* )objPtr->getDataPtr();

          // swap the bytes into a temporary buffer
          retVal[0] = IntToConvert[1];
          retVal[1] = IntToConvert[0];

          Common::Logger::globalInfo(Common::Logger::L1,"Received DebugLvl change request to value: ", std::to_string(*retVal).c_str());

          if(*retVal > 0 && *retVal  < 4) {
              Common::Constants::GetParseMap().at(std::string(objPtr->getAddress().c_str()))((char *)retVal);
              Common::Logger::globalInfo(Common::Logger::L1,"Set Debug level successfully to : ", std::to_string(*retVal).c_str());
          } 
          
          delete retVal;
          return PVSS_TRUE;
        }
        
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
            Common::Logger::globalWarning("Not a valid Var for address", objPtr->getAddress().c_str());
            return PVSS_FALSE;
        }
        else{
          auto wrQueue = writeQueueForIP.find(addressOptions[ADDRESS_OPTIONS_IP]);
          int length = (int)objPtr->getDlen();

          if(length == 2) {
            char *correctval = new char[sizeof(int16_t)];
            std::memcpy(correctval, objPtr->getDataPtr(), sizeof(int16_t));
            int16_t* checkVal = reinterpret_cast<int16_t*> (correctval);

            const int16_t inInt16 = *checkVal;
            
              int16_t retVal;
              char *IntToConvert = ( char* ) & inInt16;
              char *returnInt = ( char* ) & retVal;

              // swap the bytes into a temporary buffer
              returnInt[0] = IntToConvert[1];
              returnInt[1] = IntToConvert[0];

            Common::Logger::globalInfo(Common::Logger::L2,"Received request to write integer, Correct val is: ", to_string(retVal).c_str());
            wrQueue->second.push_back( std::make_pair( addressOptions[ADDRESS_OPTIONS_VAR], correctval));
          } else if(length == 4){
            char *correctval = new char[sizeof(float)];
            std::memcpy(correctval, objPtr->getDataPtr(), sizeof(float));
            float *checkVal = reinterpret_cast<float*> (correctval);

            const float inFloat = *checkVal;
           
            float retVal;
            char *floatToConvert = ( char* ) & inFloat;
            char *returnFloat = ( char* ) & retVal;

            // swap the bytes into a temporary buffer
            returnFloat[0] = floatToConvert[3];
            returnFloat[1] = floatToConvert[2];
            returnFloat[2] = floatToConvert[1];
            returnFloat[3] = floatToConvert[0];


            Common::Logger::globalInfo(Common::Logger::L2,"Received request to write float, Correct val is:  ", to_string(retVal).c_str());
            wrQueue->second.push_back( std::make_pair( addressOptions[ADDRESS_OPTIONS_VAR], correctval));
          } else {
            char *correctval = new char[length];
            std::memcpy(correctval, objPtr->getDataPtr(), length);
            Common::Logger::globalInfo(Common::Logger::L2,"Received request to write non integer/float: ", correctval);
            wrQueue->second.push_back( std::make_pair( addressOptions[ADDRESS_OPTIONS_VAR], correctval));
          }

          Common::Logger::globalInfo(Common::Logger::L1,"Added write request to queue",objPtr->getAddress(), objPtr->getInfo() );
        }
    }
    else{
        Common::Logger::globalWarning(__PRETTY_FUNCTION__,"Connection not found for IP: ", addressOptions[ADDRESS_OPTIONS_IP].c_str());
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
