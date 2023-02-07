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


#ifndef S7200HWMAPPER_H_
#define S7200HWMAPPER_H_

#include <HWMapper.hxx>
#include <unordered_set>

// TODO: Write here all the Transformation types, one for every transformation
#define S7200DrvBoolTransType (TransUserType)
#define S7200DrvUint8TransType (TransUserType + 1)
#define S7200DrvInt16TransType (TransUserType + 2)
#define S7200DrvInt32TransType (TransUserType + 3)
#define S7200DrvFloatTransType (TransUserType + 4)
#define S7200DrvStringTransType (TransUserType + 5)

using newIPAddressCallback = std::function<void(const std::string& ip)>;

class S7200HWMapper : public HWMapper
{
  public:
    virtual PVSSboolean addDpPa(DpIdentifier &dpId, PeriphAddr *confPtr);
    virtual PVSSboolean clrDpPa(DpIdentifier &dpId, PeriphAddr *confPtr);

    void setNewIPAddressCallback(newIPAddressCallback cb) {_newIPAddressCB = cb;}
    const std::unordered_set<std::string>& getS7200IPs(){return S7200IPs;}
    const std::map<std::string, std::unordered_set<std::string>>& getS7200Addresses(){return S7200Addresses;}

  private:
    void addAddress(const std::string& ip, const std::string& var);

    std::unordered_set<std::string> S7200IPs;
    std::map<std::string, std::unordered_set<std::string>> S7200Addresses;
    newIPAddressCallback _newIPAddressCB = {nullptr};

    enum Direction
    {
        DIRECTION_OUT = 1,
        DIRECTION_IN = 2,
        DIRECTION_INOUT = 6,
    };
};

#endif
