/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#include "PlayerObjectFactory.h"
#include "InventoryFactory.h"
#include "DatapadFactory.h"
#include "ObjectFactoryCallback.h"
#include "DatabaseManager/Database.h"
#include "DatabaseManager/DatabaseResult.h"
#include "DatabaseManager/DataBinding.h"
#include "LogManager/LogManager.h"
#include "WorldManager.h"
#include "WorldConfig.h"
#include "Utils/utils.h"
#include "Item.h"
#include "Weapon.h"
#include "BuffManager.h"
#include "Tutorial.h"
#include <assert.h>

//=============================================================================

bool					PlayerObjectFactory::mInsFlag    = false;
PlayerObjectFactory*	PlayerObjectFactory::mSingleton  = NULL;

//======================================================================================================================

PlayerObjectFactory*	PlayerObjectFactory::Init(Database* database)
{
	if(!mInsFlag)
	{
		mSingleton	= new PlayerObjectFactory(database);
		mInsFlag	= true;

		return mSingleton;
	}
	else
		return mSingleton;
}

//=============================================================================

PlayerObjectFactory::PlayerObjectFactory(Database* database) : FactoryBase(database)
{
	mInventoryFactory	= InventoryFactory::Init(mDatabase);
	mDatapadFactory		= DatapadFactory::Init(mDatabase);

	_setupDatabindings();
}

//=============================================================================

PlayerObjectFactory::~PlayerObjectFactory()
{
	_destroyDatabindings();

	mInsFlag = false;
	delete(mSingleton);
}

//=============================================================================

