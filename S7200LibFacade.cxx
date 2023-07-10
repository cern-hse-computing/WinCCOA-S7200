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

#include <csignal>

#include "S7200LibFacade.hxx"
#include "Common/Constants.hxx"
#include "Common/Logger.hxx"

#include <algorithm>
#include <vector>


S7200LibFacade::S7200LibFacade(const std::string& ip, consumeCallbackConsumer cb, errorCallbackConsumer erc = nullptr)
    : _ip(ip), _consumeCB(cb), _errorCB(erc)
{
     Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Initialized LibFacade with IP: ", _ip.c_str());
}

void S7200LibFacade::Connect()
{
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Connecting to : Local TSAP Port : Remote TSAP Port'", (_ip + " : "+ std::to_string(Common::Constants::getLocalTsapPort()) + ":" + std::to_string(Common::Constants::getRemoteTsapPort())).c_str());

    try{
        _client = new TS7Client();

        _client->SetConnectionParams(_ip.c_str(), Common::Constants::getLocalTsapPort(), Common::Constants::getRemoteTsapPort());
        int res = _client->Connect();


        if (res==0) {
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Connected to '", _ip.c_str());
            //printf("  PDU Requested  : %d bytes\n",Client->PDURequested());
            //printf("  PDU Negotiated : %d bytes\n",Client->PDULength());
            _initialized = true;
        }
    }
    catch(std::exception& e)
    {
        Common::Logger::globalWarning("Snap7: Unable to initialize connection!", e.what());
    }
}

void S7200LibFacade::Reconnect()
{
 Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Reconnecting to : Local TSAP Port : Remote TSAP Port'", (_ip + " : "+ std::to_string(Common::Constants::getLocalTsapPort()) + ":" + std::to_string(Common::Constants::getRemoteTsapPort())).c_str());

    try{
        _client = new TS7Client();

        _client->SetConnectionParams(_ip.c_str(), Common::Constants::getLocalTsapPort(), Common::Constants::getRemoteTsapPort());
        int res = _client->Connect();


        if (res==0) {
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Connected to '", _ip.c_str());
            //printf("  PDU Requested  : %d bytes\n",Client->PDURequested());
            //printf("  PDU Negotiated : %d bytes\n",Client->PDULength());
            _initialized = true;
        }
    }
    catch(std::exception& e)
    {
        Common::Logger::globalWarning("Snap7: Unable to initialize connection!", e.what());
    }
}

void S7200LibFacade::Disconnect()
{
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Disconnecting from '", _ip.c_str());

    try{
        int res = _client->Disconnect();


        if (res==0) {
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Snap7: Disconnected successfully from '", _ip.c_str());
            _initialized = false;
        }
    }
    catch(std::exception& e)
    {
        Common::Logger::globalWarning("Snap7: Unable to disconnect!", e.what());
    }
}

void S7200LibFacade::clearLastWriteTimeList() {
    lastWritePerAddress.clear();
    //clear() destroys all the elements in the map
}

