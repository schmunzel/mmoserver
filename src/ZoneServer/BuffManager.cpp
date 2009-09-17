#include "BuffManager.h"
#include "Utils/EventHandler.h"
#include "PlayerObject.h"
#include "MessageLib/MessageLib.h"
#include "WorldManager.h"
#include "Buff.h"
#include "DatabaseManager/DatabaseManager.h"
#include "DatabaseManager/Database.h"
#include "DatabaseManager/DataBinding.h"
#include "DatabaseManager/DatabaseResult.h"
#include "BuffDBItem.h"

bool			BuffManager::mInsFlag = false;
BuffManager*	BuffManager::mSingleton = NULL;

struct buffAsyncContainer
{
	Buff*				buff;
	PlayerObject*		player;
	uint64				currentTime;
	BMQuery				mQueryType;
	WMAsyncContainer*	asyncContainer;
	void*				callBack;
	
};

BuffManager::BuffManager(Database *database):mDatabase(database)
{
}

BuffManager::~BuffManager(void)
{
}
BuffManager* BuffManager::Init(Database *database)
{
	if(mInsFlag == false)
	{
		mSingleton = new BuffManager(database);
		mInsFlag = true;
		return mSingleton;
	}
	else
		return mSingleton;
}
void BuffManager::handleDatabaseJobComplete(void *ref, DatabaseResult *result)
{
	buffAsyncContainer* asyncContainer = reinterpret_cast<buffAsyncContainer*>(ref);

	switch(asyncContainer->mQueryType)
	{
		//we started saving a player ASync - give it back to the WorldManager
		case BMQuery_Safe_Async:
		{
			//get the player and check whether this was the last buff callback
			PlayerObject* playerObject			= asyncContainer->player;

			playerObject->DecAsyncCount();

			//if this is the last callback continue with saving the players data
			if(!playerObject->GetAsyncCount())
			{

				//the asynccontainer was prepared by the worldmanager
				WMAsyncContainer* asContainer = asyncContainer->asyncContainer;
				
				// position save - the callback will be in the worldmanager to proceed with the rest of the safe
				mDatabase->ExecuteSqlAsync(reinterpret_cast<DatabaseCallback*>(asyncContainer->callBack),asContainer,"UPDATE characters SET parent_id=%lld,oX=%f,oY=%f,oZ=%f,oW=%f,x=%f,y=%f,z=%f,planet_id=%u,jedistate=%u WHERE id=%lld",playerObject->getParentId()
									,playerObject->mDirection.mX,playerObject->mDirection.mY,playerObject->mDirection.mZ,playerObject->mDirection.mW
									,playerObject->mPosition.mX,playerObject->mPosition.mY,playerObject->mPosition.mZ
									,gWorldManager->getZoneId(),playerObject->getJediState(),playerObject->getId());
			}

		}
		break;

		case BMQuery_Buffs:
		{
			LoadBuffsFromResult(asyncContainer, result);
			
		}
		break;
		case BMQuery_Buff_Attributes:
		{
			LoadBuffAttributesFromResult(asyncContainer, result);
		}
		break;
		case BMQuery_Delete:
		{
			//SAFE_DELETE(asyncContainer);
		}
		break;
		default:
		{
			//SAFE_DELETE(asyncContainer);
		}
		break;
	}
}
void BuffManager::LoadBuffsFromResult(buffAsyncContainer* asyncContainer, DatabaseResult* result)
{
	gLogger->logMsg("now load buffs from our results\n");
	DataBinding*	buffBinding = mDatabase->CreateDataBinding(9);
	buffBinding->addField(DFT_uint64,offsetof(BuffDBItem,mBuffId),8,0);
	buffBinding->addField(DFT_uint64,offsetof(BuffDBItem,mTargetId),8,1);
	buffBinding->addField(DFT_uint64,offsetof(BuffDBItem,mInstigatorId),8,2);
	buffBinding->addField(DFT_uint32,offsetof(BuffDBItem,mMaxTicks),4,3);
	buffBinding->addField(DFT_uint64,offsetof(BuffDBItem,mTickLength),8,4);
	buffBinding->addField(DFT_uint32,offsetof(BuffDBItem,mCurrentTick),4,5);
	buffBinding->addField(DFT_uint32,offsetof(BuffDBItem,mIconCRC),4,6);
	buffBinding->addField(DFT_uint64,offsetof(BuffDBItem,mPausedGlobalTick),8,7);
	buffBinding->addField(DFT_uint64,offsetof(BuffDBItem,mStartGlobalTick),8,8);

	uint64 rowCount = result->getRowCount();

	if(rowCount == 0)
	{
		gLogger->logMsgF("BuffManager : Couldn't find any Buffs for player : %I64u",MSG_NORMAL,asyncContainer->player->getId());
		
		SAFE_DELETE(asyncContainer);
		mDatabase->DestroyDataBinding(buffBinding);
		return;
	}

	buffAsyncContainer*	envelope	= dynamic_cast<buffAsyncContainer*>(asyncContainer);
	PlayerObject*	player			= envelope->player;

	BuffDBItem* tmp = new BuffDBItem();
	for(uint64 i = 0;i < rowCount;i++)
	{
		result->GetNextRow(buffBinding,tmp);
		Buff* buffTemp = Buff::FromDB(tmp, envelope->currentTime);
		buffTemp->setTarget(player);
		player->AddBuff(buffTemp);
	}
	SAFE_DELETE(tmp);
	mDatabase->DestroyDataBinding(buffBinding);


	BuffList::iterator it = player->Buffs.begin();

	while(it != player->Buffs.end())
	{
		envelope->buff = *it;
		envelope->mQueryType = BMQuery_Buff_Attributes;
		LoadBuffAttributes(envelope);
		it++;
	}

	
	buffAsyncContainer* newenvelope = new buffAsyncContainer();
	newenvelope->buff = envelope->buff;
	newenvelope->currentTime = envelope->currentTime;
	newenvelope->player = envelope->player;
	newenvelope->mQueryType = BMQuery_Delete;

	int8 sql2[550];
	sprintf(sql2, "delete from character_buffs where character_id = %I64u;", player->getId());
	mDatabase->ExecuteSqlAsync(this,newenvelope,sql2);

	SAFE_DELETE(envelope);
}