void PlayerObjectFactory::handleDatabaseJobComplete(void* ref,DatabaseResult* result)
{
	QueryContainerBase* asyncContainer = reinterpret_cast<QueryContainerBase*>(ref);

	switch(asyncContainer->mQueryType)
	{
		case POFQuery_MainPlayerData:
		{
			
			PlayerObject* playerObject = _createPlayer(result);

			playerObject->setClient(asyncContainer->mClient);
			QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,POFQuery_Skills,asyncContainer->mClient);
			asContainer->mObject = playerObject;

			mDatabase->ExecuteSqlAsync(this,asContainer,"SELECT skill_id FROM character_skills WHERE character_id=%lld",playerObject->getId());
		}
		break;

		case POFQuery_Skills:
		{
			PlayerObject* playerObject = dynamic_cast<PlayerObject	*>(asyncContainer->mObject);
			uint32 skillId;

			DataBinding* binding = mDatabase->CreateDataBinding(1);
			binding->addField(DFT_uint32,0,4);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&skillId);
				playerObject->mSkills.push_back(gSkillManager->getSkillById(skillId));
			}

			mDatabase->DestroyDataBinding(binding);

			playerObject->prepareSkillMods();
			playerObject->prepareSkillCommands();
			playerObject->prepareSchematicIds();

			playerObject->mSkillCmdUpdateCounter = playerObject->getSkillCommands()->size();
			playerObject->mSkillModUpdateCounter = playerObject->getSkillMods()->size();

			QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,POFQuery_Badges,asyncContainer->mClient);
			asContainer->mObject = playerObject;

			mDatabase->ExecuteSqlAsync(this,asContainer,"SELECT badge_id FROM character_badges WHERE character_id=%lld",playerObject->getId());
		}
		break;

		case POFQuery_Badges:
		{
			PlayerObject* playerObject = dynamic_cast<PlayerObject*>(asyncContainer->mObject);
			uint32 badgeId;

			DataBinding* binding = mDatabase->CreateDataBinding(1);
			binding->addField(DFT_uint32,0,4);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&badgeId);
				playerObject->mBadges.push_back(badgeId);
			}

			mDatabase->DestroyDataBinding(binding);

			QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,POFQuery_Factions,asyncContainer->mClient);
			asContainer->mObject = playerObject;

			mDatabase->ExecuteSqlAsync(this,asContainer,"SELECT faction_id,value FROM character_faction WHERE character_id=%lld ORDER BY faction_id",playerObject->getId());
		}
		break;

		case POFQuery_Factions:
		{
			PlayerObject*	playerObject = dynamic_cast<PlayerObject*>(asyncContainer->mObject);
			XpContainer		factionCont;

			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(XpContainer,mId),4,0);
			binding->addField(DFT_int32,offsetof(XpContainer,mValue),4,1);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&factionCont);
				playerObject->mFactionList.push_back(std::make_pair(factionCont.mId,factionCont.mValue));
			}

			mDatabase->DestroyDataBinding(binding);

			// query friendslist
			QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,POFQuery_Friends,asyncContainer->mClient);
			asContainer->mObject = playerObject;

			mDatabase->ExecuteSqlAsync(this,asContainer,"SELECT characters.firstname FROM chat_friendlist "
														"INNER JOIN characters ON (chat_friendlist.friend_id = characters.id) "
														"WHERE (chat_friendlist.character_id = %lld)",playerObject->getId());
		}
		break;

		case POFQuery_Friends:
		{
			PlayerObject* playerObject = dynamic_cast<PlayerObject*>(asyncContainer->mObject);
			string name;

			DataBinding* binding = mDatabase->CreateDataBinding(1);
			binding->addField(DFT_bstring,0,64);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&name);
				name.toLower();
				playerObject->mFriendsList.insert(std::make_pair(name.getCrc(),name.getAnsi()));
			}

			mDatabase->DestroyDataBinding(binding);

			// check online friends

			// query ignorelist
			QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,POFQuery_Ignores,asyncContainer->mClient);
			asContainer->mObject = playerObject;

			mDatabase->ExecuteSqlAsync(this,asContainer,"SELECT characters.firstname FROM chat_ignorelist "
														"INNER JOIN characters ON (chat_ignorelist.ignore_id = characters.id) "
														"WHERE (chat_ignorelist.character_id = %lld)",playerObject->getId());	
		}
		break;
		
		case POFQuery_DenyService:
		{
			PlayerObject* playerObject = dynamic_cast<PlayerObject*>(asyncContainer->mObject);
			uint64 id;

			DataBinding* binding = mDatabase->CreateDataBinding(1);
			binding->addField(DFT_uint64,0,8);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&id);
				playerObject->mDenyAudienceList.push_back(id);
			}

			mDatabase->DestroyDataBinding(binding);


			// query ignorelist
			QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,POFQuery_HoloEmotes,asyncContainer->mClient);
			asContainer->mObject = playerObject;

			mDatabase->ExecuteSqlAsync(this,asContainer,"SELECT  emote_id, charges FROM character_holoemotes WHERE character_id = %I64u",playerObject->getId());	
		}
		break;

		case POFQuery_HoloEmotes:
		{
			PlayerObject*	playerObject = dynamic_cast<PlayerObject*>(asyncContainer->mObject);
		

			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(PlayerObject,mHoloEmote),4,0);
			binding->addField(DFT_int32,offsetof(PlayerObject,mHoloCharge),4,1);

			uint64 count = result->getRowCount();

			if(count ==1)
			{
				result->GetNextRow(binding,playerObject);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;
		
		case POFQuery_Ignores:
		{
			PlayerObject* playerObject = dynamic_cast<PlayerObject*>(asyncContainer->mObject);
			string name;

			DataBinding* binding = mDatabase->CreateDataBinding(1);
			binding->addField(DFT_bstring,0,64);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&name);
				name.toLower();
				playerObject->mIgnoreList.insert(std::make_pair(name.getCrc(),name.getAnsi()));
			}

			mDatabase->DestroyDataBinding(binding);

			QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,POFQuery_XP,asyncContainer->mClient);
			asContainer->mObject = playerObject;

			mDatabase->ExecuteSqlAsync(this,asContainer,"SELECT xp_id,value FROM character_xp WHERE character_id=%lld",playerObject->getId());

			QueryContainerBase* outcastContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,POFQuery_DenyService,asyncContainer->mClient);
			outcastContainer->mObject = playerObject;

			mDatabase->ExecuteSqlAsync(this,outcastContainer,"SELECT outcast_id FROM entertainer_deny_service WHERE entertainer_id=%lld",playerObject->getId());

			QueryContainerBase* cloneDestIdContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,POFQuery_PreDefCloningFacility,asyncContainer->mClient);
			cloneDestIdContainer->mObject = playerObject;

			mDatabase->ExecuteSqlAsync(this,cloneDestIdContainer,"SELECT spawn_facility_id, x, y, z, planet_id FROM character_clone WHERE character_id=%lld",playerObject->getId());
		}
		break;

		case POFQuery_XP:
		{
			PlayerObject*	playerObject = dynamic_cast<PlayerObject*>(asyncContainer->mObject);
			XpContainer		xpCont;

			DataBinding* binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_uint32,offsetof(XpContainer,mId),4,0);
			binding->addField(DFT_int32,offsetof(XpContainer,mValue),4,1);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&xpCont);
				playerObject->mXpList.push_back(std::make_pair(xpCont.mId,xpCont.mValue));
			}
			// Initiate all XP caps and optionally any missing skills. 
			// Skills that require Jedi or JTL will not be included if player do not have the prerequisites.
			gSkillManager->initExperience(playerObject);

			playerObject->mXpUpdateCounter = count;

			mDatabase->DestroyDataBinding(binding);

			// store us for later lookup
			InLoadingContainer* ilc = new(mILCPool.ordered_malloc()) InLoadingContainer(playerObject,asyncContainer->mOfCallback,asyncContainer->mClient,2);
			ilc->mLoadCounter = 2;
			
			mObjectLoadMap.insert(std::make_pair(playerObject->getId(),ilc));

			// request inventory
			mInventoryFactory->requestObject(this,playerObject->mId + 1,TanGroup_Inventory,TanType_CharInventory,asyncContainer->mClient);
		
		}
		break;

		case POFQuery_EquippedItems:
		{
			PlayerObject* playerObject = dynamic_cast<PlayerObject*>(asyncContainer->mObject);

			InLoadingContainer*	mIlc = _getObject(playerObject->getId());			

			uint64 id;
			DataBinding* binding = mDatabase->CreateDataBinding(1);
			binding->addField(DFT_uint64,0,8);
			
			uint64 count = result->getRowCount();
			mIlc->mLoadCounter += count;

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&id);
				gTangibleFactory->requestObject(this,id,TanGroup_Item,0,asyncContainer->mClient);
			
			}
			mDatabase->DestroyDataBinding(binding);

			// get the datapad here to avoid a race condition
			// request datapad
			mDatapadFactory->requestObject(this,playerObject->mId + 3,TanGroup_Datapad,TanType_CharacterDatapad,asyncContainer->mClient);
		}
		break;

		// Get the id of the pre defined cloning facility, if any.
		case POFQuery_PreDefCloningFacility:
		{
			PlayerObject* playerObject = dynamic_cast<PlayerObject*>(asyncContainer->mObject);
	
			DataBinding* binding = mDatabase->CreateDataBinding(5);
			binding->addField(DFT_uint64,offsetof(PlayerObject,mPreDesignatedCloningFacilityId),8,0);
			binding->addField(DFT_float,offsetof(PlayerObject,mBindCoords.mX),4,1);
			binding->addField(DFT_float,offsetof(PlayerObject,mBindCoords.mY),4,2);
			binding->addField(DFT_float,offsetof(PlayerObject,mBindCoords.mZ),4,3);
			binding->addField(DFT_uint8,offsetof(PlayerObject,mBindPlanet),1,4);

			uint64 count = result->getRowCount();

			if (count == 1)
			{
				result->GetNextRow(binding,playerObject);
			}
			else
			{
				playerObject->mPreDesignatedCloningFacilityId = 0;
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		default:
		{
			break;
		}
	}

	mQueryContainerPool.free(asyncContainer);
}

