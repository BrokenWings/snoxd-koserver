#include "StdAfx.h"

std::queue<Packet *> DatabaseThread::_queue;
bool DatabaseThread::_running = true;
FastMutex DatabaseThread::_lock;
HANDLE DatabaseThread::s_hEvent;

void DatabaseThread::Startup(DWORD dwThreads)
{
	DWORD id;
	s_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	for (DWORD i = 0; i < dwThreads; i++)
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&ThreadProc, (LPVOID)i, NULL, &id);
}

void DatabaseThread::AddRequest(Packet * pkt)
{
	_lock.Acquire();
	_queue.push(pkt);
	_lock.Release();
	SetEvent(s_hEvent);
}

BOOL WINAPI DatabaseThread::ThreadProc(LPVOID lpParam)
{
	while (_running)
	{
		Packet *p = NULL;

		// Pull the next packet from the shared queue
		_lock.Acquire();
		if (_queue.size())
		{
			p = _queue.front();
			_queue.pop();
		}
		_lock.Release();

		// If there's no more packets to handle, wait until there are.
		if (p == NULL)
		{
			WaitForSingleObject(s_hEvent, INFINITE);
			continue;
		}

		// References are fun =p
		Packet & pkt = *p;

		// First 2 bytes are always going to be the socket ID
		// or -1 for no user.
		int16 uid = pkt.read<int16>();

		// Attempt to lookup the user if necessary
		CUser *pUser = NULL;
		if (uid >= 0)
		{
			pUser = g_pMain->GetUserPtr(uid);

			// Check to make sure they're still connected.
			if (pUser == NULL)
				continue;
		}

		switch (pkt.GetOpcode())
		{
		case WIZ_LOGIN:
			if (pUser) pUser->ReqAccountLogIn(pkt);
			break;
		case WIZ_SEL_NATION:
			if (pUser) pUser->ReqSelectNation(pkt);
			break;
		case WIZ_ALLCHAR_INFO_REQ:
			if (pUser) pUser->ReqAllCharInfo(pkt);
			break;
		case WIZ_CHANGE_HAIR:
			if (pUser) pUser->ReqChangeHair(pkt);
			break;
		case WIZ_NEW_CHAR:
			if (pUser) pUser->ReqCreateNewChar(pkt);
			break;
		case WIZ_DEL_CHAR:
			if (pUser) pUser->ReqDeleteChar(pkt);
			break;
		case WIZ_SEL_CHAR:
			if (pUser) pUser->ReqSelectCharacter(pkt);
			break;
		case WIZ_DATASAVE:
			if (pUser) pUser->ReqSaveCharacter();
			break;
		case WIZ_KNIGHTS_PROCESS:
			// g_pMain->m_KnightsManager.KnightsPacket(pUser, pkt);
			break;
		case WIZ_LOGIN_INFO:
			if (pUser) pUser->ReqSetLogInInfo(pkt);
			break;
		case WIZ_BATTLE_EVENT:
			// g_pMain->BattleEventResult(pkt);
			break;
		case WIZ_SHOPPING_MALL:
			if (pUser) pUser->ReqShoppingMall(pkt);
			break;
		case WIZ_SKILLDATA:
			if (pUser) pUser->ReqSkillDataProcess(pkt);
			break;
		case WIZ_FRIEND_PROCESS:
			if (pUser) pUser->ReqFriendProcess(pkt);
			break;
		case WIZ_CAPE:
			if (pUser) pUser->ReqChangeCape(pkt);
			break;
		}
		// Free the packet.
		delete p;
	}

	TRACE("[Thread %d] Exiting...\n", lpParam);
	return TRUE;
}


