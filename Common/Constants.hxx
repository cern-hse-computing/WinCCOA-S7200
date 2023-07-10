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

#ifndef CONSTANTS_HXX_
#define CONSTANTS_HXX_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <map>
#include <Common/Utils.hxx>
#include <Common/Logger.hxx>

namespace Common{

/*!
 * \class Constants
 * \brief Class containing constant values used in driver
 */
class Constants{
public:

    static void setDrvName(std::string lp);

    static std::string& getDrvName();

    static std::string& getDrvVersion();

    // called in driver init to set the driver number dynamically
    static void setDrvNo(uint32_t no);
    // subsequentally called when writing buffers etc.
    static uint32_t getDrvNo();

    static void setLocalTsapPort(uint32_t port);
    static const uint32_t& getLocalTsapPort();

    static void setRemoteTsapPort(uint32_t port);
    static const uint32_t& getRemoteTsapPort();

    static void setPollingInterval(size_t pollingInterval);
    static const size_t& getPollingInterval();

    static const std::map<std::string,std::function<void(const char *)>>& GetParseMap();

private:
    static std::string drv_name;
    static std::string drv_version;

    static uint32_t DRV_NO;   // WinCC OA manager number
    static uint32_t TSAP_PORT_LOCAL;
    static uint32_t TSAP_PORT_REMOTE;
    static size_t POLLING_INTERVAL;

    static std::map<std::string, std::function<void(const char *)>> parse_map;
};

inline const std::map<std::string,std::function<void(const char *)>>& Constants::GetParseMap()
{
    return parse_map;
}

inline void Constants::setDrvName(std::string dname){
	drv_name = dname;
}

inline std::string& Constants::getDrvName(){
	return drv_name;
}

inline std::string& Constants::getDrvVersion(){
	return drv_version;
}

inline void Constants::setDrvNo(uint32_t no){
	DRV_NO = no;
}

inline uint32_t Constants::getDrvNo(){
	return DRV_NO;
}

inline void Constants::setLocalTsapPort(uint32_t port){
    printf("Setting TSAP_PORT_LOCAL=" + CharString(port) + "\n");
    TSAP_PORT_LOCAL = port;
}

inline const uint32_t& Constants::getLocalTsapPort(){
    return TSAP_PORT_LOCAL;
}

inline void Constants::setRemoteTsapPort(uint32_t port){
    printf("Setting TSAP_PORT_REMOTE=" + CharString(port) + "\n");
    TSAP_PORT_REMOTE = port;
}

inline const uint32_t& Constants::getRemoteTsapPort(){
    return TSAP_PORT_REMOTE;
}

inline void Constants::setPollingInterval(size_t pollingInterval)
{
    printf("Setting POLLING_INTERVAL=" + CharString(pollingInterval) + "\n");
    POLLING_INTERVAL = pollingInterval;
}

inline const size_t& Constants::getPollingInterval()
{
    return POLLING_INTERVAL;
}

}//namespace
#endif /* CONSTANTS_HXX_ */
