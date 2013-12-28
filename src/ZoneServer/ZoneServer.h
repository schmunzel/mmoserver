/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2010 The SWG:ANH Team
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

#ifndef ANH_ZONESERVER_ZONESERVER_H
#define ANH_ZONESERVER_ZONESERVER_H

#include <memory>
#include <boost/thread/thread.hpp>

#include "Utils/bstring.h"
#include "Utils/typedefs.h"

#include "Common/Server.h"
#include "anh/app/swganh_kernel.h"
#include "anh/service/service_directory.h"
#include "anh/service/service_interface.h"
#include "anh/app/kernel_interface.h"
#include "anh/app/bindings.h"

//======================================================================================================================

namespace swganh {
namespace event_dispatcher {
    class EventDispatcherInterface;
}}  // namespace anh::event_dispatcher
//using namespace swganh;

class NetworkManager;
class Service;

namespace swganh	{
namespace service	{
	class ServiceInterface;
}
}

namespace swganh	{
namespace database	{
class DatabaseManager;
class Database;
}}

class MessageDispatch;
class CharacterLoginHandler;
class ObjectControllerDispatch;

// @note: for initial testing the new "Service" classes will be manually set up here
// in the future we should have a map container of string to services that they are
// stored in to make it easier to dynamically register services.
namespace zone {
class HamService;
}


//======================================================================================================================

class ProcessAddress
{
public:

    uint32	mType;
    BString		mAddress;
    uint16	mPort;
    uint32	mStatus;
    uint32	mActive;
};

//======================================================================================================================



class ZoneServer : public common::BaseServer
{
public:

    ZoneServer(int argc, char* argv[]);
    ~ZoneServer(void);

    void	Process(void);

    void	handleWMReady();

    std::string  getZoneName()  {
        return mZoneName;
    }

	
private:
    // Disable compiler generated methods.
    ZoneServer();
    ZoneServer(const ZoneServer&);
    const ZoneServer& operator=(const ZoneServer&);

    void	_updateDBServerList(uint32 status);
    void	_connectToConnectionServer(void);

	boost::asio::io_service io_pool_, cpu_pool_;
    std::unique_ptr<boost::asio::io_service::work> io_work_, cpu_work_;
    std::vector<boost::thread> io_threads_, cpu_threads_;
	
	/*@brief this will load the individual services
	*
	*/
	void LoadCoreServices_();
    void CleanupServices_();


	std::shared_ptr<swganh::app::SwganhKernel>	kernel_;

    std::string									mZoneName;
    uint32										mLastHeartbeat;
    
	NetworkManager*								mNetworkManager;
    swganh::database::DatabaseManager*          mDatabaseManager;

    Service*									mRouterService;
    
    CharacterLoginHandler*						mCharacterLoginHandler;
    ObjectControllerDispatch*					mObjectControllerDispatch;

    std::unique_ptr<zone::HamService>			ham_service_;
};



//======================================================================================================================

#endif  // ANH_ZONESERVER_ZONESERVER_H






