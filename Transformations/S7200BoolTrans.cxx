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

#include "S7200BoolTrans.hxx"

#include "S7200HWMapper.hxx"

#include "Common/Logger.hxx"

#include <cmath>

#include "BitVar.hxx"

namespace Transformations{

TransformationType S7200BoolTrans::isA() const {
    return (TransformationType) S7200DrvBoolTransType;
}

TransformationType S7200BoolTrans::isA(TransformationType type) const {
	if (type == isA())
		return type;
	else
		return Transformation::isA(type);
}

Transformation *S7200BoolTrans::clone() const {
    return new S7200BoolTrans;
}

int S7200BoolTrans::itemSize() const {
	return size;
}

VariableType S7200BoolTrans::getVariableType() const {
	return BIT_VAR;
}


PVSSboolean S7200BoolTrans::toPeriph(PVSSchar *buffer, PVSSuint len,	const Variable &var, const PVSSuint subix) const {

	if(var.isA() != BIT_VAR){
		ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
				ErrClass::ERR_PARAM, // Wrong parametrization
				ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
				"REMUSDRVBoolTrans", "toPeriph", // File and function name
				"Wrong variable type "  + CharString(subix)// Unfortunately we don't know which DP
				);
		return PVSS_FALSE;
	}

	*reinterpret_cast<bool *>(buffer) = (reinterpret_cast<const BitVar&>(var)).getValue();

	return PVSS_TRUE;
}

VariablePtr S7200BoolTrans::toVar(const PVSSchar *buffer, const PVSSuint dlen, const PVSSuint subix) const {
	if(dlen && buffer != NULL)
		return new BitVar(*reinterpret_cast<const bool*>(buffer));

	ErrHdl::error(ErrClass::PRIO_SEVERE, // Data will be lost
			ErrClass::ERR_PARAM, // Wrong parametrization
			ErrClass::UNEXPECTEDSTATE, // Nothing else appropriate
			"S7200BoolTrans", "toVar", // File and function name
			"Wrong buffer" // Unfortunately we don't know which DP
			);
	return NULL;
}

}//namespace