//=============================================================================

void PlayerObjectFactory::requestObject(ObjectFactoryCallback* ofCallback,uint64 id,uint16 subGroup,uint16 subType,DispatchClient* client)
{
	QueryContainerBase* asyncContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(ofCallback,POFQuery_MainPlayerData,client);

	int8 sql[8152];
	sprintf(sql,"SELECT characters.id,characters.parent_Id,characters.account_id,characters.oX,characters.oY,characters.oZ,characters.oW,"
		"characters.x,characters.y,characters.z,character_appearance.base_model_string,"
		"characters.firstname,characters.lastname,character_appearance.hair,character_appearance.hair1,character_appearance.hair2,race.name,"
		"character_appearance.`00FF`,character_appearance.`01FF`,character_appearance.`02FF`,character_appearance.`03FF`,character_appearance.`04FF`,"
		"character_appearance.`05FF`,character_appearance.`06FF`,character_appearance.`07FF`,character_appearance.`08FF`,character_appearance.`09FF`,"
		"character_appearance.`0AFF`,character_appearance.`0BFF`,character_appearance.`0CFF`,character_appearance.`0DFF`,character_appearance.`0EFF`,"
		"character_appearance.`0FFF`,character_appearance.`10FF`,character_appearance.`11FF`,character_appearance.`12FF`,character_appearance.`13FF`,"
		"character_appearance.`14FF`,character_appearance.`15FF`,character_appearance.`16FF`,character_appearance.`17FF`,character_appearance.`18FF`,"
		"character_appearance.`19FF`,character_appearance.`1AFF`,character_appearance.`1BFF`,character_appearance.`1CFF`,character_appearance.`1DFF`,"
		"character_appearance.`1EFF`,character_appearance.`1FFF`,character_appearance.`20FF`,character_appearance.`21FF`,character_appearance.`22FF`,"
		"character_appearance.`23FF`,character_appearance.`24FF`,character_appearance.`25FF`,character_appearance.`26FF`,character_appearance.`27FF`,"
		"character_appearance.`28FF`,character_appearance.`29FF`,character_appearance.`2AFF`,character_appearance.`2BFF`,character_appearance.`2CFF`,"
		"character_appearance.`2DFF`,character_appearance.`2EFF`,character_appearance.`2FFF`,character_appearance.`30FF`,character_appearance.`31FF`,"
		"character_appearance.`32FF`,character_appearance.`33FF`,character_appearance.`34FF`,character_appearance.`35FF`,character_appearance.`36FF`,"
		"character_appearance.`37FF`,character_appearance.`38FF`,character_appearance.`39FF`,character_appearance.`3AFF`,character_appearance.`3BFF`,"
		"character_appearance.`3CFF`,character_appearance.`3DFF`,character_appearance.`3EFF`,character_appearance.`3FFF`,character_appearance.`40FF`,"
		"character_appearance.`41FF`,character_appearance.`42FF`,character_appearance.`43FF`,character_appearance.`44FF`,character_appearance.`45FF`,"
		"character_appearance.`46FF`,character_appearance.`47FF`,character_appearance.`48FF`,character_appearance.`49FF`,character_appearance.`4AFF`,"
		"character_appearance.`4BFF`,character_appearance.`4CFF`,character_appearance.`4DFF`,character_appearance.`4EFF`,character_appearance.`4FFF`,"
		"character_appearance.`50FF`,character_appearance.`51FF`,character_appearance.`52FF`,character_appearance.`53FF`,character_appearance.`54FF`,"
		"character_appearance.`55FF`,character_appearance.`56FF`,character_appearance.`57FF`,character_appearance.`58FF`,character_appearance.`59FF`,"
		"character_appearance.`5AFF`,character_appearance.`5BFF`,character_appearance.`5CFF`,character_appearance.`5DFF`,character_appearance.`5EFF`,"
		"character_appearance.`5FFF`,character_appearance.`60FF`,character_appearance.`61FF`,character_appearance.`62FF`,character_appearance.`63FF`,"
		"character_appearance.`64FF`,character_appearance.`65FF`,character_appearance.`66FF`,character_appearance.`67FF`,character_appearance.`68FF`,"
		"character_appearance.`69FF`,character_appearance.`6AFF`,character_appearance.`6BFF`,character_appearance.`6CFF`,character_appearance.`6DFF`,"
		"character_appearance.`6EFF`,character_appearance.`6FFF`,character_appearance.`70FF`,character_appearance.`ABFF`,character_appearance.`AB2FF`,character_attributes.health_max,character_attributes.strength_max,"
		"character_attributes.constitution_max,character_attributes.action_max,character_attributes.quickness_max,character_attributes.stamina_max,"
		"character_attributes.mind_max,character_attributes.focus_max,character_attributes.willpower_max,character_attributes.health_current,"
		"character_attributes.strength_current,character_attributes.constitution_current,character_attributes.action_current,"
		"character_attributes.quickness_current,character_attributes.stamina_current,character_attributes.mind_current,character_attributes.focus_current,"
		"character_attributes.willpower_current,character_attributes.health_wounds,character_attributes.strength_wounds,"
		"character_attributes.constitution_wounds,character_attributes.action_wounds,character_attributes.quickness_wounds,"
		"character_attributes.stamina_wounds,character_attributes.mind_wounds,character_attributes.focus_wounds,character_attributes.willpower_wounds,"
		"character_attributes.health_encum,character_attributes.action_encum,character_attributes.mind_encum,character_attributes.battlefatigue,character_attributes.language,"
		"banks.credits,faction.name,"
		"character_attributes.posture,character_attributes.moodId,characters.jedistate,character_attributes.title,character_appearance.scale,"
		"character_movement.baseSpeed,character_movement.baseAcceleration,character_movement.baseTurnrate,character_movement.baseTerrainNegotiation,"
		"character_attributes.character_flags,character_biography.biography,character_attributes.states,characters.race_id,"
		"banks.planet_id,account.csr,character_attributes.group_id,characters.bornyear,"
		"character_matchmaking.match_1, character_matchmaking.match_2, character_matchmaking.match_3, character_matchmaking.match_4,"
		"character_attributes.force_current,character_attributes.force_max,character_attributes.new_player_exemptions"
		" FROM characters"
		" INNER JOIN account ON(characters.account_id = account.account_id)"
		" INNER JOIN banks ON (%lld = banks.id)"
		" INNER JOIN character_appearance ON (characters.id = character_appearance.character_id)"
		" INNER JOIN race ON (characters.race_id = race.id)"
		" INNER JOIN character_attributes ON (characters.id = character_attributes.character_id)"
		" INNER JOIN character_movement ON (characters.id = character_movement.character_id)"
		" INNER JOIN faction ON (character_attributes.faction_id = faction.id)"
		" INNER JOIN character_biography ON (characters.id = character_biography.character_id)"
		" INNER JOIN character_matchmaking ON (characters.id = character_matchmaking.character_id)"
		" WHERE"
		" (characters.id = %lld);",id + 4,id);

	mDatabase->ExecuteSqlAsync(this,asyncContainer,sql);
}

