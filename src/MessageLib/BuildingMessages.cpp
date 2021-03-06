/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2014 The SWG:ANH Team
---------------------------------------------------------------------------------------
Use of this source code is governed by the GPL v3 license that can be found
in the COPYING file or at http://www.gnu.org/licenses/gpl-3.0.html

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
---------------------------------------------------------------------------------------
*/

#include "MessageLib.h"

#include "ZoneServer/GameSystemManagers/Structure Manager/BuildingObject.h"
#include "ZoneServer/GameSystemManagers/Structure Manager/CellObject.h"
#include "ZoneServer/GameSystemManagers/Structure Manager/HouseObject.h"
#include "ZoneServer/GameSystemManagers/Structure Manager/PlayerStructure.h"
#include "ZoneServer/Objects/Player Object/PlayerObject.h"
#include "ZoneServer/WorldManager.h"
#include "ZoneServer/ZoneOpcodes.h"




#include "NetworkManager/DispatchClient.h"
#include "NetworkManager/MessageFactory.h"
#include "NetworkManager/MessageOpcodes.h"
#include "NetworkManager/Message.h"



//======================================================================================================================
//
// Building Baselines Type 3
// contain: name,
//

bool MessageLib::sendBaselinesBUIO_3(BuildingObject* buildingObject,PlayerObject* playerObject)
{
    if(!(playerObject->isConnected()))
        return(false);

    Message* newMessage;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opBaselinesMessage);
    mMessageFactory->addUint64(buildingObject->getId());
    mMessageFactory->addUint32(opBUIO);
    mMessageFactory->addUint8(3);

    uint32 byteCount = 49 + buildingObject->getNameFile().getLength() + buildingObject->getName().getLength();
    mMessageFactory->addUint32(byteCount);
    mMessageFactory->addUint16(11);
    mMessageFactory->addFloat(1.0);
    mMessageFactory->addString(buildingObject->getNameFile());
    mMessageFactory->addUint32(0);
    mMessageFactory->addString(buildingObject->getName());
    mMessageFactory->addUint32(0);//custom name
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint16(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(1000);
    mMessageFactory->addUint8(1);

    newMessage = mMessageFactory->EndMessage();

    (playerObject->getClient())->SendChannelA(newMessage, playerObject->getAccountId(), CR_Client, 5);

    return(true);
}

bool MessageLib::sendBaselinesBUIO_3(HouseObject* buildingObject,PlayerObject* playerObject)
{
    if(!(playerObject->isConnected()))
        return(false);

    Message* fragment;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint16(11);
    mMessageFactory->addFloat(1.0);
    mMessageFactory->addString(buildingObject->getNameFile());
    mMessageFactory->addUint32(0);
    mMessageFactory->addString(buildingObject->getName());
    mMessageFactory->addUint32(0);//custom name
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint16(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint8(1);

    fragment = mMessageFactory->EndMessage();


    Message* newMessage;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opBaselinesMessage);
    mMessageFactory->addUint64(buildingObject->getId());
    mMessageFactory->addUint32(opBUIO);
    mMessageFactory->addUint8(3);
    mMessageFactory->addUint32(fragment->getSize());
    mMessageFactory->addData(fragment->getData(),fragment->getSize());

    newMessage = mMessageFactory->EndMessage();
    fragment->setPendingDelete(true);

    (playerObject->getClient())->SendChannelA(newMessage, playerObject->getAccountId(), CR_Client, 5);

    return(true);
}


//======================================================================================================================
//
// Building Baselines Type 6
// contain: unknown
//

bool MessageLib::sendBaselinesBUIO_6(BuildingObject* buildingObject,PlayerObject* playerObject)
{
    if(!(playerObject->isConnected()))
        return(false);

    Message* newMessage;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opBaselinesMessage);
    mMessageFactory->addUint64(buildingObject->getId());
    mMessageFactory->addUint32(opBUIO);
    mMessageFactory->addUint8(6);

    mMessageFactory->addUint32(14);
    mMessageFactory->addUint16(2);	// unknown
    mMessageFactory->addUint32(66); // unknown
    mMessageFactory->addUint32(0);	// unknown
    mMessageFactory->addUint32(0);

    newMessage = mMessageFactory->EndMessage();

    (playerObject->getClient())->SendChannelA(newMessage, playerObject->getAccountId(), CR_Client, 5);

    return(true);
}


bool MessageLib::sendBaselinesBUIO_6(HouseObject* buildingObject,PlayerObject* playerObject)
{
    if(!(playerObject->isConnected()))
        return(false);

    Message* newMessage;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opBaselinesMessage);
    mMessageFactory->addUint64(buildingObject->getId());
    mMessageFactory->addUint32(opBUIO);
    mMessageFactory->addUint8(6);

    mMessageFactory->addUint32(14);
    mMessageFactory->addUint16(2);	// unknown
    mMessageFactory->addUint32(0); // unknown
    mMessageFactory->addUint32(0);	// unknown
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);

    newMessage = mMessageFactory->EndMessage();

    (playerObject->getClient())->SendChannelA(newMessage, playerObject->getAccountId(), CR_Client, 5);

    return(true);
}


//======================================================================================================================
//
// Cell Baselines Type 3
// contain: cell nr,
//