void CUser::ReqAccountLogIn(Packet & pkt)
{
	string strPasswd;
	pkt >> strPasswd;

	int8 nation = g_DBAgent.AccountLogin(m_strAccountID, strPasswd);

	// TO-DO: Clean up this account name nonsense
	if (nation >= 0)
	{
		strcpy(m_pUserData.m_Accountid, m_strAccountID.c_str());
		g_pMain->AddAccountName(this);
	}
	else
	{
		m_strAccountID.clear();
	}

	Packet result(WIZ_LOGIN);
	result << nation;
	Send(&result);
}

void CUser::ReqSelectNation(Packet & pkt)
{
	Packet result(WIZ_SEL_NATION);
	uint8 bNation = pkt.read<uint8>(), bResult;

	bResult = g_DBAgent.NationSelect(m_strAccountID, bNation) ? bNation : 0;
	result << bResult;
	Send(&result);
}

void CUser::ReqAllCharInfo(Packet & pkt)
{
	Packet result(WIZ_ALLCHAR_INFO_REQ);
	string strCharID1, strCharID2, strCharID3;

	result << uint8(1);
#if __VERSION >= 1920
	result << uint8(1); // 1.920+ flag, probably indicates whether there's any characters or not (stays 1 for 1+ characters though, so not a count :'(). Untested without.
#endif
	g_DBAgent.GetAllCharID(m_strAccountID, strCharID1, strCharID2, strCharID3);
	g_DBAgent.LoadCharInfo(strCharID1, result);
	g_DBAgent.LoadCharInfo(strCharID2, result);
	g_DBAgent.LoadCharInfo(strCharID3, result);

	Send(&result);
}

void CUser::ReqChangeHair(Packet & pkt)
{
	Packet result(WIZ_CHANGE_HAIR);
	string strUserID;
	uint32 nHair;
	uint8 bOpcode, bFace;
	pkt.SByte();
	pkt >> bOpcode >> strUserID >> bFace >> nHair;
	pkt.put(2, g_DBAgent.ChangeHair(m_strAccountID, strUserID, bOpcode, bFace, nHair));
	Send(&result);
}

void CUser::ReqCreateNewChar(Packet & pkt)
{
	string strCharID;
	uint32 nHair;
	uint16 sClass;
	uint8 bCharIndex, bRace, bFace, bStr, bSta, bDex, bInt, bCha;
	pkt >> bCharIndex >> strCharID >> bRace >> sClass >> bFace >> nHair >> bStr >> bSta >> bDex >> bInt >> bCha;

	Packet result(WIZ_NEW_CHAR);
	result << g_DBAgent.CreateNewChar(m_strAccountID, bCharIndex, strCharID, bRace, sClass, nHair, bFace, bStr, bSta, bDex, bInt, bCha);
	Send(&result);
}

void CUser::ReqDeleteChar(Packet & pkt)
{
	string strCharID, strSocNo;
	uint8 bCharIndex;
	pkt >> bCharIndex >> strCharID >> strSocNo;

	Packet result(WIZ_DEL_CHAR);
	int8 retCode = g_DBAgent.DeleteChar(m_strAccountID, bCharIndex, strCharID, strSocNo);
	result << retCode << uint8(retCode ? bCharIndex : -1);
	Send(&result);

#if 0
	if (retCode == 1 && sKnights != 0)
	{
		// TO-DO: Synchronise this system better. Much better. This is dumb.
		g_pMain->m_KnightsManager.RemoveKnightsUser(sKnights, (char *)strCharID.c_str());
		result.SetOpcode(UDP_KNIGHTS_PROCESS);
		result << uint8(KNIGHTS_WITHDRAW) << sKnights << strCharID;
		g_pMain->Send_UDP_All(&result, g_pMain->m_nServerGroup == 0 ? 0 : 1);
	}
#endif
}

