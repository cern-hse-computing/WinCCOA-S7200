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

#include <S7200Drv.hxx>
#include <S7200HWMapper.hxx>
#include <S7200HWService.hxx>

#include <HWObject.hxx>

//------------------------------------------------------------------------------------

void S7200Drv::install_HWMapper()
{
  hwMapper = new S7200HWMapper;
}

//--------------------------------------------------------------------------------

void S7200Drv::install_HWService()
{
  hwService = new S7200HWService;
}

//--------------------------------------------------------------------------------

HWObject * S7200Drv::getHWObject() const
{
  return new HWObject();
}

//--------------------------------------------------------------------------------

void S7200Drv::install_AlertService()
{
  DrvManager::install_AlertService();
}