void S7200LibFacade::Poll(std::vector<std::pair<std::string, int>>& vars, std::chrono::time_point<std::chrono::steady_clock> loopStartTime)
{
    std::vector<std::pair<std::string, void *>> addresses;

    for (uint i = 0 ; i < vars.size() ; i++) {
        if(S7200AddressIsValid(vars[i].first)){
            if(lastWritePerAddress.count(vars[i].first) == 0) {
                lastWritePerAddress.insert(std::pair<std::string, std::chrono::time_point<std::chrono::steady_clock>>(vars[i].first, loopStartTime));
                Common::Logger::globalInfo(Common::Logger::L3,"Added to lastWritePerAddress queue: address", vars[i].first.c_str());
                addresses.push_back(std::pair<std::string, void *>(vars[i].first, (void *)&vars[i].second));
            } else{
                int fpollTime;
                int fpollingInterval = std::chrono::seconds(Common::Constants::getPollingInterval()).count() > 0 ? std::chrono::seconds(Common::Constants::getPollingInterval()).count() : 2;  

                if(fpollingInterval < vars[i].second) {
                    fpollTime = vars[i].second;
                } else {
                    fpollTime = fpollingInterval;
                    //Common::Logger::globalInfo(Common::Logger::L1,"Using default polling Interval for address", vars[i].first.c_str());
                    //Common::Logger::globalInfo(Common::Logger::L1,"Default polling Interval: ", std::to_string(fpollingInterval).c_str());
                }

                std::chrono::duration<double> tDiff = loopStartTime - lastWritePerAddress[vars[i].first];
                if((int)tDiff.count() >= fpollTime) {
                    lastWritePerAddress[vars[i].first] = loopStartTime;
                    addresses.push_back(std::pair<std::string, void *>(vars[i].first, (void *)&vars[i].second));
                }
            }
        }
    }

    if(addresses.size() == 0) {
        Common::Logger::globalInfo(Common::Logger::L2, "Valid vars size is 0, did not call read");
        return;
    }
    
                       
    // for(uint i = 0; i < addresses.size() ; i++) {
    //     int a;
    //     std::memcpy(&a, addresses[i].second, sizeof(int));    
    //     Common::Logger::globalInfo(Common::Logger::L1,("Address: "+ addresses[i].first + "Polling Time" + std::to_string(a)).c_str());
    // }
    
    S7200ReadWriteMaxN(addresses, 19, PDU_SIZE, OVERHEAD_READ_VARIABLE, OVERHEAD_READ_MESSAGE, OPERATION_READ);
}

void S7200LibFacade::write(std::vector<std::pair<std::string, void *>> addresses) {
    S7200ReadWriteMaxN(addresses, 12, PDU_SIZE, OVERHEAD_WRITE_VARIABLE, OVERHEAD_WRITE_MESSAGE, OPERATION_WRITE);

    for(uint i = 0; i < addresses.size(); i++) {
        delete[] (char *)addresses[i].second;
    }
}

void S7200LibFacade::markForNextRead(std::vector<std::pair<std::string, void *>> addresses, std::chrono::time_point<std::chrono::steady_clock> loopFirstStartTime) {
    for(auto & PairAddress: addresses) {
         if(S7200AddressIsValid(PairAddress.first)){
            if(lastWritePerAddress.count(PairAddress.first) != 0) {
                lastWritePerAddress[PairAddress.first] = loopFirstStartTime;
            }
         }
    }
}

int S7200LibFacade::S7200AddressGetWordLen(std::string S7200Address)
{
    if(S7200Address.length() < 2){
        return -1; //invalid
    }
    else if(std::tolower((char) S7200Address.at(1)) == 'b'){
        return S7WLByte;
    }
    else if(std::tolower((char) S7200Address.at(1)) == 'w'){
        return S7WLWord;
    }
    else if(std::tolower((char) S7200Address.at(1)) == 'd'){
        return S7WLReal; //e.g. VD124 GLB.CAL.GANA1
    }
    else{  //e.g.: V255.3
        return S7WLBit;
    }
    return 0; //dummy
}

int S7200LibFacade::S7200AddressGetStart(std::string S7200Address)
{
    if(S7200Address.length() < 2){
        return -1; //invalid
    }
    else if(std::tolower((char) S7200Address.at(1)) == 'b' || std::tolower((char) S7200Address.at(1)) == 'w' || std::tolower((char) S7200Address.at(1)) == 'd'){ //Addesses like XX9999
        if(S7200Address.find_first_of('.')  == std::string::npos){
            return (int) std::stoi(S7200Address.substr(2)); //e.g.:VB2978
        }
        else{
            return (int) std::stoi(S7200Address.substr(2, S7200Address.find_first_of('.')-1)); //e.g.:VB2978.20
        }
    }
    else{ //Addesses like X9999
        if(S7200Address.find_first_of('.')  == std::string::npos){
            return -1; //invalid
        }
        else{
            return (int) std::stoi(S7200Address.substr(1, S7200Address.find_first_of('.')-1)); //e.g.: V255.3
        }
    }
  return 0; //dummy
}

