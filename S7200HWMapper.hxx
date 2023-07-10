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

class S7200HWMapper : public HWMapper
{
  public:
    std::map<std::string, bool> isIPrunning;
    virtual PVSSboolean addDpPa(DpIdentifier &dpId, PeriphAddr *confPtr);
    virtual PVSSboolean clrDpPa(DpIdentifier &dpId, PeriphAddr *confPtr);

    const std::unordered_set<std::string>& getS7200IPs() {return S7200IPs;}
    const std::map<std::string, std::vector<std::pair<std::string, int>>>& getS7200Addresses(){return S7200Addresses;}
    bool checkIPExist(std::string);

  private:
    void addAddress(const std::string &ip, const std::string &var, const std::string &pollTime);
    void removeAddress(const std::string& ip, const std::string& var, const std::string &pollTime);

    std::unordered_set<std::string> S7200IPs;
    std::map<std::string,  int> addressCounter; //For counting the number of times an address has been added
    std::map<std::string, std::vector<std::pair<std::string, int>>> S7200Addresses;

    enum Direction
    {
        DIRECTION_OUT = 1,
        DIRECTION_IN = 2,
        DIRECTION_INOUT = 6,
    };
};

#endif
