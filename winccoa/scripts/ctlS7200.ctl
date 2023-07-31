/**
 * Library of functions used to manage the S7200 driver
 * @file   Drivers/ctlS7200.ctl
 * @author Adrien Ledeul, Richi Dubey
 * @date   10/07/2023
 * @modifications:
 *  -[author] [date] [object]
*/

/**
 * Called when a DPE connected to this driver is adressed (used to set the proper periph. address config)
 * @param address         address string
 * @param driverNum       driver manager number
 * @param path            path to the dpe
 * @param mode            adressing mode
 * @param exceptionInfo   exeption lists
 * @return 0 if OK, -1 if not
*/
const unsigned periphAddress_S7200_SUBINDEX = 7;


bool isBitOnPLC(const string var)
{
  if(strlen(var)<2)
    return false;
  string secondChar = substr(var, 1, 1);
  return patternMatch("[0123456789]", secondChar);
}

bool isByteOnPLC(const string var)
{
  return strlen(var)>2 &&  var[1] == 'B';
}

bool isWordOnPLC(const string var)
{
  return strlen(var)>2 && var[1] == 'W';
}

bool isDoubleWordOnPLC(const string var)
{
   return strlen(var)>2 &&  var[1] == 'D';
}

int getDataTypeTrans(const int dptype, const string var)
{
  if(dptype == DPEL_INT)
    return isBitOnPLC(var) ? 1000 : // bool
           isByteOnPLC(var) ? 1001 : // uint8
           isWordOnPLC(var) ? 1002 : // uint16
           isDoubleWordOnPLC(var) ? 1003 : // uint32
           1002; // default
  else if(dptype == DPEL_BOOL)
    return 1000; // bool
  else if(isWordOnPLC(var))
    return 1002; // uint16
  else if(dptype == DPEL_FLOAT)
    return 1004; // float
  else if(dptype == DPEL_STRING)
    return 1005; // string
  writeLog("INCOORECT TYPE!!!", "warning");
  return 1005; // default in all other cases
}

