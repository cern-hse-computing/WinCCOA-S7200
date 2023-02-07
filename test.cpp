/*=============================================================================|
|  PROJECT SNAP7                                                         1.4.0 |
|==============================================================================|
|  Copyright (C) 2013, 2014 Davide Nardella                                    |
|  All rights reserved.                                                        |
|==============================================================================|
|  SNAP7 is free software: you can redistribute it and/or modify               |
|  it under the terms of the Lesser GNU General Public License as published by |
|  the Free Software Foundation, either version 3 of the License, or           |
|  (at your option) any later version.                                         |
|                                                                              |
|  It means that you can distribute your commercial software linked with       |
|  SNAP7 without the requirement to distribute the source code of your         |
|  application and without the requirement that your application be itself     |
|  distributed under LGPL.                                                     |
|                                                                              |
|  SNAP7 is distributed in the hope that it will be useful,                    |
|  but WITHOUT ANY WARRANTY; without even the implied warranty of              |
|  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               |
|  Lesser GNU General Public License for more details.                         |
|                                                                              |
|  You should have received a copy of the GNU General Public License and a     |
|  copy of Lesser GNU General Public License along with Snap7.                 |
|  If not, see  http://www.gnu.org/licenses/                                   |
|==============================================================================|
|                                                                              |
|  Client Example                                                              |
|                                                                              |
|=============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include "snap7.h"

#ifdef OS_WINDOWS
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

    TS7Client *Client;

    byte Buffer[65536]; // 64 K buffer
    int SampleDBNum = 1000;

    char *Address;     // PLC IP Address
    int Rack=0,Slot=2; // Default Rack and Slot

    int ok = 0; // Number of test pass
    int ko = 0; // Number of test failure

    bool JobDone=false;
    int JobResult=0;

//------------------------------------------------------------------------------
//  Async completion callback 
//------------------------------------------------------------------------------
// This is a simply text demo, we use callback only to set an internal flag...
void S7API CliCompletion(void *usrPtr, int opCode, int opResult)
{
    JobResult=opResult;
    JobDone = true;
}

//------------------------------------------------------------------------------
//  Usage Syntax
//------------------------------------------------------------------------------
void Usage()
{
    printf("Usage\n");
    printf("  client <IP> [Rack=0 Slot=2]\n");
    printf("Example\n");
    printf("  client 192.168.1.101 0 2\n");
    printf("or\n");
    printf("  client 192.168.1.101\n");
    getchar();
}
//------------------------------------------------------------------------------
// hexdump, a very nice function, it's not mine.
// I found it on the net somewhere some time ago... thanks to the author ;-)
//------------------------------------------------------------------------------
#ifndef HEXDUMP_COLS
#define HEXDUMP_COLS 16
#endif
void hexdump(void *mem, unsigned int len)
{
        unsigned int i, j;

        for(i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++)
        {
                /* print offset */
                if(i % HEXDUMP_COLS == 0)
                {
                        printf("0x%04x: ", i);
                }

                /* print hex data */
                if(i < len)
                {
                        printf("%02x ", 0xFF & ((char*)mem)[i]);
                }
                else /* end of block, just aligning for ASCII dump */
                {
                        printf("   ");
                }

                /* print ASCII dump */
                if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
                {
                        for(j = i - (HEXDUMP_COLS - 1); j <= i; j++)
                        {
                                if(j >= len) /* end of block, not really printing */
                                {
                                        putchar(' ');
                                }
                                else if(isprint((((char*)mem)[j] & 0x7F))) /* printable char */
                                {
                                        putchar(0xFF & ((char*)mem)[j]);
                                }
                                else /* other char */
                                {
                                        putchar('.');
                                }
                        }
                        putchar('\n');
                }
        }
}
//------------------------------------------------------------------------------
// Check error
//------------------------------------------------------------------------------
bool Check(int Result, const char * function)
{
    printf("\n");
    printf("+-----------------------------------------------------\n");
    printf("| %s\n",function);
    printf("+-----------------------------------------------------\n");
    if (Result==0) {
        printf("| Result         : OK\n");
        printf("| Execution time : %d ms\n",Client->ExecTime());
        printf("+-----------------------------------------------------\n");
        ok++;
    }
    else {
        printf("| ERROR !!! \n");
        if (Result<0)
            printf("| Library Error (-1)\n");
        else
            printf("| %s\n",CliErrorText(Result).c_str());
        printf("+-----------------------------------------------------\n");
        ko++;
    }
    return Result==0;
}