//=============================================================================
//
//

void BuffManager::LoadBuffAttributesFromResult(buffAsyncContainer* asyncContainer, DatabaseResult* result)
{
	//gLogger->logMsg("Loading an Attribute", FOREGROUND_GREEN);

	DataBinding*	buffBinding = mDatabase->CreateDataBinding(4);
	buffBinding->addField(DFT_int32,offsetof(BuffAttributeDBItem,mType),4,0);
	buffBinding->addField(DFT_int32,offsetof(BuffAttributeDBItem,mInitialValue),4,1);
	buffBinding->addField(DFT_int32,offsetof(BuffAttributeDBItem,mTickValue),4,2);
	buffBinding->addField(DFT_int32,offsetof(BuffAttributeDBItem,mFinalValue),4,3);

	uint64 rowCount = result->getRowCount();
	
	BuffAttributeDBItem* tmp = new BuffAttributeDBItem();
	for(uint64 i = 0;i < rowCount;i++)
	{
		result->GetNextRow(buffBinding,tmp);
		asyncContainer->buff->AddAttribute(BuffAttribute::FromDB(tmp));
	}
	SAFE_DELETE(tmp);
	mDatabase->DestroyDataBinding(buffBinding);

	//Start the buff later on - we want to avoid racing conditions with the knownplayer map
	//asyncContainer->buff->ReInit();

	int8 sql2[550];
	sprintf(sql2, "delete from character_buff_attributes where character_id = %I64u and buff_id = %I64u;", asyncContainer->player->getId(), asyncContainer->buff->GetDBID());
	mDatabase->ExecuteSqlAsync(NULL,NULL,sql2);
	SAFE_DELETE(asyncContainer);
}

//=============================================================================
//
//