public int S7200_addressDPE(string dpName, string address, int driverNum, string path, int mode, dyn_string &exceptionInfo, string logsDPE="")
{
  dyn_anytype params;

  bool isConfig = false;
  bool isStatus = false;

  string ref = "";
  dyn_string MSDAFastItems = makeDynString("*.CH?.MES.*","*.CH?.SP.NAME","*.GLB.CFG.ACK","*.GLB.CFG.MSNAME","*.GLB.CFG.TYPE","*.GLB.MS.*","*.GLB.OPC.*","*.DIP.*");
  dyn_string WMSFastItems = makeDynString("*.CH?.CMD.ACK","*.CH?.MES.*","*.CH?.SP.NAME","*.GLB.CFG.COMVALID","*.GLB.CFG.MSNAME","*.GLB.CFG.PAN","*.GLB.CFG.ST","*.GLB.CFG.TYPE","*.GLB.PROCESS.PUMPSTARTDELAY","*.GLB.PROCESS.ST1","*.GLB.OPC.*");
  dyn_string VMSFastItems = makeDynString("*.Commands.AlarmAcks","*.GLB.OPC.COM_ERROR","*.GLB.OPC.ENABLED","*.MS.Configuration.ConfValid","*.MS.DeviceTypeId","*.MS.FaultStatus","*.MS.Name","*.MS.PanelStatus","*.VAS.Measure.Date","*.VAS.Measure.Time","*.VAS.Measure.Value","*.VAS.Name","*.VAS.PumpOnTime","*.VAS.PumpStatus","*.VAS.Status1","*.VAS.Status2","*.VAS.WorkingMode","*.VFR.VFR1.Measure.Date","*.VFR.VFR1.Measure.Flow","*.VFR.VFR1.Measure.Time","*.VFR.VFR1.Measure.Value","*.VFR.VFR1.Name","*.VFR.VFR1.Status1","*.VFR.VFR1.VentilationOn","*.VFR.VFR2.Measure.Flow","*.VFR.VFR2.Measure.Value","*.VFR.VFR2.Status1","*.VFR.VFR2.VentilationOn","*.VGM.Measure.Date","*.VGM.Measure.Time","*.VGM.Measure.Value","*.VGM.Name","*.VGM.ReleaseActivity","*.VGM.Status1","*.VGM.Status2");
  dyn_string MMSFastItems = makeDynString("*.CH1.NAME","*.CH1.VALUE","*.CH2.NAME","*.CH2.VALUE","*.CH3.NAME","*.CH3.VALUE","*.CH4.NAME","*.CH4.VALUE","*.CH5.NAME","*.CH5.VALUE","*.CH6.NAME","*.CH6.VALUE","*.CH7.NAME","*.CH7.VALUE","*.GLB.CFG.COMVALID","*.GLB.MS.220V","*.GLB.MS.BATTERY","*.GLB.MS.CHARGER","*.GLB.MS.COMMUNICATION","*.GLB.MS.COMPSTATUS","*.GLB.CMD.MODE","*.GLB.MS.MSNAME","*.GLB.MS.NTP","*.GLB.MS.PANELFAULT","*.GLB.MS.TYPE","*.GLB.OPC.COM_ERROR","*.GLB.OPC.ENABLED","*.MES.CHSTATUS","*.MES.DATE","*.MES.TIME""*.CH1.NAME","*.CH1.VALUE","*.CH2.NAME","*.CH2.VALUE","*.CH3.NAME","*.CH3.VALUE","*.CH4.NAME","*.CH4.VALUE","*.CH5.NAME","*.CH5.VALUE","*.CH6.NAME","*.CH6.VALUE","*.CH7.NAME","*.CH7.VALUE","*.GLB.CFG.COMVALID","*.GLB.MS.220V","*.GLB.MS.BATTERY","*.GLB.MS.CHARGER","*.GLB.MS.COMMUNICATION","*.GLB.MS.COMPSTATUS","*.GLB.MS.MSNAME","*.GLB.MS.NTP","*.GLB.MS.PANELFAULT","*.GLB.MS.TYPE","*.GLB.OPC.COM_ERROR","*.GLB.OPC.ENABLED","*.GLB.PARAM.PARVALID","*.MES.CHSTATUS","*.MES.DATE","*.MES.TIME");

  try
  {
    if(patternMatch("*_system._Error*", address))
      isStatus = true;
    else if(patternMatch("*_system*", address))
      return 0; //Special OPC addresses, ignore
    else if(patternMatch("CONFIG*", dpName))
      isConfig = true;
    else if(patternMatch("*touchConError*", address))
      isStatus = true;
    else if(patternMatch("*PANEL_IP*", address)){
       return 0; //Special RAMS7200 address, ignore
    }

    //writeLog( "DP Name is " + dpName + "Address is "+address + "Path is : " + path, "info");

    params[fwPeriphAddress_DRIVER_NUMBER] = driverNum;
    params[fwPeriphAddress_DIRECTION]= mode;
    params[fwPeriphAddress_ACTIVE] = true;
    params[periphAddress_S7200_SUBINDEX] = 0;

    string pollingTimeFast;
    string oldAddress;
    string poolingTime;
    string configVar;
    string var;
    string PLCIP;
    string panelIP;
    string addressesTmp, addressesTmp2;
    dyn_string tempDynString = makeDynString();
    bool isFast;

    pollingTimeFast =  substr(address, 0, strpos(address, "$"));
    oldAddress = substr(address, strpos(address, "$")+1);

    address = oldAddress;


    addressesTmp = address;
    poolingTime = substr(addressesTmp, 0, strpos(addressesTmp, "$"));
    addressesTmp2 = substr(addressesTmp, strpos(addressesTmp, "$")+1);
    //writeLog( "Faster polling time is " + pollingTimeFast + "Slower is " +poolingTime , "info");

    isFast = patternMatch("MSDA*", dpTypeName(dpName)) ? (multiPatternMatch(path, MSDAFastItems) ? true : false) :
                         patternMatch("WMS*", dpTypeName(dpName)) ?  (multiPatternMatch(path, WMSFastItems) ? true : false) :
                         patternMatch("VMS*", dpTypeName(dpName))?  (multiPatternMatch(path, VMSFastItems) ? true : false) :
                         patternMatch("MMS*", dpTypeName(dpName))?  (multiPatternMatch(path, MMSFastItems) ? true : false) : false;


    if(isFast) {
      poolingTime = pollingTimeFast;
      //writeLog( "Yes, this is a fast item. Final polling time is " + poolingTime, "info");
    }
    if(isConfig){ //Config address e.g. "2$DEBUGLVL"; "2$VERSION"
      configVar =  addressesTmp2;
    }
    else{ //Regular address e.g. "2$172.18.130.170;172.18.130.39.VW304"; "2$172.18.130.170;172.18.130.39.E304.1"; "2$172.18.130.170;172.18.130.39.touchConError"
      tempDynString = strsplit(addressesTmp2, ".");
      if(dynlen(tempDynString) > 2){
        var = tempDynString[dynlen(tempDynString)];
        if(patternMatch("[0123456789]", substr(var,0,1))){
          var = tempDynString[dynlen(tempDynString)-1] + "." + var;
        }
      }

      addressesTmp = substr(addressesTmp2, 0, strlen(addressesTmp2) - strlen(var) - 1); //contain 172.18.130.170;172.18.130.39 or 172.18.130.170

      tempDynString = strsplit(addressesTmp, ";");
      if(dynlen(tempDynString)>1){
        PLCIP = tempDynString[1];
        panelIP = tempDynString[2];
      }
      else{
        PLCIP = addressesTmp;
        writeLog("Error in S7200_addressDPE: Incorrect address format, format should be: 'PLC_IP;PANEL_IP'", LOG_LEVEL_ERROR, logsDPE);
        return -1;
      }
    }

    if(isConfig) {
      ref = configVar; //e.g. VERSION or DEBUGLVL
    } else {
      if(isStatus) {
        ref = PLCIP + "$" + var;    //E.g. IPADDRESS$touchConError or IPADDRESS$._Error
      } else {
        ref = PLCIP + "$" + var + "$" + poolingTime; //last param is polling time comign from driver address prefix
      }
    }

    int type = getDataTypeTrans(dpElementType(path),var);
    writeLog("--->" + path + ":" + PLCIP + ":" + var+ ":" + type + ":a-:"+ address, "info");

    if(dpElementType(path))
    params[fwPeriphAddress_DATATYPE] = type;
    params[fwPeriphAddress_REFERENCE] = ref;
    //writeLog( "Invalid address for DPE " + dpName + " won't be addressed. Full address string: " + address, LOG_LEVEL_ERROR, logsDPE);


    int ret = S7200_PeriphAddress_set(path, params, exceptionInfo);

    if(ret != 0) {
        writeLog("Caught:  Error in executing S7200_PeriphAddress_set: Most likely due to a duplicate address", LOG_LEVEL_ERROR, logsDPE);
      return -1;
    }

  }
  catch
  {
    writeLog("Uncaught exception in S7200_addressDPE: " + getLastException(), LOG_LEVEL_ERROR, logsDPE);
    return -1;
  }
  return 0;
}