int S7200LibFacade::S7200AddressGetArea(std::string S7200Address)
{
    if(S7200Address.length() < 2){
        return -1; //invalid
    }
    else if(std::tolower((char) S7200Address.at(0)) == 'v'){ //Data Blocks
        return S7AreaDB;
    }
    else if(std::tolower((char) S7200Address.at(0)) == 'i' || std::tolower((char) S7200Address.at(0)) == 'e'){ //Inputs
        return S7AreaPE;
    }
    else if(std::tolower((char) S7200Address.at(0)) == 'q' || std::tolower((char) S7200Address.at(0)) == 'a'){ //Outputs
        return S7AreaPA;
    }
    else if(std::tolower((char) S7200Address.at(0)) == 'm' || std::tolower((char) S7200Address.at(0)) == 'f'){ //Flag memory
        return S7AreaMK;
    }
    else if(std::tolower((char) S7200Address.at(0)) == 't'){ //Timers
        return S7AreaTM;
    }
    else if(std::tolower((char) S7200Address.at(0)) == 'c' || std::tolower((char) S7200Address.at(0)) == 'z'){ //Counters
        return S7AreaCT;
    }
    return -1; //invalid
}


bool S7200LibFacade::S7200AddressIsValid(std::string S7200Address){
    return S7200AddressGetArea(S7200Address)!=-1 && 
    S7200AddressGetWordLen(S7200Address)!=-1 &&
    S7200AddressGetAmount(S7200Address)!=-1 &&
    S7200AddressGetStart(S7200Address)!=-1;
}

int S7200LibFacade::S7200AddressGetAmount(std::string S7200Address)
{
    if(S7200Address.length() < 2){
        return -1; //invalid
    }
    else if(std::tolower((char) S7200Address.at(1)) == 'b'){ //VB can be words or strings if it contains a '.'
        if(S7200Address.find_first_of('.') != std::string::npos){
            return (int) std::stoi(S7200Address.substr(S7200Address.find('.')+1));
        }
        else{
            return 1;
        }
    }
    else if(std::tolower((char) S7200Address.at(1)) == 'w' || std::tolower((char) S7200Address.at(1)) == 'd'){
        return 1;
    }
    else{ //bit addressing like V255.3
        return 1;
    }
    return 0; //dummy
}

int S7200LibFacade::S7200AddressGetBit(std::string S7200Address)
{
    if(S7200Address.length() < 2){
        return -1; //invalid
    }
    else if(S7200AddressGetWordLen(S7200Address) == S7WLBit && S7200Address.find_first_of('.') != std::string::npos){
        return (int) std::stoi(S7200Address.substr(S7200Address.find('.')+1));
    }
    else{
        return 0; //N/A
    }
    return 0; //dummy
}

int S7200LibFacade::S7200DataSizeByte(int WordLength)
{
     switch (WordLength){
          case S7WLBit     : return 1;  // S7 sends 1 byte per bit
          case S7WLByte    : return 1;
          case S7WLWord    : return 2;
          case S7WLDWord   : return 4;
          case S7WLReal    : return 4;
          case S7WLCounter : return 2;
          case S7WLTimer   : return 2;
          default          : return 0;
     }
}