bool BuffManager::SaveBuffsAsync(WMAsyncContainer* asyncContainer,DatabaseCallback* callback, PlayerObject* playerObject, uint64 currenttime)
{
	//we need to count our asyncalls we are going to do and store it in the player object
	//the attribute asynccall will get all out with the worldmanager - when the last call has processed
	//the worldmanager will proceed with saving
	uint32 buffCount = 0;

	//Remove Deleted Buffs
	playerObject->CleanUpBuffs();

	playerObject->SetAsyncCount(0);

	//count the calls necessary
	BuffList::iterator it = playerObject->Buffs.begin();
	while(it != playerObject->Buffs.end())
	{
		Buff* buff = *it;
		//Check if it is an active Buff
		if(!buff->GetIsMarkedForDeletion()) 
		{
			//TODO Check whether this is a buff that needs saving or just undoing or neither or both

			gWorldManager->removeBuffToProcess(buff->GetID());	
			
			//store the amount of async calls so we know when the last call finished
			playerObject->IncAsyncCount(); //this is the buff

			playerObject->IncAsyncCount(); //all attributes of a buff are stored in a single query
			
			//Save to DB, and remove from the Process Queue
			if(AddBuffToDB(asyncContainer, callback, buff, currenttime))
				buffCount++;
		
			//Free up Memory
			buff->EraseAttributes();
			it = playerObject->Buffs.erase(it);
		}
	}

	return(buffCount>0);

}
void BuffManager::SaveBuffs(PlayerObject* playerObject, uint64 currenttime)
{
	//RemovedDeleted Buffs
	playerObject->CleanUpBuffs();

	BuffList::iterator it = playerObject->Buffs.begin();
	while(it != playerObject->Buffs.end())
	{
		Buff* temp = *it;
		//Check if it is an active Buff
		if(!temp->GetIsMarkedForDeletion()) 
		{
			//Save to DB, and remove from the Process Queue
			AddBuffToDB(temp, currenttime);
			gWorldManager->removeBuffToProcess(temp->GetID());
		}

		//Free up Memory
		temp->EraseAttributes();
		it = playerObject->Buffs.erase(it);
	}
	
}

//=============================================================================
//
//

void BuffManager::LoadBuffs(PlayerObject* playerObject, uint64 currenttime)
{
	//check we don't have ghosted buffs
	if(playerObject->GetNoOfBuffs() > 0)
	{
		gLogger->logMsg("PlayerObject has ghosted Buffs. Inform Lloydyboy\n", FOREGROUND_RED);
		gMessageLib->sendSystemMessage(playerObject, "You appear to have Ghosted Buffs (Bug #958). Please inform Lloydyboy you saw this message");
		return;
	}

	//buffenvelope* temp = new buffenvelope();
	//temp->Player = playerObject;
	//temp->CurrentTime = currenttime;

	buffAsyncContainer* envelope	= new buffAsyncContainer();
	envelope->mQueryType	= BMQuery_Buffs;
	envelope->currentTime	= currenttime;
	envelope->player		= playerObject;

	int8 sql[550];
	sprintf(sql, "SELECT buff_id,character_id,instigator_id,max_ticks,tick_length,current_tick,icon,current_global_tick,start_global_tick from character_buffs where character_id = %I64u", playerObject->getId());
	//gLogger->logMsg(sql,FOREGROUND_BLUE | FOREGROUND_GREEN);
	mDatabase->ExecuteSqlAsync(this,envelope,sql);
}

//=============================================================================
//
//

void BuffManager::LoadBuffAttributes(buffAsyncContainer* envelope)
{
	buffAsyncContainer* newenvelope	= new buffAsyncContainer();
	newenvelope->buff			= envelope->buff;
	newenvelope->currentTime	= envelope->currentTime;
	newenvelope->player			= envelope->player;
	newenvelope->mQueryType		= BMQuery_Buff_Attributes;

	int8 sql[550];
	sprintf(sql, "SELECT type,initial,tick,final from character_buff_attributes where character_id = %I64u and buff_id = %I64u;", envelope->player->getId(), envelope->buff->GetDBID());
	//gLogger->logMsg(sql,FOREGROUND_BLUE | FOREGROUND_GREEN);
	mDatabase->ExecuteSqlAsync(this,newenvelope,sql);
}

//=============================================================================
//saves the buff to db asynchronically and gives the callbackhandler 
//