//=============================================================================

PlayerObject* PlayerObjectFactory::_createPlayer(DatabaseResult* result)
{
	PlayerObject*	playerObject	= new PlayerObject();
	TangibleObject*	playerHair		= new TangibleObject();
	MissionBag*		playerMissionBag;
	Bank*			playerBank		= new Bank();
	Weapon*			playerWeapon	= new Weapon();

	playerBank->setParent(playerObject);

	uint64 count = result->getRowCount();
	assert(count == 1);

	// get our results
	result->GetNextRow(mPlayerBinding,(void*)playerObject);
	result->ResetRowIndex();
	result->GetNextRow(mHairBinding,(void*)playerHair);
	result->ResetRowIndex();
	result->GetNextRow(mBankBinding,(void*)playerBank);

	//male or female ?
	BStringVector				dataElements;
	playerObject->mModel.split(dataElements,'_');
	playerObject->setGender(dataElements[1].getCrc() == BString("female.iff").getCrc());

	// player object
	int8 tmpModel[128];
	sprintf(tmpModel,"object/creature/player/shared_%s",&playerObject->mModel.getAnsi()[23]);
	playerObject->setModelString(tmpModel);

	playerObject->buildCustomization(playerObject->mCustomization);
		
	playerObject->setFactionRank(0);
	// playerObject->setPvPStatus(16);
	playerObject->setPvPStatus(CreaturePvPStatus_Player);
	playerObject->setSpeciesGroup("species");
	playerObject->setCL(0);
	playerObject->setPlayerObjId(playerObject->mId + 7);
	playerObject->mTypeOptions = 0x80;
	playerObject->mBiography.convert(BSTRType_Unicode16);

	playerObject->mHam.calcAllModifiedHitPoints();
	playerObject->mHam.updateRegenRates();

	// hair
	if((playerHair->mModel).getLength())
	{
		int8 tmpHair[128];
		sprintf(tmpHair,"object/tangible/hair/%s/shared_%s",playerObject->mSpecies.getAnsi(),&playerHair->mModel.getAnsi()[22 + playerObject->mSpecies.getLength()]);
		playerHair->setId(playerObject->mId + 8);
		playerHair->setParentId(playerObject->mId);
		playerHair->setModelString(tmpHair);
		playerHair->setTangibleGroup(TanGroup_Hair);
		playerHair->setTangibleType(TanType_Hair);
		playerHair->setName("hair");
		playerHair->setNameFile("hair_name");
		playerHair->setEquipSlotMask(CreatureEquipSlot_Hair);
	
		playerHair->buildTanoCustomization(3);

		playerObject->mEquipManager.addEquippedObject(CreatureEquipSlot_Hair,playerHair);
		playerObject->setHair(playerHair);
	}
	else
	{
		playerObject->setHair(NULL);
		delete playerHair;
	}
		
	// mission bag
	playerMissionBag = new MissionBag(playerObject->mId + 2,playerObject,"object/tangible/mission_bag/shared_mission_bag.iff","item_n","mission_bag");
	playerMissionBag->setEquipSlotMask(CreatureEquipSlot_MissionBag);

	playerObject->mEquipManager.addEquippedObject(CreatureEquipSlot_MissionBag,playerMissionBag);

	// bank
	playerBank->setId(playerObject->mId + 4);
	playerBank->setParentId(playerObject->mId);
	playerBank->setModelString("object/tangible/bank/shared_character_bank.iff");
	playerBank->setName("bank");
	playerBank->setNameFile("item_n");
	playerBank->setTangibleGroup(TanGroup_PlayerInternal);
	playerBank->setTangibleType(TanType_Bank);
	playerBank->setEquipSlotMask(CreatureEquipSlot_Bank);

	playerObject->mEquipManager.addEquippedObject(CreatureEquipSlot_Bank,playerBank);

	// weapon
	playerWeapon->setId(playerObject->mId + 5);
	playerWeapon->setParentId(playerObject->mId);
	playerWeapon->setModelString("object/weapon/melee/unarmed/shared_unarmed_default_player.iff");
	playerWeapon->setGroup(WeaponGroup_Unarmed);
	playerWeapon->setEquipSlotMask(CreatureEquipSlot_Weapon);
	playerWeapon->addInternalAttribute("weapon_group","1");

	playerObject->mEquipManager.setDefaultWeapon(playerWeapon);
	
	// just making sure
	playerObject->togglePlayerFlagOff(PlayerFlag_LinkDead);
	playerObject->toggleStateOff(CreatureState_Crafting);
	playerObject->toggleStateOff(CreatureState_Combat);
	playerObject->toggleStateOff(CreatureState_Dizzy);
	playerObject->toggleStateOff(CreatureState_Stunned);
	playerObject->toggleStateOff(CreatureState_Blinded);
	playerObject->toggleStateOff(CreatureState_Intimidated);

	// logging in dead or incapped, shouldn't happen. (player is moved to cloning facility when disconnecting in those states
	if(playerObject->getPosture() == CreaturePosture_SkillAnimating
	|| playerObject->getPosture() == CreaturePosture_Incapacitated
	|| playerObject->getPosture() == CreaturePosture_Dead)
	{
		playerObject->setPosture(CreaturePosture_Upright);
	}

	// setup controller validators 
	playerObject->mObjectController.initEnqueueValidators();
	playerObject->mObjectController.initProcessValidators();

	// update movement properties
	playerObject->updateMovementProperties();

	// update race / gender mask
	playerObject->updateRaceGenderMask(playerObject->getGender());

	gBuffManager->LoadBuffs(playerObject, gWorldManager->GetCurrentGlobalTick());

	// Start tutorial, if any.
	playerObject->startTutorial();

	return playerObject;
}