void S7200LibFacade::S7200DisplayTS7DataItem(PS7DataItem item)
{
    //hexdump(item->pdata  , sizeof(item->pdata));
    switch(item->WordLen){
        case S7WLByte:
            if(item->Amount>1){
                std::string strVal( reinterpret_cast<char const*>(item->pdata));
                //printf("-->read valus as string :'%s'\n", strVal.c_str());
            }
            else{
                uint8_t byteVal;
                std::memcpy(&byteVal, item->pdata  , sizeof(uint8_t));
                //printf("-->read value as byte : %d\n", byteVal);
            }
            break;
        case S7WLWord:
            uint16_t wordVal;
            std::memcpy(&wordVal, item->pdata  , sizeof(uint16_t));
            //printf("-->read value as word : %d\n", __bswap_16(wordVal));
            break;
        case S7WLReal:{
            float realVal;
            u_char f[] = { static_cast<byte*>(item->pdata)[3], static_cast<byte*>(item->pdata)[2], static_cast<byte*>(item->pdata)[1], static_cast<byte*>(item->pdata)[0]};
            std::memcpy(&realVal, f, sizeof(float));
            //printf("-->read value as real : %.3f\n", realVal);
            }
            break;
        case S7WLBit:
            uint8_t bitVal;
            std::memcpy(&bitVal, item->pdata  , sizeof(uint8_t));
            //printf("-->read value as bit : %d\n", bitVal);      
            break;
    }
}

TS7DataItem S7200LibFacade::S7200TS7DataItemFromAddress(std::string S7200Address){
    TS7DataItem item;
    item.Area     = S7200AddressGetArea(S7200Address);
    item.WordLen  = S7200AddressGetWordLen(S7200Address);
    item.DBNumber = 1;
    item.Start    = item.WordLen == S7WLBit ? (S7200AddressGetStart(S7200Address)*8)+S7200AddressGetBit(S7200Address) : S7200AddressGetStart(S7200Address);
    item.Amount   = S7200AddressGetAmount(S7200Address);
    item.pdata   = new char[S7200DataSizeByte(item.WordLen )*item.Amount];
    return item;
}

void S7200LibFacade::S7200MarkDeviceConnectionError(std::string ip, bool error_status){
    if(error_status)
        Common::Logger::globalInfo(Common::Logger::L1, "Request from LambdaThread: Writing true to DPE for PLC connection erorr for PLC IP : ", ip.c_str());
    else
        Common::Logger::globalInfo(Common::Logger::L1, "Request from LambdaThread: Writing false to DPE for PLC connection erorr for PLC IP : ", ip.c_str());
    
    TS7DataItem PLC_Conn_Stat_item = S7200TS7DataItemFromAddress("_Error");
    memcpy(PLC_Conn_Stat_item.pdata, &error_status , sizeof(bool));
    this->_consumeCB(ip, "_Error", "", reinterpret_cast<char*>(PLC_Conn_Stat_item.pdata));
}

float ReverseFloat( const float inFloat )
{
   float retVal;
   char *floatToConvert = ( char* ) & inFloat;
   char *returnFloat = ( char* ) & retVal;

   // swap the bytes into a temporary buffer
   returnFloat[0] = floatToConvert[3];
   returnFloat[1] = floatToConvert[2];
   returnFloat[2] = floatToConvert[1];
   returnFloat[3] = floatToConvert[0];

   return retVal;
}