void CUser::ReqSelectCharacter(Packet & pkt)
{
	Packet result(WIZ_SEL_CHAR);
	uint8 bInit;
	string strCharID;

	pkt >> strCharID >> bInit;
	if (m_strAccountID.empty() || strCharID.empty()
		|| m_strAccountID.length() > MAX_ID_SIZE || strCharID.length() > MAX_ID_SIZE
		|| !g_DBAgent.LoadUserData(m_strAccountID, strCharID, &m_pUserData)
		|| !g_DBAgent.LoadWarehouseData(m_strAccountID, &m_pUserData)
		|| !g_DBAgent.LoadPremiumServiceUser(m_strAccountID, &m_pUserData))
	{
		result << uint8(0);
	}
	else
	{
		result << uint8(1) << bInit;
	}

	SelectCharacter(result); 
}

void CUser::ReqShoppingMall(Packet & pkt)
{
	switch (pkt.read<uint8>())
	{
	case STORE_CLOSE:
		ReqLoadWebItemMall(pkt);
		break;
	}
}

void CUser::ReqLoadWebItemMall(Packet & pkt)
{
	Packet result(WIZ_SHOPPING_MALL, uint8(STORE_CLOSE));

	int offset = result.wpos(); // preserve offset
	result << uint8(0);

	if (g_DBAgent.LoadWebItemMall(result, &m_pUserData))
		result.put(offset, uint8(1));

	// m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqSkillDataProcess(Packet & pkt)
{
	uint8 opcode = pkt.read<uint8>();
	if (opcode == SKILL_DATA_LOAD)
		ReqSkillDataLoad(pkt);
	else if (opcode == SKILL_DATA_SAVE)
		ReqSkillDataSave(pkt);
}

void CUser::ReqSkillDataLoad(Packet & pkt)
{
	Packet result(WIZ_SKILLDATA, uint8(SKILL_DATA_LOAD));
	if (!g_DBAgent.LoadSkillShortcut(result, &m_pUserData))
		result << uint8(0);

	// m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqSkillDataSave(Packet & pkt)
{
	char buff[260];
	short sCount;

	// Initialize our buffer (not all skills are likely to be saved, we need to store the entire 260 bytes).
	memset(buff, 0x00, sizeof(buff));

	// Read in our skill count
	pkt >> sCount;

	// Make sure we're not going to copy too much (each skill is 1 uint32).
	if ((sCount * sizeof(uint32)) > sizeof(buff))
		return;

	// Copy the skill data directly in from where we left off reading in the packet buffer
	memcpy(buff, (char *)(pkt.contents() + pkt.rpos()), sCount * sizeof(uint32));

	// Finally, save the skill data.
	g_DBAgent.SaveSkillShortcut(sCount, buff, &m_pUserData);
}

void CUser::ReqFriendProcess(Packet & pkt)
{
	switch (pkt.read<uint8>())
	{
	case FRIEND_REQUEST:
		ReqRequestFriendList(pkt);
		break;

	case FRIEND_ADD:
		ReqAddFriend(pkt);
		break;

	case FRIEND_REMOVE:
		ReqRemoveFriend(pkt);
		break;
	}
}

void CUser::ReqRequestFriendList(Packet & pkt)
{
	Packet result(WIZ_FRIEND_PROCESS, uint8(FRIEND_REQUEST));
	std::vector<string> friendList;

	g_DBAgent.RequestFriendList(friendList, &m_pUserData);

	result << uint16(friendList.size());
	foreach (itr, friendList)
		result << (*itr);
	// m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqAddFriend(Packet & pkt)
{
	Packet result(WIZ_FRIEND_PROCESS, uint8(FRIEND_ADD));
	string strCharID;
	int16 tid;

	pkt.SByte();
	pkt >> tid >> strCharID;

	FriendAddResult resultCode = g_DBAgent.AddFriend(GetSocketID(), tid);
	result.SByte();
	result << tid << uint8(resultCode) << strCharID;
	// m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqRemoveFriend(Packet & pkt)
{
	Packet result(WIZ_FRIEND_PROCESS, uint8(FRIEND_REMOVE));
	string strCharID;

	pkt.SByte();
	pkt >> strCharID;

	FriendRemoveResult resultCode = g_DBAgent.RemoveFriend(strCharID, &m_pUserData);
	result.SByte();
	result << uint8(resultCode) << strCharID;
	// m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqChangeCape(Packet & pkt)
{
	uint16 sClanID, sCapeID;
	uint8 r, g, b;
	pkt >> sClanID >> sCapeID >> r >> g >> b;

	g_DBAgent.UpdateCape(sClanID, sCapeID, r, g, b);
}

// This one's temporarily synchronous to avoid potential threading issues
void CUser::ReqUserLogOut()
{
	string strCharID = GetName();

	g_DBAgent.UpdateUser(strCharID, UPDATE_LOGOUT, &m_pUserData);
	g_DBAgent.UpdateWarehouseData(m_strAccountID, UPDATE_LOGOUT, &m_pUserData);
	
	if (m_pUserData.m_bLogout != 2)	// zone change logout
		g_DBAgent.AccountLogout(m_strAccountID);
}

#if 0
void CUser::ReqConCurrentUserCount()
{
	int count = 0, total = m_DBAgent.m_UserDataArray.size();
	for (int i = 0; i < total; i++)
	{
		_USER_DATA * pUser = m_DBAgent.m_UserDataArray[i];
		if (pUser == NULL || *pUser->m_id == 0)
			continue;
		
		count++;
	}

	m_DBAgent.UpdateConCurrentUserCount(m_nServerNo, m_nZoneNo, count);
}
#endif

void CUser::ReqSaveCharacter()
{
	std::string strUserID = GetName();
	g_DBAgent.UpdateUser(strUserID, UPDATE_PACKET_SAVE, &m_pUserData);
	g_DBAgent.UpdateWarehouseData(m_strAccountID, UPDATE_PACKET_SAVE, &m_pUserData);
}

#if 0
void CUser::ReqKnightsPacket(Packet & pkt)
{
	uint8 opcode;
	pkt >> opcode;
	switch (opcode)
	{
	case KNIGHTS_CREATE:
		CreateKnights(pkt, uid);
		break;
	case KNIGHTS_JOIN:
		JoinKnights(pkt, uid);
		break;
	case KNIGHTS_WITHDRAW:
		WithdrawKnights(pkt, uid);
		break;
	case KNIGHTS_REMOVE:
	case KNIGHTS_ADMIT:
	case KNIGHTS_REJECT:
	case KNIGHTS_CHIEF:
	case KNIGHTS_VICECHIEF:
	case KNIGHTS_OFFICER:
	case KNIGHTS_PUNISH:
		ModifyKnightsMember(pkt, opcode, uid);
		break;
	case KNIGHTS_DESTROY:
		DestroyKnights(pkt, uid);
		break;
	case KNIGHTS_MEMBER_REQ:
		AllKnightsMember(pkt, uid);
		break;
	case KNIGHTS_LIST_REQ:
		KnightsList(pkt, uid);
		break;
	case KNIGHTS_ALLLIST_REQ:
		m_DBAgent.LoadKnightsAllList(pkt.read<uint8>()); // read nation
		break;
	case KNIGHTS_MARK_REGISTER:
		RegisterClanSymbol(pkt, uid);
		break;
	}
}

void CUser::ReqCreateKnights(Packet & pkt)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_CREATE));
	string strKnightsName, strChief;
	uint16 sClanID;
	uint8 bCommunity, bNation;

	pkt >> bCommunity >> sClanID >> bNation >> strKnightsName >> strChief;
	result	<< m_DBAgent.CreateKnights(sClanID, bNation, strKnightsName, strChief, bCommunity)
			<< bCommunity << sClanID << bNation << strKnightsName << strChief;

	m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqJoinKnights(Packet & pkt)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_JOIN));
	uint16 sClanID;

	pkt >> sClanID;
	_USER_DATA *pUser = m_DBAgent.GetUser(uid);
	if (pUser == NULL)
		return;

	string strCharID = pUser->m_id;
	result	<< int8(m_DBAgent.UpdateKnights(KNIGHTS_JOIN, strCharID, sClanID, 0))
			<< sClanID;
	m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqWithdrawKnights(Packet & pkt)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_WITHDRAW));
	uint16 sClanID;
	pkt >> sClanID;

	_USER_DATA *pUser = m_DBAgent.GetUser(uid);
	if (pUser == NULL)
		return;

	string strCharID = pUser->m_id;

	result << int8(m_DBAgent.UpdateKnights(KNIGHTS_WITHDRAW, strCharID, sClanID, 0))
		   << sClanID;
	m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqModifyKnightsMember(Packet & pkt, uint8 command)
{
	Packet result(WIZ_KNIGHTS_PROCESS, command);
	string strCharID;
	uint16 sClanID;
	uint8 bRemoveFlag;

	pkt >> sClanID >> strCharID >> bRemoveFlag;

	result	<< command << int8(m_DBAgent.UpdateKnights(command, strCharID, sClanID, bRemoveFlag))
			<< sClanID << strCharID;

	m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqDestroyKnights(Packet & pkt)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_DESTROY));
	uint16 sClanID = pkt.read<uint16>();

	result << int8(m_DBAgent.DeleteKnights(sClanID)) << sClanID;
	m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqAllKnightsMember(Packet & pkt)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_MEMBER_REQ));
	int nOffset;
	uint16 sClanID, sCount;

	pkt >> sClanID;
	result << uint8(0);
	nOffset = result.wpos(); // store offset
	result << uint16(0) << uint16(0); // placeholders
	sCount = m_DBAgent.LoadKnightsAllMembers(sClanID, result);

	pkt.put(nOffset, result.size() - 3);
	pkt.put(nOffset + 2, sCount);

	m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqKnightsList(Packet & pkt)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_LIST_REQ));
	uint16 sClanID = pkt.read<uint16>();

	result << uint8(0);
	m_DBAgent.LoadKnightsInfo(sClanID, result);
	
	m_LoggerSendQueue.PutData(&result, uid);
}

