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

#include <csignal>

#include "S7200LibFacade.hxx"
#include "Common/Constants.hxx"
#include "Common/Logger.hxx"

#include <vector>

S7200LibFacade::S7200LibFacade(const std::string& ip, consumeCallbackConsumer cb, errorCallbackConsumer erc = nullptr)
    : _ip(ip), _consumeCB(cb), _errorCB(erc)
{
    Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Connecting to '", ip.c_str());

    try{
        _client = new TS7Client();

        _client->SetConnectionParams(ip.c_str(), Common::Constants::getLocalTsapPort(), Common::Constants::getRemoteTsapPort());
        int res = _client->Connect();


        if (res==0) {
            Common::Logger::globalInfo(Common::Logger::L1,__PRETTY_FUNCTION__, "Connected to '", ip.c_str());
            //printf("  PDU Requested  : %d bytes\n",Client->PDURequested());
            //printf("  PDU Negotiated : %d bytes\n",Client->PDULength());
            _initialized = true;
        }
    }
    catch(std::exception& e)
    {
        Common::Logger::globalWarning("Unable to initialize connection!", e.what());
    }
}

void S7200LibFacade::poll(std::unordered_set<std::string>& vars)
{
    _client->PDULength();

    for (const auto& var : vars){
        if(S7200AddressIsValid(var)){
            char buffer[256];
            TS7DataItem item = S7200Read(var, buffer);
            this->_consumeCB(_ip, var, reinterpret_cast<char*>(item.pdata));
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
        return S7WLReal;
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
    item.pdata   = malloc(S7200DataSizeByte(item.WordLen )*item.Amount);
    return item;
}

TS7DataItem S7200LibFacade::S7200Read(std::string S7200Address, void* val)
{
    try{
        TS7DataItem item = S7200TS7DataItemFromAddress(S7200Address);
        int memSize = (S7200DataSizeByte(item.WordLen )*item.Amount);

        //printf("-------------read S7200Address=>(Area, Start, WordLen, Amount): memSize : %s =>(%d, %d, %d, %d) : %dB\n", S7200Address.c_str(), item.Area, item.Start, item.WordLen, item.Amount, memSize);
        
        if(_client->ReadMultiVars(&item, 1) == 0){
            if(item.WordLen == S7WLWord){
                uint16_t wordVal;
                std::memcpy(&wordVal, item.pdata, memSize);
                wordVal = __bswap_16(wordVal);
                std::memcpy(val, &wordVal, memSize);
            }
            else{
                std::memcpy(val, item.pdata, memSize);
            }
            //S7200DisplayTS7DataItem(&item);
        }
        else{
        //printf("-->read NOK!\n");
        }
        return item;
    }
    catch(std::exception& e){
        Common::Logger::globalWarning(__PRETTY_FUNCTION__," Read invalid", S7200Address.c_str());
        Common::Logger::globalError(e.what());
    }
}

int S7200LibFacade::getByteSizeFromAddress(std::string S7200Address)
{
    TS7DataItem item = S7200TS7DataItemFromAddress(S7200Address);
    return (S7200DataSizeByte(item.WordLen )*item.Amount);
}

TS7DataItem S7200LibFacade::S7200Write(std::string S7200Address, void* val)
{
    try{
        TS7DataItem item = S7200TS7DataItemFromAddress(S7200Address);
        int memSize = (S7200DataSizeByte(item.WordLen )*item.Amount);
        if(item.WordLen == S7WLWord){
            uint16_t wordVal;
            std::memcpy(&wordVal, val, memSize);
            wordVal = __bswap_16(wordVal);
            std::memcpy(item.pdata , &wordVal , memSize);
        }
        else{
            std::memcpy(item.pdata , val , memSize);
        }
        //printf("-------------write S7200Address=>(Area, Start, WordLen, Amount): memSize : %s =>(%d, %d, %d, %d) : %dB\n", S7200Address.c_str(), item.Area, item.Start, item.WordLen, item.Amount, memSize);
        if(_client->WriteMultiVars(&item, 1) == 0){
            //printf("-->write OK!\n");
        }
        else{
            //printf("-->write NOK!\n");
        }
        return item;
    }
    catch(std::exception& e){
        Common::Logger::globalWarning(__PRETTY_FUNCTION__," Write invalid", S7200Address.c_str());
        Common::Logger::globalError(e.what());
    }
}