void S7200LibFacade::S7200ReadWriteMaxN(std::vector <std::pair<std::string, void *>> validVars, uint N, int PDU_SZ, int VAR_OH, int MSG_OH, int rorw) {
    try{
        uint last_index = 0;
        uint to_send = 0;

        TS7DataItem *item = new TS7DataItem[validVars.size()];

        for(uint i = 0; i < validVars.size(); i++) {
           // Common::Logger::globalInfo(Common::Logger::L1,"Getting item with address", validVars[i].first.c_str());
            item[i] = S7200TS7DataItemFromAddress(validVars[i].first);
            
            if(rorw == 1) {
                int memSize = (S7200DataSizeByte(item[i].WordLen )*item[i].Amount);

                if(S7200DataSizeByte(item[i].WordLen) == 2) {
                    std::memcpy(item[i].pdata , validVars[i].second, sizeof(int16_t));
                } else if (S7200DataSizeByte(item[i].WordLen) == 4) {
                    std::memcpy(item[i].pdata , validVars[i].second, sizeof(float));
                } else {
                    //case string
                    std::memcpy(item[i].pdata, validVars[i].second, memSize);
                }
            }
        }

        int retOpt;

        int curr_sum;

        last_index = 0;
        while(last_index < validVars.size()) {
            to_send = 0;
            curr_sum = 0;
            
            uint i;
            for(i = last_index; i < validVars.size(); i++) {
                
                if( curr_sum + (((S7200DataSizeByte(item[i].WordLen)) * item[i].Amount) + VAR_OH) < ( PDU_SZ - MSG_OH ) ) {
                    to_send++;
                    curr_sum += ((S7200DataSizeByte(item[i].WordLen)) * item[i].Amount) + VAR_OH;
                } else{
                    break;
                }

                if(to_send == N) { //Request upto N variables
                    break;
                }
            }

            if(to_send == 0) {
                //This means that the current variable has a mem size > PDU. Call with ReadArea 
                //printf("To read a single variable with index %d and total size %d\n", last_index, ((S7200DataSizeByte(item[i].WordLen)) * item[i].Amount));
                to_send += 1;
                curr_sum = ((S7200DataSizeByte(item[i].WordLen)) * item[i].Amount) + VAR_OH + MSG_OH;

                if(rorw == 0)
                    retOpt = _client->ReadArea(item[last_index].Area, item[last_index].DBNumber, item[last_index].Start, item[last_index].Amount, item[last_index].WordLen, item[last_index].pdata);
                else
                    retOpt = _client->WriteArea(item[last_index].Area, item[last_index].DBNumber, item[last_index].Start, item[last_index].Amount, item[last_index].WordLen, item[last_index].pdata);

            } else {
                //printf("To read %d variables, total requesting incl overheads: %d\n", to_send, curr_sum+MSG_OH);
                //printf("To read variables from range %d to %d \n", last_index, last_index + to_send);

                if(rorw == 0)
                    retOpt = _client->ReadMultiVars(&(item[last_index]), to_send);
                else {
                    retOpt = _client->WriteMultiVars(&(item[last_index]), to_send);
                }
            }

            if( retOpt == 0) {
                //printf("Read/Write OK. ");
                //printf("Read/Write %d items\n", to_send);

                if(rorw == 0) {
                    Common::Logger::globalInfo(Common::Logger::L3, "Read OK");
                
                    for(uint i = last_index; i < last_index + to_send; i++) {
                        int a;
                        std::memcpy(&a, validVars[i].second, sizeof(int));    
                        this->_consumeCB(_ip, validVars[i].first, std::to_string(a), reinterpret_cast<char*>(item[i].pdata));
                    }
                } else {
                    Common::Logger::globalInfo(Common::Logger::L1, "Write OK");
                }
            }
            else{
                if(rorw == 0) {
                    //printf("-->Read NOK!, Tried to read %d elements with total requesting size: %d .retOpt is %d\n", to_send, curr_sum + MSG_OH, retOpt);
                    Common::Logger::globalInfo(Common::Logger::L1, "-->Read NOK");
                    readFailures++;
                }
                else {
                    //printf("-->Write NOK!, Tried to write %d elements with total requesting size: %d .retOpt is %d\n", to_send, curr_sum + MSG_OH, retOpt);
                    Common::Logger::globalInfo(Common::Logger::L1, "-->Write NOK");
                }
            }
            
            last_index += to_send;
        }

    }
    catch(std::exception& e){
        printf("Exception in read function\n");
        Common::Logger::globalWarning(__PRETTY_FUNCTION__," Read invalid. Encountered Exception.");
        //Common::Logger::globalError(e.what());
    }
}


int S7200LibFacade::getByteSizeFromAddress(std::string S7200Address)
{
    TS7DataItem item = S7200TS7DataItemFromAddress(S7200Address);
    return (S7200DataSizeByte(item.WordLen )*item.Amount);
}