bool BuffManager::AddBuffToDB(WMAsyncContainer* asyncContainer,DatabaseCallback* callback, Buff* buff, uint64 currenttime)
{
	// we are counting the queries we will have to start so the callbackhandler knows when the last query is through
	// not sure what to do for creatures yet but my guess is thatthe handling will be very similiar
	// thus the respective counter is in the creatureobject
	// all asyncqueries will be returned to the callbackHandler provided
	// we can decrease then the call counter of the creature object and in the case it reaches zero continue with
	// whatever it was we set out to do


	//If we have been passed a null Buff
	if(!buff)
		return false;

	//Get PlayerObjects from CreatureObjects
	PlayerObject* player = dynamic_cast<PlayerObject*>(buff->GetTarget());
	PlayerObject* target = dynamic_cast<PlayerObject*>(buff->GetInstigator());   //what is the difference ?
	

	//If target is a player, not Creature/NPC
	if(player)
	{
		//Get ID for Instigator (if not Creature/NPC)
		uint64 instigatorid = 0;
		if(target)
		{
			instigatorid = target->getId();
		}

		uint32 bufficon = buff->GetIcon();
		int8 sql[550];
		sprintf(sql, "INSERT INTO character_buffs (character_id, buff_id, instigator_id, max_ticks, tick_length, current_tick, icon, current_global_tick, start_global_tick) values(");
		sprintf(sql+strlen(sql), "%I64u,", player->getId());
		sprintf(sql+strlen(sql), "%I64u,", buff->GetID());
		sprintf(sql+strlen(sql), "%I64u,", instigatorid);
		sprintf(sql+strlen(sql), "%u,", buff->GetNoOfTicks());
		sprintf(sql+strlen(sql), "%I64u,", buff->GetTickLength());
		sprintf(sql+strlen(sql), "%u,", buff->GetCurrentTickNumber());
		sprintf(sql+strlen(sql), "%u,", bufficon);
		sprintf(sql+strlen(sql), "%I64u,", currenttime);
		sprintf(sql+strlen(sql), "%I64u);", buff->GetStartGlobalTick());
		
		buffAsyncContainer*	asContainer = new(buffAsyncContainer);
		asContainer->mQueryType		= BMQuery_Safe_Async;
		asContainer->player			= player;
		asContainer->asyncContainer	= asyncContainer;
		asContainer->callBack		= callback;
		

		mDatabase->ExecuteSqlAsync(this,asContainer,sql);
		

		int8 sql2[550];
		sprintf(sql2, "INSERT INTO character_buff_attributes (buff_id,character_id,type,initial,tick,final) VALUES");
		//Add Attributes
		AttributeList::iterator it = buff->Attributes.begin();

		//if we have no attributes
		if(it == buff->Attributes.end())
		{
			return true; //the first part generates already a callback
		}

		while(it != buff->Attributes.end())
		{
			BuffAttribute* batemp = *it;

			//undo the attribute pre safe - we will reapply this on login
			buff->ModifyAttribute(batemp->GetType(), batemp->GetFinalValue());													  
			
			sprintf(sql2+strlen(sql2), "(%I64u,", buff->GetID());
			sprintf(sql2+strlen(sql2), "%I64u,", player->getId());
			sprintf(sql2+strlen(sql2), "%d,", (int32)batemp->GetType());
			sprintf(sql2+strlen(sql2), "%d,", batemp->GetInitialValue());
			sprintf(sql2+strlen(sql2), "%d,", batemp->GetTickValue());
			if(it+1 != buff->Attributes.end())
			{
				sprintf(sql2+strlen(sql2), "%d),", batemp->GetFinalValue());
			} else 
			{
				sprintf(sql2+strlen(sql2), "%d);", batemp->GetFinalValue());
			}
			it++;
		}
		
		asContainer					= new(buffAsyncContainer);
		asContainer->mQueryType		= BMQuery_Safe_Async;
		asContainer->player			= player;
		asContainer->asyncContainer	= asyncContainer;
		asContainer->callBack		= callback;
	
		mDatabase->ExecuteSqlAsync(this,asContainer,sql2);
		
		return true;
	}
	else 
		return false;
		
}

//=============================================================================
//saves the buff to db sync
//

