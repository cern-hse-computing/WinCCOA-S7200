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

#include <cstring>

#include "S7200Int32Trans.hxx"

#include "S7200HWMapper.hxx"

#include "Common/Logger.hxx"

#include <cmath>

#include <IntegerVar.hxx>

namespace Transformations {

TransformationType S7200Int32Trans::isA() const {
    return (TransformationType) S7200DrvInt32TransType;
}

TransformationType S7200Int32Trans::isA(TransformationType type) const {
	if (type == isA())
		return type;
	else
		return Transformation::isA(type);
}

Transformation *S7200Int32Trans::clone() const {
    return new S7200Int32Trans;
}

int S7200Int32Trans::itemSize() const {
	return size;
}

VariableType S7200Int32Trans::getVariableType() const {
	return INTEGER_VAR;
}

int32_t ReverseInt32( const int32_t inInt32 )
{
   int32_t retVal;
   char *IntToConvert = ( char* ) & inInt32;
   char *returnInt = ( char* ) & retVal;

   // swap the bytes into a temporary buffer
   returnInt[0] = IntToConvert[3];
   returnInt[1] = IntToConvert[2];
   returnInt[2] = IntToConvert[1];
   returnInt[3] = IntToConvert[0];

   return retVal;
}

PVSSboolean S7200Int32Trans::toPeriph(PVSSchar *buffer, PVSSuint len, const Variable &var, const PVSSuint subix) const {

	if(var.isA() != INTEGER_VAR /* || subix >= Transformation::getNumberOfElements() */){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"S7200Int32Trans", "toPeriph", // File and function name
				"Wrong variable type or wrong length: " + CharString(len) + ", subix: " + CharString(subix) // Unfortunately we don't know which DP
				);

		return PVSS_FALSE;
	}
	reinterpret_cast<int32_t *>(buffer)[subix] = ReverseInt32(reinterpret_cast<const IntegerVar &>(var).getValue());
	return PVSS_TRUE;
}

VariablePtr S7200Int32Trans::toVar(const PVSSchar *buffer, const PVSSuint dlen, const PVSSuint subix) const {

	if(buffer == NULL || dlen%size > 0 || dlen < size*(subix+1)){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"S7200Int32Trans", "toVar", // File and function name
				"Null buffer pointer or wrong length: " + CharString(dlen) // Unfortunately we don't know which DP
				);
		return NULL;
	}
	return new IntegerVar(ReverseInt32(reinterpret_cast<const int32_t*>(buffer)[subix]));
}

}//namespace