int S7200AddressGetWordLen(std::string S7200Address)
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

int S7200AddressGetStart(std::string S7200Address)
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

int S7200AddressGetArea(std::string S7200Address)
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

int S7200AddressGetAmount(std::string S7200Address)
{
    if(S7200Address.length() < 2){
        return 0;
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

int S7200AddressGetBit(std::string S7200Address)
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

int S7200DataSizeByte(int WordLength)
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

void S7200DisplayTS7DataItem(PS7DataItem item)
{
    //hexdump(item->pdata  , sizeof(item->pdata));
    switch(item->WordLen){
        case S7WLByte:
            if(item->Amount>1){
                std::string strVal( reinterpret_cast<char const*>(item->pdata));
                printf("-->read valus as string :'%s'\n", strVal.c_str());
            }
            else{
                uint8_t byteVal;
                std::memcpy(&byteVal, item->pdata  , sizeof(uint8_t));
                printf("-->read value as byte : %d\n", byteVal);
            }
            break;
        case S7WLWord:
            uint16_t wordVal;
            std::memcpy(&wordVal, item->pdata  , sizeof(uint16_t));
            printf("-->read value as word : %d\n", __bswap_16(wordVal));
            break;
        case S7WLReal:{
                float realVal;
                u_char f[] = { static_cast<byte*>(item->pdata)[3], static_cast<byte*>(item->pdata)[2], static_cast<byte*>(item->pdata)[1], static_cast<byte*>(item->pdata)[0]};
                std::memcpy(&realVal, f, sizeof(float));
                printf("-->read value as real : %.3f\n", realVal);
            }
            break;
        case S7WLBit:
            uint8_t bitVal;
            std::memcpy(&bitVal, item->pdata  , sizeof(uint8_t));
            printf("-->read value as bit : %d\n", bitVal);      
            break;
    }
}

TS7DataItem S7200TS7DataItemFromAddress(std::string S7200Address){
    TS7DataItem item;
    item.Area     = S7200AddressGetArea(S7200Address);
    item.WordLen  = S7200AddressGetWordLen(S7200Address);
    item.DBNumber = 1;
    item.Start    = item.WordLen == S7WLBit ? (S7200AddressGetStart(S7200Address)*8)+S7200AddressGetBit(S7200Address) : S7200AddressGetStart(S7200Address);
    item.Amount   = S7200AddressGetAmount(S7200Address);
    item.pdata   = malloc(S7200DataSizeByte(item.WordLen )*item.Amount);
    return item;
}

TS7DataItem S7200Read(std::string S7200Address, void* val)
{
    TS7DataItem item = S7200TS7DataItemFromAddress(S7200Address);
    int memSize = (S7200DataSizeByte(item.WordLen )*item.Amount);

    printf("-------------read S7200Address=>(Area, Start, WordLen, Amount): memSize : %s =>(%d, %d, %d, %d) : %dB", S7200Address.c_str(), item.Area, item.Start, item.WordLen, item.Amount, memSize);
    
    if(Client->ReadMultiVars(&item, 1) == 0){
        if(item.WordLen == S7WLWord){
            uint16_t wordVal;
            std::memcpy(&wordVal, item.pdata, memSize);
            wordVal = __bswap_16(wordVal);
            std::memcpy(val, &wordVal, memSize);
        }
        else{
            std::memcpy(val, item.pdata, memSize);
        }
        S7200DisplayTS7DataItem(&item);
    }
    else{
        printf("-->read NOK!\n");
    }
    return item;
}

TS7DataItem S7200Write(std::string S7200Address, void* val)
{
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
    printf("-------------write S7200Address=>(Area, Start, WordLen, Amount): memSize : %s =>(%d, %d, %d, %d) : %dB", S7200Address.c_str(), item.Area, item.Start, item.WordLen, item.Amount, memSize);
    if(Client->WriteMultiVars(&item, 1) == 0){
        printf("-->write OK!\n");
    }
    else{
        printf("-->write NOK!\n");
    }
    return item;
}




//------------------------------------------------------------------------------
// Unit Connection
//------------------------------------------------------------------------------
bool CliConnect()
{
    Client->SetConnectionParams(Address, 0x1001, 0x1000);
    int res = Client->Connect();
    if (Check(res,"UNIT Connection")) {
          printf("  Connected to   : %s (Rack=%d, Slot=%d)\n",Address,Rack,Slot);
          printf("  PDU Requested  : %d bytes\n",Client->PDURequested());
          printf("  PDU Negotiated : %d bytes\n",Client->PDULength());
    };
    return res==0;
}
//------------------------------------------------------------------------------
// Unit Disconnection
//------------------------------------------------------------------------------
void CliDisconnect()
{
     Client->Disconnect();
}
//------------------------------------------------------------------------------
// Perform readonly tests, no cpu status modification
//------------------------------------------------------------------------------
void PerformTests()
{
    std::string addresses[] = {
        "VW1984", //VW1984=13220
        "VB2978.20", //VB2978.20='29 1-035'
        "VB1604", //VB1604=8
        "V1604.2", //V1604.2=0
        "V1604.3", //V1604.3=1
        "V2640.0",
        "V2640.02",
        "V2640.04",
        "V2640.06",
        "V2641.0",
        "V2641.02",
        "V2641.04",
        "V2641.06",
        "M10.0", //M10.0=1
        //"M10.1", //M10.1=0 Read-only?
        //"E0.0", //E0.0=1 Read-only?
        //"E1.1", //E0.0=0 Read-only?
        //"A0.2", //A0.2=1 Read-only?
        //"A0.3", //A0.3=0 Read-only?
        //"I0.2", //I0.2=1 Read-only?
        //"I0.3", //I0.3=0 Read-only?
        //"Q0.2", //Q0.2=1 Read-only?
        //"Q0.3" //Q0.3=0 Read-only?

    };

    for(std::string address : addresses){
        TS7DataItem item = S7200TS7DataItemFromAddress(address);
        switch (item.WordLen){
            case S7WLByte:
                if(item.Amount >1){
                    char valInitR[256], valInitW[256], valChangedW[256], valChangedR[256];
                    S7200Read(address, &valInitR);
                    std::memcpy(valChangedW , &valInitR , 256);
                    valChangedW[0] = valChangedW[0]+1;
                    S7200Write(address, &valChangedW);
                    S7200Read(address, &valChangedR);
                    Check(strcmp(valChangedW,valChangedR), ("IO :" + address).c_str());
                    std::memcpy(valInitW , &valInitR , 256);
                    S7200Write(address, &valInitW);
                    S7200Read(address, &valInitR);
                    Check(strcmp(valInitW,valInitR), ("IO re-init :" + address).c_str());
                    printf("valInitR: %s\n",valInitR);
                    printf("valInitW: %s\n",valInitW);
                    printf("valChangedW: %s\n",valChangedW);
                    printf("valChangedR: %s\n",valChangedR);
                }
                else{
                    uint8_t  valInitR, valInitW, valChangedW, valChangedR;
                    S7200Read(address, &valInitR);
                    valChangedW = valInitR == 1 ? 0 : 1;
                    S7200Write(address, &valChangedW);
                    S7200Read(address, &valChangedR);
                    Check((valChangedW==valChangedR) ? 0 : -1, ("IO :" + address).c_str());
                    valInitW = valInitR;
                    S7200Write(address, &valInitW);
                    S7200Read(address, &valInitR);
                    Check((valInitW==valInitR) ? 0 : -1, ("IO re-init :" + address).c_str());
                    printf("valInitR: %d\n",valInitR);
                    printf("valInitW: %d\n",valInitW);
                    printf("valChangedW: %d\n",valChangedW);
                    printf("valChangedR: %d\n",valChangedR);  
                }
                break;
            case S7WLBit:{
                    uint8_t  valInitR, valInitW, valChangedW, valChangedR;
                    S7200Read(address, &valInitR);
                    valChangedW = valInitR == 0x0001 ? 0x0000 : 0x0001;
                    S7200Write(address, &valChangedW);
                    S7200Read(address, &valChangedR);
                    Check((valChangedW==valChangedR) ? 0 : -1, ("IO :" + address).c_str());
                    valInitW = valInitR;
                    S7200Write(address, &valInitW);
                    S7200Read(address, &valInitR);
                    Check((valInitW==valInitR) ? 0 : -1, ("IO re-init :" + address).c_str());
                    printf("valInitR: %d\n",valInitR);
                      //hexdump(&valInitR , sizeof(valInitR));
                    printf("valInitW: %d\n",valInitW);
                      //hexdump(&valInitW , sizeof(valInitW));
                    printf("valChangedW: %d\n",valChangedW);
                      //hexdump(&valChangedW , sizeof(valChangedW));
                    printf("valChangedR: %d\n",valChangedR);
                      //hexdump(&valChangedR , sizeof(valChangedR));
                }
                break;
            case S7WLWord:{
                    uint16_t  valInitR, valInitW, valChangedW, valChangedR;
                    S7200Read(address, &valInitR);
                    valChangedW = valInitR == 1 ? 0 : 1;
                    S7200Write(address, &valChangedW);
                    S7200Read(address, &valChangedR);
                    Check((valChangedW==valChangedR) ? 0 : -1, ("IO :" + address).c_str());
                    valInitW = valInitR;
                    S7200Write(address, &valInitW);
                    S7200Read(address, &valInitR);
                    Check((valInitW==valInitR) ? 0 : -1, ("IO re-init :" + address).c_str());
                    printf("valInitR: %d\n",valInitR);
                    printf("valInitW: %d\n",valInitW);
                    printf("valChangedW: %d\n",valChangedW);
                    printf("valChangedR: %d\n",valChangedR);
                }
                break;
            case S7WLReal:{
                    float  valInitR, valInitW, valChangedW, valChangedR;
                    S7200Read(address, &valInitR);
                    valChangedW = valInitR == 1.0 ? 0.0 : 1.0;
                    S7200Write(address, &valChangedW);
                    S7200Read(address, &valChangedR);
                    Check((valChangedW==valChangedR) ? 0 : -1, ("IO :" + address).c_str());
                    valInitW = valInitR == 1 ? 0 : 1;
                    S7200Write(address, &valInitW);
                    S7200Read(address, &valInitR);
                    Check((valInitW==valInitR) ? 0 : -1, ("IO re-init :" + address).c_str());
                    printf("valInitR: %.3f\n",valInitR);
                    printf("valInitW: %.3f\\n",valInitW);
                    printf("valChangedW: %.3f\\n",valChangedW);
                    printf("valChangedR: %.3f\\n",valChangedR);
                }
                break;
            default:
                break;
        }
    }
}

//------------------------------------------------------------------------------
// Tests Summary
//------------------------------------------------------------------------------
void Summary()
{
    printf("\n");
    printf("+-----------------------------------------------------\n");
    printf("| Test Summary \n");
    printf("+-----------------------------------------------------\n");
    printf("| Performed : %d\n",(ok+ko));
    printf("| Passed    : %d\n",ok);
    printf("| Failed    : %d\n",ko);
    printf("+----------------------------------------[press a key]\n");
    getchar();
}
//------------------------------------------------------------------------------
// Main              
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
// Get Progran args (we need the client address and optionally Rack and Slot)  
    if (argc!=2 && argc!=4)
    {
        Usage();
        return 1;
    }
    Address=argv[1];
    if (argc==4)
    {
        Rack=atoi(argv[2]);
        Slot=atoi(argv[3]);
    }
    
// Client Creation
    Client= new TS7Client();
    Client->SetAsCallback(CliCompletion,NULL);

// Connection
    if (CliConnect())
    {
        PerformTests();
        CliDisconnect();
    };

// Deletion
    delete Client;
    Summary();

    return 0;
}


/*
void read(std::string S7200Address)
{
    // CHID1=WMS_172_18_130_170.WMS_172_18_130_170.VW1984=13220
    int addressArea = S7200AddressGetArea(S7200Address);
    int addressStart = S7200AddressGetStart(S7200Address);
    int addressWordLen = S7200AddressGetWordLen(S7200Address);
    int addressAmount = S7200AddressGetAmount(S7200Address);
    int addressBit = S7200AddressGetBit(S7200Address);
    byte mem[S7200DataSizeByte(addressWordLen)*addressAmount];

    printf("-------------read S7200Address=>(addressArea, addressStart, addressWordLen, addressAmount, addressBit) : %s =>(%d, %d, %d, %d, %d)\n", S7200Address.c_str(), addressArea, addressStart, addressWordLen, addressAmount, addressBit);

    int res;
    if(addressWordLen == S7WLBit){
        res=Client->ReadArea(addressArea, 1, (addressStart*8)+addressBit, addressAmount, addressWordLen, &mem);
    }
    else{
        res=Client->ReadArea(addressArea, 1, addressStart, addressAmount, addressWordLen, &mem);
    }
    //printf("read Result : %d\n", res);
    //printf("sizeof(mem) : %d\n", sizeof(mem));
    if (res==0){
        //printf("hexdump:\n");
        hexdump(mem, S7200DataSizeByte(addressWordLen)*addressAmount);
        u_char b[] = {mem[3],mem[2],mem[1],mem[0]};
        switch(addressWordLen){
            case S7WLByte:
                if(addressAmount>1){
                    std::string strVal( reinterpret_cast<char const*>(mem));
                    printf("---------------------------------------------->read valus as string :'%s'\n", strVal.c_str());
                }
                else{
                    uint8_t byteVal;
                    std::memcpy(&byteVal, mem, sizeof(mem));
                    printf("---------------------------------------------->read value as byte : %d\n", byteVal);
                }
                break;
            case S7WLWord:
                uint16_t wordVal;
                std::memcpy(&wordVal, mem, sizeof(mem));
                printf("---------------------------------------------->read value as word : %d\n", __bswap_16(wordVal));
                break;
            case S7WLReal:
                float realVal;
                std::memcpy(&realVal, b, sizeof(realVal));
                printf("---------------------------------------------->read value as real : %.3f\n", realVal);
                break;
            case S7WLBit:
                uint8_t bitVal;
                std::memcpy(&bitVal, mem, sizeof(mem));
                printf("---------------------------------------------->read value as bit : %d\n", bitVal);            
                break;
        }
    }
}
*/

//------------------------------------------------------------------------------
//  Read S7200
//------------------------------------------------------------------------------
/*
void ReadS7200()
{
    // CHID1=WMS_172_18_130_170.WMS_172_18_130_170.VW1984=13220
    uint16_t MB;
    int res=Client->ReadArea(S7AreaDB, 1, 1984, 1, S7WLWord, &MB);
    printf("Read VW1984 Result : %d\n",res);
    if (res==0){
        uint16_t MBswaped = (MB>>8) | (MB<<8);
        printf("Read VW1984 value : %X\n", MBswaped);
        printf("Read VW1984 value : %d\n", MBswaped);
    }
}
*/

//------------------------------------------------------------------------------
//  Write S7200
//------------------------------------------------------------------------------
/*
void WriteS7200()
{
    // CHID1=WMS_172_18_130_170.WMS_172_18_130_170.VW1984=13220
    uint16_t MB = 13220;
    uint16_t MBswaped = (MB>>8) | (MB<<8);
    int res=Client->WriteArea(S7AreaDB, 1, 1984, 1, S7WLWord, &MBswaped);
    printf("Write VW1984 Result : %d\n",res);
    if (res==0){
        printf("Write VW1984 value : %X\n", MB);
        printf("Write VW1984 value : %d\n", MB);
    }
}