/**
 * Called when a DPE connected to this driver is unadressed (used to unset the periph. address if necessary)
 * @param path            path to the dpe
 * @param exceptionInfo   exeption lists
 * @return 0 if OK, -1 if not
*/
public int S7200_unaddressDPE(string path, dyn_string &exceptionInfo)
{
//  Nothing to do here.
  return 0;
}

/**
 * Called when a device DP is about to be deleted (used to kill the device thread on the driver if necessary)
 * @param dp              dataPoint name
 * @param exceptionInfo   exeption lists
 * @return 0 if OK, -1 if not
*/
public int S7200_disconnectDeviceDP(string dp, dyn_string &exceptionInfo)
{
    return 0;
}

/**
 * Called when the manager of the driver start or stop (used to repush the data to the driver if necessary)
 * @param manRunning     TRUE: Manager has started, FALSE: Manager has stopped
 * @param manNum         manager number
 * @param exceptionInfo   exeption lists
 * @return 0 if OK, -1 if not
*/
public int S7200_managerStateChanged(bool manRunning, int manNum, dyn_string &exceptionInfo)
{
  try
  {
    if(isSystemActive())
    {
//      Repush Data to S7200 device
      bool driverRunning = false;
      int dataCnt = 0;

//      Generate event
      if(manRunning)
        insertEvent(EVENT_TYPE_ORDINARY_EVENT, manNum, 2004, "", MANAGER_SOURCE_TYPE, getCurrentTime(), 0, myReduHost(), 6);
      else
        insertEvent(EVENT_TYPE_ORDINARY_EVENT, manNum, 2005, "", MANAGER_SOURCE_TYPE, getCurrentTime(), 0, myReduHost(), 6);

//      Push the data
      fwPeriphAddress_checkIsDriverRunning(manNum, driverRunning, exceptionInfo);
      if(driverRunning)
      {
//        Avoid sync problems with driver states
        delay(0,500);

        writeLog("Driver Restart: System is Active and S7200 Driver is Running, repushing Data to the S7200 Driver", "info");

        dataCnt = pushS7200Data(manNum);

        writeLog("Driver Restart: "  + dataCnt  + " Data have been pushed to the S7200 Driver", "info");
      }
    }
  }
  catch
  {
    writeLog("Uncaught exception in S7200_managerStateChanged: " + getLastException(), LOG_LEVEL_ERROR);
    return -1;
  }
  return 0;
}