//=============================================================================

void PlayerObjectFactory::_setupDatabindings()
{
	//player binding
	mPlayerBinding = mDatabase->CreateDataBinding(189);
	mPlayerBinding->addField(DFT_uint64,offsetof(PlayerObject,mId),8,0);
	mPlayerBinding->addField(DFT_uint64,offsetof(PlayerObject,mParentId),8,1);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mAccountId),4,2);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mDirection.mX),4,3);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mDirection.mY),4,4);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mDirection.mZ),4,5);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mDirection.mW),4,6);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mPosition.mX),4,7);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mPosition.mY),4,8);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mPosition.mZ),4,9);
	mPlayerBinding->addField(DFT_bstring,offsetof(PlayerObject,mModel),128,10);
	mPlayerBinding->addField(DFT_bstring,offsetof(PlayerObject,mFirstName),64,11);
	mPlayerBinding->addField(DFT_bstring,offsetof(PlayerObject,mLastName),64,12);
	mPlayerBinding->addField(DFT_bstring,offsetof(PlayerObject,mSpecies),16,16);
	mPlayerBinding->addField(DFT_bstring,offsetof(PlayerObject,mFaction),16,165);
	mPlayerBinding->addField(DFT_uint8,offsetof(PlayerObject,mPosture),1,166);
	mPlayerBinding->addField(DFT_uint8,offsetof(PlayerObject,mMoodId),1,167);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mJediState),4,168);
	mPlayerBinding->addField(DFT_bstring,offsetof(PlayerObject,mTitle),255,169);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mScale),4,170);

	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mBaseRunSpeedLimit),4,171);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mBaseAcceleration),4,172);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mBaseTurnRate),4,173);
	mPlayerBinding->addField(DFT_float,offsetof(PlayerObject,mBaseTerrainNegotiation),4,174);

	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mPlayerFlags),4,175);
	mPlayerBinding->addField(DFT_bstring,offsetof(PlayerObject,mBiography),4096,176);
	mPlayerBinding->addField(DFT_uint64,offsetof(PlayerObject,mState),8,177);
	mPlayerBinding->addField(DFT_uint8,offsetof(PlayerObject,mRaceId),1,178);
	mPlayerBinding->addField(DFT_uint8,offsetof(PlayerObject,mLanguage),1,163);
	mPlayerBinding->addField(DFT_uint8,offsetof(PlayerObject,mCsrTag),1,180);
	mPlayerBinding->addField(DFT_uint64,offsetof(PlayerObject,mGroupId),8,181);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mBornyear),4,182);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mPlayerMatch[0]),4,183);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mPlayerMatch[1]),4,184);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mPlayerMatch[2]),4,185);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mPlayerMatch[3]),4,186);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mCurrentForce),4,187);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mMaxForce),4,188);
	mPlayerBinding->addField(DFT_uint8,offsetof(PlayerObject,mNewPlayerExemptions),1,189);
	
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mHealth.mMaxHitPoints),4,132);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mStrength.mMaxHitPoints),4,133);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mConstitution.mMaxHitPoints),4,134);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mAction.mMaxHitPoints),4,135);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mQuickness.mMaxHitPoints),4,136);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mStamina.mMaxHitPoints),4,137);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mMind.mMaxHitPoints),4,138);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mFocus.mMaxHitPoints),4,139);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mWillpower.mMaxHitPoints),4,140);
	
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mHealth.mCurrentHitPoints),4,141);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mStrength.mCurrentHitPoints),4,142);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mConstitution.mCurrentHitPoints),4,143);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mAction.mCurrentHitPoints),4,144);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mQuickness.mCurrentHitPoints),4,145);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mStamina.mCurrentHitPoints),4,146);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mMind.mCurrentHitPoints),4,147);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mFocus.mCurrentHitPoints),4,148);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mWillpower.mCurrentHitPoints),4,149);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mHealth.mWounds),4,150);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mStrength.mWounds),4,151);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mConstitution.mWounds),4,152);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mAction.mWounds),4,153);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mQuickness.mWounds),4,154);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mStamina.mWounds),4,155);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mMind.mWounds),4,156);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mFocus.mWounds),4,157);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mWillpower.mWounds),4,158);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mHealth.mEncumbrance),4,159);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mAction.mEncumbrance),4,160);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mMind.mEncumbrance),4,161);
	mPlayerBinding->addField(DFT_uint32,offsetof(PlayerObject,mHam.mBattleFatigue),4,162);

	for(uint16 i = 0;i < 0x71;i++)
		mPlayerBinding->addField(DFT_uint16,offsetof(PlayerObject,mCustomization[i]),2,i + 17);
	mPlayerBinding->addField(DFT_uint16,offsetof(PlayerObject,mCustomization[171]),2,130);
	mPlayerBinding->addField(DFT_uint16,offsetof(PlayerObject,mCustomization[172]),2,131);

	//hair binding
	mHairBinding = mDatabase->CreateDataBinding(3);
	mHairBinding->addField(DFT_bstring,offsetof(TangibleObject,mModel),128,13);
	mHairBinding->addField(DFT_uint16,offsetof(TangibleObject,mCustomization[1]),2,14);
	mHairBinding->addField(DFT_uint16,offsetof(TangibleObject,mCustomization[2]),2,15);

	//bank binding
	mBankBinding = mDatabase->CreateDataBinding(2);
	mBankBinding->addField(DFT_uint32,offsetof(Bank,mCredits),4,164);
	mBankBinding->addField(DFT_uint8,offsetof(Bank,mPlanet),1,179);
}

