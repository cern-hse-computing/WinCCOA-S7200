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

#include <cstring>

#include "S7200Int16Trans.hxx"

#include "S7200HWMapper.hxx"

#include "Common/Logger.hxx"

#include <cmath>

#include <IntegerVar.hxx>

namespace Transformations {

TransformationType S7200Int16Trans::isA() const {
    return (TransformationType) S7200DrvInt16TransType;
}

TransformationType S7200Int16Trans::isA(TransformationType type) const {
	if (type == isA())
		return type;
	else
		return Transformation::isA(type);
}

Transformation *S7200Int16Trans::clone() const {
    return new S7200Int16Trans;
}

int S7200Int16Trans::itemSize() const {
	return size;
}

VariableType S7200Int16Trans::getVariableType() const {
	return INTEGER_VAR;
}

int16_t ReverseInt16( const int16_t inInt16 )
{
   int16_t retVal;
   char *IntToConvert = ( char* ) & inInt16;
   char *returnInt = ( char* ) & retVal;

   // swap the bytes into a temporary buffer
   returnInt[0] = IntToConvert[1];
   returnInt[1] = IntToConvert[0];

   return retVal;
}


PVSSboolean S7200Int16Trans::toPeriph(PVSSchar *buffer, PVSSuint len,	const Variable &var, const PVSSuint subix) const {

	if(var.isA() != INTEGER_VAR){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"S7200Int16Trans", "toPeriph", // File and function name
				"Wrong variable type" // Unfortunately we don't know which DP
				);

		return PVSS_FALSE;
	}
	// this one is a bit special as the number is handled by wincc oa as int32, but we handle it as 16 bit  integer
	// thus any info above the 16 first bits is lost
	reinterpret_cast<int16_t *>(buffer)[subix] = ReverseInt16(reinterpret_cast<const IntegerVar &>(var).getValue());
	
	return PVSS_TRUE;
}

VariablePtr S7200Int16Trans::toVar(const PVSSchar *buffer, const PVSSuint dlen, const PVSSuint subix) const {

	if(buffer == NULL || dlen%size > 0 || dlen < size*(subix+1)){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"S7200Int16Trans", "toVar", // File and function name
				"Null buffer pointer or wrong length: " + CharString(dlen) // Unfortunately we don't know which DP
				);
		return NULL;
	}
	// this one is a bit special as the number is handled by wincc oa as int32, but we handle it as 16 bit  integer
	return new IntegerVar(__bswap_16((int16_t)*reinterpret_cast<const int16_t*>(buffer + (subix * size))));
}

}//namespace