/**
 * Called when the primary server changed (used to repush the data to the driver if necessary)
 * @param primary         TRUE: Server beame primary, FALSE: Server beame secondary
 * @param manNum          manager number
 * @param exceptionInfo   exeption lists
 * @return 0 if OK, -1 if not
*/
public int S7200_redundancyChanged(bool primary, int manNum, dyn_string &exceptionInfo)
{
  try
  {
    if(primary)
    {
//      Repush Data to S7200 device
      bool driverRunning = false;
      int dataCnt = 0;

      fwPeriphAddress_checkIsDriverRunning(manNum, driverRunning, exceptionInfo);
      if(driverRunning)
      {
///        Avoid sync problems with driver states
        delay(0,500);

        writeLog("Redundancy Change: System is Active and S7200 Driver is Running, repushing Data to the S7200 Driver", "info");

        dataCnt = pushS7200Data(manNum);

        writeLog("Redundancy Change: "  + dataCnt  + " Data have been pushed to the S7200 Driver", "info");
      }
    }
  }
  catch
  {
    writeLog("Uncaught exception in S7200_redundancyChanged: " + getLastException(), LOG_LEVEL_ERROR);
    return -1;
  }
  return 0;
}

/**
 * Called right before the deletion of a device DP
 * @param dp                      dataPoint Name
 * @param exceptionInfo   exeption lists
 * @return 0 if OK, -1 if not
*/
public int S7200_beforeDPDelete(string dp, dyn_string &exception_info){
  // Dummy
  return 0;
}

/**
 * Called after all the DPEs of the DP have been addressed
 * @param dp                      dataPoint Name
 * @param exceptionInfo   exeption lists
 * @return 0 if OK, -1 if not
*/
public int S7200_afterAllDPEAdressed(string dp, dyn_string &exceptionInfo){
  // Dummy
  writeLog("All DPEs have been addressed for DP:" + dp, "info");
  //delay(5,0);
  //string path = dp+".GLB.CFG.PANELIP";
 /// string panelIP;
  ///dpGet(path, panelIP);
  ///writeLog("Read value from PANEL IP DPE: "+panelIP, "info");
  //dpSetWait(path, panelIP);
  return 0;
}

/**
 * Called after all the DPE functions  of the DP have been set
 * @param dp                      dataPoint Name
 * @param exceptionInfo   exeption lists
 * @return 0 if OK, -1 if not
*/
public int S7200_afterAllDPEFctSet(string dp, dyn_string &exceptionInfo){
///   Dummy
  return 0;
}