void CUser::ReqRegisterClanSymbol(Packet & pkt)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_MARK_REGISTER));
	char clanSymbol[MAX_KNIGHTS_MARK];
	uint16 sClanID, sSymbolSize;

	pkt >> sClanID >> sSymbolSize;
	pkt.read(clanSymbol, sSymbolSize);

	bool bResult = m_DBAgent.UpdateClanSymbol(sClanID, sSymbolSize, clanSymbol);
	result << uint8(0) << bResult;
	if (bResult)
	{
		result << sClanID << sSymbolSize;
		result.append(clanSymbol, sSymbolSize); // ... and back again! Like ping pong!
	}
	
	m_LoggerSendQueue.PutData(&result, uid);
}
#endif

void CUser::ReqSetLogInInfo(Packet & pkt)
{
	string strCharID, strServerIP, strClientIP;
	uint16 sServerNo;
	uint8 bInit;

	pkt >> strCharID >> strServerIP >> sServerNo >> strClientIP >> bInit;
	// if there was an error inserting to CURRENTUSER...
	if (!g_DBAgent.SetLogInInfo(m_strAccountID, strCharID, strServerIP, sServerNo, strClientIP, bInit))
		Disconnect();
}

#if 0
void CUser::ReqBattleEventResult(Packet & pkt)
{
	string strMaxUserName;
	uint8 bType, bNation;

	pkt >> bType >> bNation >> strMaxUserName;
	m_DBAgent.UpdateBattleEvent(strMaxUserName, bNation);
}
#endif

void DatabaseThread::Shutdown()
{
	_running = false;
	SetEvent(s_hEvent); // wake them up in case they're sleeping.

	_lock.Acquire();
	while (_queue.size())
	{
		Packet *p = _queue.front();
		_queue.pop();
		delete p;
	}
	_lock.Release();
}