//=============================================================================

void PlayerObjectFactory::_destroyDatabindings()
{
	mDatabase->DestroyDataBinding(mPlayerBinding);
	mDatabase->DestroyDataBinding(mHairBinding);
	mDatabase->DestroyDataBinding(mBankBinding);
}

//=============================================================================

void PlayerObjectFactory::handleObjectReady(Object* object,DispatchClient* client)
{
	mIlc = _getObject(object->getParentId());
	if(!mIlc)
	{
		gLogger->logMsg("no mIlc :(\n");
		return;
	}
	mIlc->mLoadCounter--;

	PlayerObject*		playerObject = dynamic_cast<PlayerObject*>(mIlc->mObject);
	TangibleObject*		tangibleObject = dynamic_cast<TangibleObject*>(object);

	
	if(Inventory* inventory = dynamic_cast<Inventory*>(object))
	{
		
		inventory->setEquipSlotMask(CreatureEquipSlot_Inventory);

		playerObject->mEquipManager.addEquippedObject(CreatureEquipSlot_Inventory,inventory);
		inventory->setParent(playerObject);

		QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(0,POFQuery_EquippedItems,client);
		asContainer->mObject = playerObject;

		mDatabase->ExecuteSqlAsync(this,asContainer,"SELECT id  FROM items WHERE parent_id=%lld",playerObject->getId());

	}
	else 
	if(Datapad* datapad = dynamic_cast<Datapad*>(object))
	{
		datapad->setEquipSlotMask(CreatureEquipSlot_Datapad);

		playerObject->mEquipManager.addEquippedObject(CreatureEquipSlot_Datapad,datapad);

		datapad->setOwner(playerObject);
	}
	else
	if(TangibleObject* item =  dynamic_cast<TangibleObject*>(object))
	{
		gWorldManager->addObject(item,true);

		playerObject->mEquipManager.addEquippedObject(item);
		
		Inventory* inventory = dynamic_cast<Inventory*>(playerObject->mEquipManager.getEquippedObject(CreatureEquipSlot_Inventory));
		
		inventory->addEquippedObject(item);
		
	}
	else
	{
		gLogger->logMsg("no idea what this was\n");
	}

	if(!mIlc->mLoadCounter)
	{
		if(!(_removeFromObjectLoadMap(playerObject->getId())))
			gLogger->logMsg("PlayerObjectFactory: Failed removing object from loadmap");

		// if weapon slot is empty, equip the unarmed default weapon
		if(!playerObject->mEquipManager.getEquippedObject(CreatureEquipSlot_Weapon))
		{
			// gLogger->logMsg("equip default weapon");
			playerObject->mEquipManager.equipDefaultWeapon();
		}

		// init equip counter
		playerObject->mEquipManager.resetEquippedObjectsUpdateCounter();

		mIlc->mOfCallback->handleObjectReady(playerObject,mIlc->mClient);

		mILCPool.free(mIlc);
	}
	//gBuffManager->InitBuffs(playerObject);
}

//=============================================================================

void PlayerObjectFactory::releaseAllPoolsMemory()
{
	mInventoryFactory->releaseQueryContainerPoolMemory();
	mInventoryFactory->releaseILCPoolMemory();
	mDatapadFactory->releaseQueryContainerPoolMemory();
	mDatapadFactory->releaseILCPoolMemory();

	releaseQueryContainerPoolMemory();
	releaseILCPoolMemory();
}

//=============================================================================