/**
 * Push the data contains in DPE to the driver
 * @return number of varables pushed
*/
private int pushS7200Data(int manNum)
{
  int dataCnt;
  dyn_string configDp = dpNames("*", "CONFIG_S7200");
  dyn_string WMSDp = dpNames("*", "WMS");
  dyn_string MSDAp = dpNames("*", "MSDA2");
  dyn_string VMSDp = dpNames("*", "VMS");
  dyn_string MMSDp = dpNames("*", "MMS");


  dyn_string dpe = makeDynString();
  dyn_mixed values = makeDynMixed();

////   Push all CONFIG data
  for(int i = 1; i <= dynlen(configDp); ++i)
  {
    dpe = dpElementsWithPattern(configDp[i], "*");
    if(dynlen(dpe) > 0)
    {
      if(manNum == getDriverManagerNumber(dpe)){
        writeLog("Pushing config data for S7200 Driver #" + manNum, "info");
        dpGet(dpe, values);
        dpSetWait(dpe, values);
        dataCnt += dynlen(dpe);
        delay(0,100);
      }
    }
  }

  // Selective push on everything else
    for(int i = 1; i <= dynlen(MMSDp); ++i)
    {
      values = makeDynMixed();
      dpe = makeDynString();

      dynAppend(dpe, makeDynString(MMSDp[i]+ ".GLB.CFG.PANELIP"));

      if(dynlen(dpe) > 0){
        if(manNum == getDriverManagerNumber(dpe)){
          writeLog("Pushing device data for MMS Driver #" + manNum + " device:" + MMSDp[i], "info");
          dpGet(dpe, values);
          dpSetWait(dpe, values);
          dataCnt += dynlen(dpe);
        }
      }
     }

    for(int i = 1; i <= dynlen(VMSDp); ++i)
    {
      values = makeDynMixed();
      dpe = makeDynString();

      dynAppend(dpe, makeDynString(VMSDp[i]+ ".GLB.CFG.PANELIP"));

      if(dynlen(dpe) > 0){
        if(manNum == getDriverManagerNumber(dpe)){
          writeLog("Pushing device data for VMS Driver #" + manNum + " device:" + VMSDp[i], "info");
          dpGet(dpe, values);
          dpSetWait(dpe, values);
          dataCnt += dynlen(dpe);
        }
      }
     }

    for(int i = 1; i <= dynlen(MSDAp); ++i)
    {
      values = makeDynMixed();
      dpe = makeDynString();

      dynAppend(dpe, makeDynString(MSDAp[i]+ ".GLB.CFG.PANELIP"));

      if(dynlen(dpe) > 0){
        if(manNum == getDriverManagerNumber(dpe)){
          writeLog("Pushing device data for MSDA Driver #" + manNum + " device:" + MSDAp[i], "info");
          dpGet(dpe, values);
          dpSetWait(dpe, values);
          dataCnt += dynlen(dpe);
        }
      }
     }

    for(int i = 1; i <= dynlen(WMSDp); ++i)
    {
      values = makeDynMixed();
      dpe = makeDynString();

      dynAppend(dpe, makeDynString(WMSDp[i]+ ".GLB.CFG.PANELIP"));

      if(dynlen(dpe) > 0){
        if(manNum == getDriverManagerNumber(dpe)){
          writeLog("Pushing device data for WMS Driver #" + manNum + " device:" + WMSDp[i], "info");
          dpGet(dpe, values);
          dpSetWait(dpe, values);
          dataCnt += dynlen(dpe);
        }
      }
   }

  return dataCnt;
 }



/**
  * Method setting addressing fro S7200 devices datapoints elements
  * @param datapoint element for which address will be set
  * @param configuration parameteres
  * @param parameter to return exceptions values
  */
private int S7200_PeriphAddress_set(string dpe, dyn_anytype configParameters, dyn_string& exceptionInfo){

  int i = 1;

  dyn_string names;
  dyn_anytype values;

/////	The driver will already have been checked to see that it is running, so just dpSetWait
	dpSetWait(dpe + ":_distrib.._type", DPCONFIG_DISTRIBUTION_INFO,
						dpe + ":_distrib.._driver", configParameters[fwPeriphAddress_DRIVER_NUMBER] );
	dyn_string errors = getLastError();
  if(dynlen(errors) > 0){
		throwError(errors);
		fwException_raise(exceptionInfo, "ERROR", "Could not create the distrib config.", "");
		return;
	}

  names[i] = dpe + ":_address.._type";
  values[i++] = DPCONFIG_PERIPH_ADDR_MAIN;
  names[i] = dpe + ":_address.._drv_ident";
  values[i++] = "S7200";
  names[i] = dpe + ":_address.._reference";
  values[i++] = configParameters[fwPeriphAddress_REFERENCE];
  names[i] = dpe + ":_address.._mode";
  values[i++] = configParameters[fwPeriphAddress_DIRECTION];
  names[i] = dpe + ":_address.._datatype";
  values[i++] = configParameters[fwPeriphAddress_DATATYPE];
  names[i] = dpe + ":_address.._subindex";
  values[i++] = configParameters[periphAddress_S7200_SUBINDEX];

  writeLog("Periph Address Set Wait Started", "info");
  int ret = dpSetWait(names, values);

  if(ret!=0) {
    writeLog("Periph Address Set Wait Failed. Error is : " + getLastError(), "info");
    return -1;
  }

  writeLog("Periph Address Set Wait Finished", "info");
  return 0;

}
