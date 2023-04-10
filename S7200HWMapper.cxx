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
 * Co-Author: Richi Dubey (HSE)
 *
 **/

#include "S7200HWMapper.hxx"
#include "Transformations/S7200StringTrans.hxx"
#include "Transformations/S7200Int16Trans.hxx"
#include "Transformations/S7200Int32Trans.hxx"
#include "Transformations/S7200FloatTrans.hxx"
#include "Transformations/S7200BoolTrans.hxx"
#include "Transformations/S7200Uint8Trans.hxx"
#include "S7200HWService.hxx"

#include <algorithm>
#include "Common/Logger.hxx"
#include "Common/Constants.hxx"
#include "Common/Utils.hxx"
#include <PVSSMacros.hxx>     // DEBUG macros

//--------------------------------------------------------------------------------
// We get new configs here. Create a new HW-Object on arrival and insert it.

PVSSboolean S7200HWMapper::addDpPa(DpIdentifier &dpId, PeriphAddr *confPtr)
{
  // We don't use Subindices here, so its simple.
  // Otherwise we had to look if we already have a HWObject and adapt its length.

  Common::Logger::globalInfo(Common::Logger::L2,"addDpPa called for ", confPtr->getName().c_str());
  //Common::Logger::globalInfo(Common::Logger::L1,"addDpPa direction ", CharString(confPtr->getDirection()));

  // tell the config how we will transform data to/from the device
  // by installing a Transformation object into the PeriphAddr
  // In this template, the Transformation type was set via the
  // configuration panel (it is already set in the PeriphAddr)

  // TODO this really depends on your protocol and is therefore just an example
  // in this example we use the ones from Pbus, as those can be selected
  // with the SIM driver parametrization panel
  switch ((uint32_t)confPtr->getTransformationType()) {
  case TransUndefinedType:
      Common::Logger::globalInfo(Common::Logger::L1,"Undefined transformation" + CharString(confPtr->getTransformationType()));
      return HWMapper::addDpPa(dpId, confPtr);
  case S7200DrvBoolTransType:
        Common::Logger::globalInfo(Common::Logger::L3,"Bool transformation");
        confPtr->setTransform(new Transformations::S7200BoolTrans);
        break;
  case S7200DrvUint8TransType:
      Common::Logger::globalInfo(Common::Logger::L3,"Uint8 transformation");
      confPtr->setTransform(new Transformations::S7200Uint8Trans);
      break;
  case S7200DrvInt32TransType:
      Common::Logger::globalInfo(Common::Logger::L3,"Int32 transformation");
      confPtr->setTransform(new Transformations::S7200Int32Trans);
      break;
  case S7200DrvInt16TransType:
      Common::Logger::globalInfo(Common::Logger::L3,"Int16 transformation");
      confPtr->setTransform(new Transformations::S7200Int16Trans);
      break;
  case S7200DrvFloatTransType:
      Common::Logger::globalInfo(Common::Logger::L3,"Float transformation");
      confPtr->setTransform(new Transformations::S7200FloatTrans);
      break;
  case S7200DrvStringTransType:
        Common::Logger::globalInfo(Common::Logger::L3,"String transformation");
        confPtr->setTransform(new Transformations::S7200StringTrans);
        break;
  default:
      Common::Logger::globalError("S7200HWMapper::addDpPa", CharString("Illegal transformation type ") + CharString((int) confPtr->getTransformationType()));
      return HWMapper::addDpPa(dpId, confPtr);
  }

  // First add the config, then the HW-Object
  if ( !HWMapper::addDpPa(dpId, confPtr) )  // FAILED !! 
  {
    Common::Logger::globalInfo(Common::Logger::L1,"Failed in adding DP Para to HW Mapper object");
      return PVSS_FALSE;
  }

  HWObject *hwObj = new HWObject;
  // Set Address and Subindex
  Common::Logger::globalInfo(Common::Logger::L3, "New Object", "name:" + confPtr->getName());
  hwObj->setConnectionId(confPtr->getConnectionId());
  hwObj->setAddress(confPtr->getName());       // Resolve the HW-Address, too

  // Set the data type.
  hwObj->setType(confPtr->getTransform()->isA());

  // Set the len needed for data from _all_ subindices of this PVSS-Address.
  // Because we will deal with subix 0 only this is the Transformation::itemSize
  hwObj->setDlen(confPtr->getTransform()->itemSize());
//TODO - number of elements?
  // Add it to the list
  addHWObject(hwObj);

  std::vector<std::string> addressOptions = Common::Utils::split(hwObj->getAddress().c_str());

  if(confPtr->getDirection() == DIRECTION_IN || confPtr->getDirection() == DIRECTION_INOUT)
  {
      if (addressOptions.size() == 3) // IP + VAR + POLLTIME
      {
        if(addressOptions[0].compare("VERSION"))
          addAddress(addressOptions[0], addressOptions[1], addressOptions[2]);
      }
  }

  return PVSS_TRUE;
}