bool MessageLib::sendBaselinesSCLT_3(CellObject* cellObject,uint64 cellNr,PlayerObject* playerObject)
{
    if(!(playerObject->isConnected()))
        return(false);

    Message* newMessage;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opBaselinesMessage);
    mMessageFactory->addUint64(cellObject->getId());
    mMessageFactory->addUint32(opSCLT);
    mMessageFactory->addUint8(3);

    mMessageFactory->addUint32(26);
    mMessageFactory->addUint16(5);	// unknown
    mMessageFactory->addUint32(0); // unknown
    mMessageFactory->addUint16(0);
    mMessageFactory->addUint32(0); // unknown
    mMessageFactory->addUint16(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(static_cast<uint32>(cellNr));

    newMessage = mMessageFactory->EndMessage();

    (playerObject->getClient())->SendChannelA(newMessage, playerObject->getAccountId(), CR_Client, 5);

    return(true);
}

//======================================================================================================================
//
// Cell Baselines Type 6
// contain: unknown
//

bool MessageLib::sendBaselinesSCLT_6(CellObject* cellObject,PlayerObject* playerObject)
{
    if(!(playerObject->isConnected()))
        return(false);

    Message* newMessage;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opBaselinesMessage);
    mMessageFactory->addUint64(cellObject->getId());
    mMessageFactory->addUint32(opSCLT);
    mMessageFactory->addUint8(6);

    mMessageFactory->addUint32(14);
    mMessageFactory->addUint16(2);	// unknown
    mMessageFactory->addUint32(149); // unknown
    mMessageFactory->addUint32(0);
    mMessageFactory->addUint32(0);

    newMessage = mMessageFactory->EndMessage();

    (playerObject->getClient())->SendChannelA(newMessage, playerObject->getAccountId(), CR_Client, 5);

    return(true);
}


//======================================================================================================================
//
// advises the client to go into the structure placement mode
//
//

bool MessageLib::sendEnterStructurePlacement(Object* deed, BString objectString, PlayerObject* playerObject)
{
    if(!(playerObject->isConnected()))
        return(false);

    Message* newMessage;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opStructurePlacementMode);
    mMessageFactory->addUint64(deed->getId());
    mMessageFactory->addString(objectString);

    newMessage = mMessageFactory->EndMessage();

    (playerObject->getClient())->SendChannelA(newMessage, playerObject->getAccountId(), CR_Client, 5);

    return(true);
}


//======================================================================================================================
//
// sends the Admin List for a structure
//

bool MessageLib::sendAdminList(PlayerStructure* structure, PlayerObject* playerObject)
{
    if(!(playerObject->isConnected()))
        return(false);

	auto data = structure->getAdminData();

    Message* newMessage;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opSendPermissionList);
	mMessageFactory->addUint32(data.admin_map_.size());

	std::string name;

	auto it = data.admin_map_.begin();
    while(it != data.admin_map_.end())    {
		name = (*it).second;
		std::u16string u16_name(name.begin(), name.end());
        mMessageFactory->addString(u16_name);

        it++;
    }

    mMessageFactory->addUint32(0); // ???
    //mMessageFactory->addUint16(0);	// unknown
    name = "ADMIN";
    std::u16string u16_name(name.begin(), name.end());
    mMessageFactory->addString(u16_name);
    mMessageFactory->addUint32(0); // ???

    newMessage = mMessageFactory->EndMessage();

    (playerObject->getClient())->SendChannelA(newMessage, playerObject->getAccountId(), CR_Client, 5);

    return(true);
}

//======================================================================================================================
//
// sends the Admin List for a structure
//

bool MessageLib::sendEntryList(PlayerStructure* structure, PlayerObject* playerObject)
{
    if(!(playerObject->isConnected()))
        return(false);

    auto data = structure->getAdminData();

    Message* newMessage;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opSendPermissionList);
	mMessageFactory->addUint32(data.ban_map_.size());

	std::string name;

	auto it = data.entry_map_.begin();
    while(it != data.entry_map_.end())    {
		name = (*it).second;
		std::u16string u16_name(name.begin(), name.end());
        mMessageFactory->addString(u16_name);

        it++;
    }

    mMessageFactory->addUint32(0); // ???
    //mMessageFactory->addUint16(0);	// unknown
    name = "ENTRY";
    std::u16string u16_name(name.begin(), name.end());
    mMessageFactory->addString(u16_name);
    mMessageFactory->addUint32(0); // ???

    newMessage = mMessageFactory->EndMessage();

    return(true);
}


//======================================================================================================================
//
// sends the Admin List for a structure
//

bool MessageLib::sendBanList(PlayerStructure* structure, PlayerObject* playerObject)
{
    if(!(playerObject->isConnected()))
        return(false);

   auto data = structure->getAdminData();

    Message* newMessage;

    mMessageFactory->StartMessage();
    mMessageFactory->addUint32(opSendPermissionList);
	mMessageFactory->addUint32(data.ban_map_.size());

	std::string name;

	auto it = data.ban_map_.begin();
    while(it != data.ban_map_.end())    {
		name = (*it).second;
		std::u16string u16_name(name.begin(), name.end());
        mMessageFactory->addString(u16_name);

        it++;
    }

    mMessageFactory->addUint32(0); // ???
    //mMessageFactory->addUint16(0);	// unknown
    name = "BAN";
    std::u16string u16_name(name.begin(), name.end());
    mMessageFactory->addString(u16_name);
    mMessageFactory->addUint32(0); // ???

    newMessage = mMessageFactory->EndMessage();

    return(true);
}