void BuffManager::AddBuffToDB(Buff* buff, uint64 currenttime)
{
	//If we have been passed a null Buff
	if(!buff)
		return;

	//Check for deletion of buff
	if(buff->GetIsMarkedForDeletion())
		return;


	buffAsyncContainer* envelope	= new buffAsyncContainer();
	envelope->mQueryType	= BMQuery_Null;

	/*TODO Should I Reset the Stat here on the player so it doesn't get double buffed
		For now, this is dealt with by removing hte modifiers when saving Player HAM to DB, 
		but this might not be the best long term solution
	*/

	//Get PlayerObjects from CreatureObjects
	PlayerObject* Player = dynamic_cast<PlayerObject*>(buff->GetTarget());
	PlayerObject* target = dynamic_cast<PlayerObject*>(buff->GetInstigator());
	
	//If target is a player, not Creature/NPC
	if(Player)
	{
		//Get ID for Instigator (if not Creature/NPC)
		uint64 instigatorid = 0;
		if(target)
		{
			instigatorid = target->getId();
		}

		uint32 bufficon = buff->GetIcon();
		int8 sql[550];
		sprintf(sql, "INSERT INTO character_buffs (character_id, buff_id, instigator_id, max_ticks, tick_length, current_tick, icon, current_global_tick, start_global_tick) values(");
		sprintf(sql+strlen(sql), "%I64u,", Player->getId());
		sprintf(sql+strlen(sql), "%I64u,", buff->GetID());
		sprintf(sql+strlen(sql), "%I64u,", instigatorid);
		sprintf(sql+strlen(sql), "%u,", buff->GetNoOfTicks());
		sprintf(sql+strlen(sql), "%I64u,", buff->GetTickLength());
		sprintf(sql+strlen(sql), "%u,", buff->GetCurrentTickNumber());
		sprintf(sql+strlen(sql), "%u,", bufficon);
		sprintf(sql+strlen(sql), "%I64u,", currenttime);
		sprintf(sql+strlen(sql), "%I64u);", buff->GetStartGlobalTick());
		
		//gLogger->logMsg(sql,FOREGROUND_BLUE | FOREGROUND_GREEN);

		//Lloydyboy Changed Save SQL back to Sync, not ASync to ensure this is saved, before new zone loads
		//mDatabase->ExecuteSqlAsync(this,envelope,sql);
		mDatabase->DestroyResult(mDatabase->ExecuteSynchSql(sql));

		int8 sql2[550];
		sprintf(sql2, "INSERT INTO character_buff_attributes (buff_id,character_id,type,initial,tick,final) VALUES");
		//Add Attributes
		AttributeList::iterator it = buff->Attributes.begin();

		//if we have no attributes
		if(it == buff->Attributes.end())
		{
			return;
		}

		while(it != buff->Attributes.end())
		{
			BuffAttribute* batemp = *it;

			//undo the attribute pre safe - we will reapply this on login
			buff->ModifyAttribute(batemp->GetType(), batemp->GetFinalValue());													  

			sprintf(sql2+strlen(sql2), "(%I64u,", buff->GetID());
			sprintf(sql2+strlen(sql2), "%I64u,", Player->getId());
			sprintf(sql2+strlen(sql2), "%d,", (int32)batemp->GetType());
			sprintf(sql2+strlen(sql2), "%d,", batemp->GetInitialValue());
			sprintf(sql2+strlen(sql2), "%d,", batemp->GetTickValue());
			if(it+1 != buff->Attributes.end())
			{
				sprintf(sql2+strlen(sql2), "%d),", batemp->GetFinalValue());
			} else {
				sprintf(sql2+strlen(sql2), "%d);", batemp->GetFinalValue());
			}
			it++;
		}
		//gLogger->logMsg(sql2,FOREGROUND_BLUE | FOREGROUND_GREEN);
		
		//Lloydyboy Changed Save SQL back to Sync, not ASync to ensure this is saved, before new zone loads
		//mDatabase->ExecuteSqlAsync(this,envelope,sql2);
		mDatabase->DestroyResult(mDatabase->ExecuteSynchSql(sql2));
	}
}

//=============================================================================
//
//

void BuffManager::InitBuffs(PlayerObject* Player)
{
	BuffList::iterator it = Player->Buffs.begin();
	while(it != Player->Buffs.end())
	{
		Buff* temp = *it;
		temp->ReInit();
		
		it++;
	}
}