//--------------------------------------------------------------------------------

PVSSboolean S7200HWMapper::clrDpPa(DpIdentifier &dpId, PeriphAddr *confPtr)
{
  Common::Logger::globalInfo(Common::Logger::L2, "clrDpPa called for" + confPtr->getName());

  // Find our HWObject via a template
  HWObject adrObj;
  adrObj.setAddress(confPtr->getName());

  // Lookup HW-Object via the Name, not via the HW-Address
  // The class type isn't important here
  HWObject *hwObj = findHWAddr(&adrObj);

  std::vector<std::string> addressOptions = Common::Utils::split(hwObj->getAddress().c_str());

  if(confPtr->getDirection() == DIRECTION_IN || confPtr->getDirection() == DIRECTION_INOUT)
  {
      if (addressOptions.size() == 3) // IP + VAR + POLLTIME
      {
        removeAddress(addressOptions[0], addressOptions[1], addressOptions[2]);
      }
  }

  if ( hwObj )
  {
    // Object exists, remove it from the list and delete it.
    clrHWObject(hwObj);
    delete hwObj;
  }

  // Call function of base class to remove config
  return HWMapper::clrDpPa(dpId, confPtr);
}

void S7200HWMapper::addAddress(const std::string &ip, const std::string &var, const std::string &pollTime)
{
    if(S7200IPs.find(ip) == S7200IPs.end())
    {
        S7200IPs.insert(ip);
        isIPrunning.insert(std::pair<std::string, bool>(ip, true));
        Common::Logger::globalInfo(Common::Logger::L2, "Received var from a new IP Address");
        S7200Addresses.erase(ip);
        S7200Addresses.insert(std::pair<std::string, std::vector<std::pair<std::string, int>>>(ip, std::vector<std::pair<std::string, int>>()));
    }

    if(S7200Addresses.count(ip)){

      if(std::find(S7200Addresses[ip].begin(), S7200Addresses[ip].end(), make_pair(var,  std::stoi(pollTime))) == S7200Addresses[ip].end())
      {
        S7200Addresses[ip].push_back(make_pair(var, std::stoi(pollTime)));
        Common::Logger::globalInfo(Common::Logger::2, "Added to S7200AddressList", var.c_str());
      }
    }
}

void S7200HWMapper::removeAddress(const std::string &ip, const std::string &var, const std::string &pollTime)
{
  
  if(S7200Addresses.count(ip)) {
    
    if(std::find(S7200Addresses[ip].begin(), S7200Addresses[ip].end(), make_pair(var,  std::stoi(pollTime))) != S7200Addresses[ip].end()) {
        S7200Addresses[ip].erase(std::find(S7200Addresses[ip].begin(), S7200Addresses[ip].end(), make_pair(var,  std::stoi(pollTime))));
    }

    if(S7200Addresses[ip].size() == 0) {
      S7200IPs.erase(ip);
      S7200Addresses.erase(ip);
      Common::Logger::globalInfo(Common::Logger::L1, "All Addresses from this IP address deleted.");

      while(isIPrunning[ip]) {
          Common::Logger::globalInfo(Common::Logger::L1, "Thread for the IP is still running. Sleeping for 1 second");
          std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      Common::Logger::globalInfo(Common::Logger::L1, "Thread exited.");
    }
  }
}

bool S7200HWMapper::checkIPExist(std::string ip) {
  return S7200IPs.count(ip);
}