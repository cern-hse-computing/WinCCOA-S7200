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

// Our transformation class PVSS <--> Hardware
#include "S7200StringTrans.hxx"
#include <ErrHdl.hxx>     // The Error handler Basics/Utilities


#include "S7200HWMapper.hxx"

#include "Common/Logger.hxx"

//----------------------------------------------------------------------------
namespace Transformations {


//S7200StringTrans::S7200StringTrans() : Transformation() { }

TransformationType S7200StringTrans::isA() const
{
  return (TransformationType) S7200DrvStringTransType;
}

TransformationType S7200StringTrans::isA(TransformationType type) const {
    if (type == isA())
        return type;
    else
        return Transformation::isA(type);
}



//----------------------------------------------------------------------------

Transformation *S7200StringTrans::clone() const
{
  return new S7200StringTrans;
}

//----------------------------------------------------------------------------
// Our item size. The max we will use is 256 Bytes.
// This is an arbitrary value! A Transformation for a long e.g. would return 4

int S7200StringTrans::itemSize() const
{
  // TODO - check maximum possible size
  return _size;
}

//----------------------------------------------------------------------------
// Our preferred Variable type. Data will be converted to this type
// before toPeriph is called.

VariableType S7200StringTrans::getVariableType() const
{
  return TEXT_VAR;
}

PVSSboolean S7200StringTrans::toPeriph(PVSSchar *buffer, PVSSuint len,
                                      const Variable &var, const PVSSuint subix) const
{

  // Be paranoic, check variable type
  if ( var.isA() != TEXT_VAR )
  {
    // Throw error message
    ErrHdl::error(
      ErrClass::PRIO_SEVERE,             // Data will be lost
      ErrClass::ERR_PARAM,               // Wrong parametrization
      ErrClass::UNEXPECTEDSTATE,         // Nothing else appropriate
      "S7200StringTrans", "toPeriph",     // File and function name
      "Wrong variable type for data"     // Unfortunately we don't know which DP
    );

    return PVSS_FALSE;
  }

  // Check data len. TextVar::getString returns a CharString
  const TextVar& tv = static_cast<const TextVar &>(var);
  if (len < tv.getString().len() + 1)
  {
    // Throw error message
    ErrHdl::error(
      ErrClass::PRIO_SEVERE,             // Data will be lost
      ErrClass::ERR_IMPL,                // Mus be implementation
      ErrClass::UNEXPECTEDSTATE,         // Nothing else appropriate
      "S7200StringTrans::toPeriph",       // File and function name
      "Data buffer too small; need:" +
      CharString(tv.getString().len() + 1) +
      " have:" + CharString(len)
    );

    return PVSS_FALSE;
  }

  if(tv.getString().len() > 1024 * 10 ){
      ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
              ErrClass::ERR_PARAM, // Wrong parametrization
              ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
              "S7200StringTrans::toPeriph",       // File and function name
              "String too long" // Unfortunately we don't know which DP
              );

      return PVSS_FALSE;
  }

  sprintf((char*)buffer, "%s%c", tv.getValue(), '\0');

  return PVSS_TRUE;
}

VariablePtr S7200StringTrans::toVar(const PVSSchar *buffer, const PVSSuint dlen,
                                   const PVSSuint /* subix */) const
{
  std::string strVal(reinterpret_cast<char const*>(buffer));
  PVSSuint strSize = std::min({static_cast<PVSSuint>(strVal.length()), static_cast<PVSSuint>(dlen-1)});
  return new TextVar(strVal.c_str(), strSize);
}


}//namespace
