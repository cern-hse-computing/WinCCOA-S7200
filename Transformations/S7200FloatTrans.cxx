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

#include "S7200FloatTrans.hxx"

#include "S7200HWMapper.hxx"

#include "Common/Logger.hxx"

#include <cmath>

#include <FloatVar.hxx>

namespace Transformations{

TransformationType S7200FloatTrans::isA() const {
    return (TransformationType) S7200DrvFloatTransType;
}

TransformationType S7200FloatTrans::isA(TransformationType type) const {
	if (type == isA())
		return type;
	else
		return Transformation::isA(type);
}

Transformation *S7200FloatTrans::clone() const {
	return new S7200FloatTrans;
}

int S7200FloatTrans::itemSize() const {
	return size;
}

VariableType S7200FloatTrans::getVariableType() const {
	return FLOAT_VAR;
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


PVSSboolean S7200FloatTrans::toPeriph(PVSSchar *buffer, PVSSuint len,	const Variable &var, const PVSSuint subix) const {

	if(var.isA() != FLOAT_VAR){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"S7200FloatTrans", "toPeriph", // File and function name
				"Wrong variable type" // Unfortunately we don't know which DP
				);

		return PVSS_FALSE;
	}
	reinterpret_cast<float *>(buffer)[subix] = ReverseFloat((float)(reinterpret_cast<const FloatVar &>(var)).getValue());

	return PVSS_TRUE;
}

VariablePtr S7200FloatTrans::toVar(const PVSSchar *buffer, const PVSSuint dlen, const PVSSuint subix) const {

	if(buffer == NULL || dlen%size > 0 || dlen < size*(subix+1)){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"S7200FloatTrans", "toVar", // File and function name
				"Null buffer pointer or wrong length: " + CharString(dlen) // Unfortunately we don't know which DP
				);
		return NULL;
	}

	return new FloatVar(ReverseFloat(reinterpret_cast<const float*>(buffer)[subix]));
}

}//namespace
