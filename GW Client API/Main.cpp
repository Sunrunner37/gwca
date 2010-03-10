#include "Main.h"
#include "Communicator.h"

/*	Guild Wars Client API - GWCA
	This is a large set of functions that makes interfacing with Guild Wars a lot easier.
	Uses Window Messages for communicating by hooking the WndProc of GW, allowing you
	to SendMessage directly to the Guild Wars window.
	Protocol rules are, if your Message expects a reply, then you have to provide the
	window handle of your application in lParam. The reply is sent with code 0x500.*/

byte* BaseOffset = NULL;
byte* PacketSendFunction = NULL;
byte* CurrentTarget = NULL;
byte* AgentArrayPtr = NULL;
byte* AgentArrayMaxPtr = NULL;
byte* MessageHandlerStart = NULL;
byte* MessageHandlerReturn = NULL;
byte* SkillLogStart = NULL;
byte* SkillLogReturn = NULL;
byte* WriteWhisperStart = NULL;
byte* TargetFunctions = NULL;
byte* HeroSkillFunction = NULL;
byte* ClickToMoveFix = NULL;
byte* BuildNumber = NULL;
byte* ChangeTargetFunction = NULL;
byte* MaxZoomStill = NULL;
byte* MaxZoomMobile = NULL;
byte* SkillCancelStart = NULL;
byte* SkillCancelReturn = NULL;
byte* SellSessionStart = NULL;
byte* SellSessionReturn = NULL;
byte* SellItemFunction = NULL;
byte* BuyItemFunction = NULL;
byte* PingLocation = NULL;
byte* LoggedInLocation = NULL;
byte* NameLocation = NULL;
byte* EmailLocation = NULL;
byte* DeadLocation = NULL;
byte* BasePointerLocation = NULL;
byte* MapIdLocation = NULL;
byte* DialogStart = NULL;
byte* DialogReturn = NULL;
byte* EngineStart = NULL;
byte* EngineExRet = NULL;
byte* EngineExStart = NULL;
byte* SkillTypeBase = NULL;
byte* WinHandle = NULL;
byte* LoadFinished = NULL;
byte* TargetLogStart = NULL;
byte* TargetLogReturn = NULL;
byte* VirtualHeroFlagFunction = NULL;
byte* SetAttrisFunc = NULL;
byte* LoginFunc = NULL;
byte* RegionLocation = NULL;
byte* LanguageLocation = NULL;
byte* RequestQuoteFunction = NULL;
byte* TraderCostStart = NULL;
byte* TraderCostReturn = NULL;
byte* TraderFunction = NULL;

dword FlagLocation = 0;
dword PacketLocation = 0;

AgentArray Agents;

BuffHandler MyBuffHandler;

bool LogSkills = false;
HWND ScriptHwnd = NULL;
wchar_t* pName;
long MoveItemId = NULL;
long TmpVariable = NULL;
long CurrentBag = 1;

long SellSessionId = NULL;
long LastDialogId = 0;
byte EngineHookSave[32];

bool FinishedLoading = false;

long* AgentTargets = new long[2560];

long PartyTeamSize = 8;

long TraderCostId = 0;
long TraderCostValue = 0;

Skillbar* MySkillbar = NULL;
CSectionA* MySectionA = new CSectionA();
ItemManager* MyItemManager = new ItemManager();

HANDLE PacketMutex;
HANDLE PartyMutex;
std::vector<CPacket*> PacketQueue;
std::vector<SkillLogSkill> SkillLogQueue;
std::vector<SkillLogSkill> SkillCancelQueue;
std::vector<PartyInfo*> PartyInfoQueue;
std::vector<long> TeamAgents;

void _declspec(naked) SkillLogHook(){
	SkillLogSkill* skillPtr;

	_asm {
		POP EDI
		MOV skillPtr,EDI
		MOV EAX,DWORD PTR DS:[ESI+0x10]
		INC EAX
		MOV DWORD PTR DS:[ESI+0x10],EAX
		PUSHAD
	}

	SkillLogQueue.push_back(*skillPtr);

	_asm {
		POPAD
		JMP SkillLogReturn
	}
}

void _declspec(naked) SkillCancelHook(){
	SkillLogSkill* cancelSkillPtr;

	_asm {
		MOV ESI,ECX
		MOV EAX,DWORD PTR DS:[EDI]
		MOV ECX,DWORD PTR DS:[ESI+4]
		PUSHAD
		MOV cancelSkillPtr,EDI
		CMP EAX,ECX
		JNZ SkillCancelSkip
	}

	SkillCancelQueue.push_back(*cancelSkillPtr);

	_asm {
SkillCancelSkip:
		POPAD
		JMP SkillCancelReturn
	}
}

void _declspec(naked) SellSessionHook(){
	_asm {
		PUSH ESI
		MOV ESI,ECX
		PUSH EDI

		MOV EDX,DWORD PTR DS:[ESI+4]
		MOV SellSessionId,EDX

		MOV EDX,2

		JMP SellSessionReturn
	}
}

void _declspec(naked) DialogHook(){
	_asm {
		PUSH EBP
		MOV EBP,ESP
		
		MOV EAX,DWORD PTR SS:[EBP+8]
		MOV LastDialogId,EAX

		MOV EAX,DWORD PTR DS:[ECX+8]
		TEST AL,1
		JMP DialogReturn
	}
}

void _declspec(naked) EngineHook(){
	Sleep(1);
	_asm RET
}

void _declspec(naked) LoadHook(){
	for(int i = 1;i < 2560;i++){ AgentTargets[i] = 0; }
	FinishedLoading = true;

	_asm {
		MOV ESP,EBP
		POP EBP
		RETN 0x10
	}
}

void _declspec(naked) TargetLogHook(){
	long agentTarget, agentCaster, actionType;

	_asm {
		PUSHAD
		MOV EAX,DWORD PTR DS:[EBP+8]
		MOV agentTarget,EAX
		MOV agentCaster,EDX
		MOV actionType,ECX
	}

	if(	actionType == 0x39 ||
		actionType == 0x03){
		AgentTargets[agentCaster] = agentTarget;
	}

	_asm {
		POPAD
		PUSH EBX
		MOV DWORD PTR SS:[EBP-0xC],ECX
		PUSH ESI
		JMP TargetLogReturn
	}
}

void _declspec(naked) TraderCostHook(){
	_asm {
		PUSHAD
		MOV TraderCostId,ECX
		MOV TraderCostValue,EDX
	}
	_asm {
		POPAD
		MOV EBP,ESP
		SUB ESP,8
		JMP TraderCostReturn
	}
}

void SetAttribute(dword atr,dword val){
	dword Attribute = atr;
	dword Value = val;
	long id = myId;
	_asm{
		MOV EDX,1  //attri count
		LEA ECX,Value//value
		PUSH ECX
		LEA EAX,Attribute//attribute
		PUSH EAX
		MOV ECX,id //AgentId to change attris
		CALL SetAttrisFunc
	}
}

//not working yet
void login(wchar_t* Email,wchar_t* PW,wchar_t* Charr){
	wchar_t* mail = Email;
	wchar_t* pw = PW;
	wchar_t* charr = Charr;
	_asm{
		PUSH 0x40
		MOV ECX,DWORD PTR SS:[pw]
		MOV EDX,DWORD PTR SS:[charr]
		PUSH ECX
		PUSH EDX
		MOV EDX,DWORD PTR SS:[mail]
		MOV ECX,0xA //??
		CALL LoginFunc
	}
}

void SendError( DWORD header ){
	Param_t OutWParam;
	OutWParam.i_Param = header;
	myGWCAServer->SetResponse(CA_Error, OutWParam);
}

void HandleMessages( WORD header, Param_t InWParam = Param_t(), Param_t InLParam = Param_t())
{
	Param_t OutWParam;
	Param_t OutLParam;
	int MsgLoop = 0;

	//printf("New Message: Header(%X)\n", header);
	//printf("WParam: %i, %f, %u\n", InWParam.i_Param, InWParam.f_Param, InWParam.d_Param);
	//printf("LParam: %i, %f, %u\n\n", InLParam.i_Param, InLParam.f_Param, InLParam.d_Param);

	switch(header){
		//Stuff related to you
	//case CA_TestMessageBox:
	//	MessageBoxW(0,text,testString,MB_OK);
	//	break;
	//case CA_TestBeep:
	//	Beep(500,500);
	//case CA_TestWhisper:
	//	WriteWhisper(text,L"GWCA");
	//	break;
	case CA_GetCurrentTarget: //Current Target : No return
		OutWParam.i_Param = *(long*)CurrentTarget;
		myGWCAServer->SetResponse(CA_GetCurrentTarget,OutWParam);
		break;
	case CA_GetMyId: //Get your own agent ID : Return int
		OutWParam.i_Param = myId;
		myGWCAServer->SetResponse(CA_GetMyId,OutWParam);
		break;
	case CA_Casting: //Check if you're casting : Return int/bool
		ReloadSkillbar();
		if(MySkillbar==NULL) {SendError(CA_Casting); break;}
		OutWParam.i_Param = MySkillbar->Casting;
		myGWCAServer->SetResponse(CA_Casting,OutWParam);
		break;
	case CA_SkillRecharge: //Check if skill is recharging : Return int/long
		ReloadSkillbar();
		if(MySkillbar==NULL) {SendError(CA_SkillRecharge); break;}
		OutWParam.i_Param = MySkillbar->Skill[InWParam.i_Param-1].Recharge;
		myGWCAServer->SetResponse(CA_SkillRecharge,OutWParam);
		break;
	case CA_SkillAdrenaline: //Check adrenaline points of a skill : Return int/long
		ReloadSkillbar();
		if(MySkillbar==NULL) {SendError(CA_SkillAdrenaline); break;}
		OutWParam.i_Param = MySkillbar->Skill[InWParam.i_Param-1].Adrenaline;
		myGWCAServer->SetResponse(CA_SkillAdrenaline,OutWParam);
	case CA_SetLogAndHwnd: //Set SkillLog and Script hWnd : No return
		LogSkills = InWParam.i_Param != 0;
		ScriptHwnd = (HWND)InLParam.i_Param;
		break;
	case CA_GetAgentAndTargetPtr: //Get base Agent array pointer and Current target pointer : Return ptr & ptr
		OutWParam.d_Param = (DWORD)AgentArrayPtr;
		OutLParam.d_Param = (DWORD)CurrentTarget;
		myGWCAServer->SetResponse(CA_GetAgentAndTargetPtr, OutWParam, OutLParam);
		break;
	case CA_GetSkillbarSkillId: //Get skill id of skills on your Skillbar : Return int/dword
		ReloadSkillbar();
		if(MySkillbar==NULL || InWParam.i_Param < 1 || InWParam.i_Param > 8)  {SendError(CA_GetSkillbarSkillId); break;}
		OutWParam.i_Param = MySkillbar->Skill[InWParam.i_Param-1].Id;
		myGWCAServer->SetResponse(CA_GetSkillbarSkillId, OutWParam);
		break;
	case CA_GetMyMaxHP: //Get your own max health (and current health): Return int/long & int
		if(Agents[myId]==NULL) {SendError(CA_GetMyMaxHP); break;}
		OutWParam.i_Param = Agents[myId]->MaxHP;
		OutLParam.i_Param = (int)(floor(Agents[myId]->MaxHP * Agents[myId]->HP));
		myGWCAServer->SetResponse(CA_GetMyMaxHP, OutWParam, OutLParam);
		break;
	case CA_GetMyMaxEnergy: //Get your own max energy (and current energy): Return int/long & int
		if(Agents[myId]==NULL) {SendError(CA_GetMyMaxEnergy); break;}
		OutWParam.i_Param = Agents[myId]->MaxEnergy;
		OutLParam.i_Param = (int)(floor(Agents[myId]->MaxEnergy * Agents[myId]->Energy));
		myGWCAServer->SetResponse(CA_GetMyMaxEnergy, OutWParam, OutLParam);
		break;
	case CA_GetBuildNumber: //Get build number of GW : Return int/long
		OutWParam.i_Param = buildNumber;
		myGWCAServer->SetResponse(CA_GetBuildNumber, OutWParam);
		break;
	case CA_ChangeMaxZoom: //Change max zoom of GW : No return
		if( InWParam.f_Param< 0 ||  InWParam.f_Param> 10000) {SendError(CA_ChangeMaxZoom); break;}
		ChangeMaxZoom(InWParam.f_Param);
		break;
	case CA_GetLastDialogId: //Get last dialog id : Return int/long
		OutWParam.i_Param = LastDialogId;
		myGWCAServer->SetResponse(CA_GetLastDialogId, OutWParam);
		break;
	case CA_SetEngineHook: //Enable or disable graphics rendering : No return
		SetEngineHook(InWParam.i_Param);
		break;

		//Packet Related Commands
	case CA_Attack: //Attack : No return
		if(OutWParam.i_Param == -1){AttackTarget(*(long*)CurrentTarget);}
		AttackTarget(InWParam.i_Param);
		break;
	case CA_Move: //Move to x, y : No return
		MovePlayer(InWParam.f_Param, InLParam.f_Param);
		break;
	case CA_UseSkill: //Use skill : No return
		ReloadSkillbar();
		if(MySkillbar == NULL || InWParam.i_Param < 1 || InWParam.i_Param > 8){SendError(CA_UseSkill); break;}
		if(InLParam.i_Param == -1){
			UseSkillNew(
				MySkillbar->Skill[InWParam.i_Param-1].Id,
				*(long*)CurrentTarget,
				MySkillbar->Skill[InWParam.i_Param-1].Event);
		}
		else if(InLParam.i_Param == -2){
			UseSkillNew(
				MySkillbar->Skill[InWParam.i_Param-1].Id,
				myId,
				MySkillbar->Skill[InWParam.i_Param-1].Event);
		}
		else {
			UseSkillNew(
				MySkillbar->Skill[InWParam.i_Param-1].Id,
				InLParam.i_Param,
				MySkillbar->Skill[InWParam.i_Param-1].Event);
		}
		break;
	case CA_ChangeWeaponSet: //Change weapon set : No return
		ChangeWeaponSet(InWParam.i_Param-1);
		break;
	case CA_ZoneMap: //Zone map : No return
		if (InLParam.i_Param == -1)
			MoveMap(InWParam.i_Param, MySectionA->Region(), 0, MySectionA->Language());
		else
			MoveMap(InWParam.i_Param, MySectionA->Region(), InLParam.i_Param, MySectionA->Language());
		break;
	case CA_DropGold: //Drop gold : No return
		DropGold(InWParam.i_Param);
		break;
	case CA_GoNpc: //Go to NPC : No return
		if(InWParam.i_Param == -1){
			GoNPC(*(long*)CurrentTarget);
		}
		else {
			GoNPC(InWParam.i_Param);
		}
		break;
	case CA_GoPlayer: //Go to player : No return
		if(InWParam.i_Param == -1){
			GoPlayer(*(long*)CurrentTarget);
		}
		else {
			GoPlayer(InWParam.i_Param);
		}
		break;
	case CA_GoSignpost: //Go to signpost : No return
		if(InWParam.i_Param == -1){
			GoSignpost(*(long*)CurrentTarget);
		}
		else {
			GoSignpost(InWParam.i_Param);
		}
		break;
	case CA_EnterChallenge: //Enter challenge mission : No return
		EnterChallenge();
		break;
	case CA_OpenChest: //Open chest : No return
		OpenChest();
		break;
	case CA_PickupItem: //Pick up item : No return
		if(InWParam.i_Param == -1){
			PickUpItem(*(long*)CurrentTarget);
		}
		else {
			PickUpItem(InWParam.i_Param);
		}
		break;
	case CA_Dialog: //Dialog packet : No return
		Dialog(InWParam.i_Param);
		break;
	case CA_ChangeTarget: //Change target : No return
		if(Agents[InWParam.i_Param]==NULL && InWParam.i_Param != 0){SendError(CA_ChangeTarget); break;}
		_asm MOV ECX,InWParam.i_Param
		_asm MOV EDX,0
		_asm CALL ChangeTargetFunction
		break;
	case CA_TargetNearestFoe: //Target nearest foe : No return
		TargetNearestFoe();
		break;
	case CA_TargetNearestAlly: //Target nearest ally : No return
		TargetNearestAlly();
		break;
	case CA_TargetNearestItem: //Target nearest item : No return
		TargetNearestItem();
		break;
	case CA_TargetCalledTarget: //Target called target : No return
		TargetCalledTarget();
		break;
	case CA_UseHero1Skill: //Use hero 1 skill : No return
		if(InLParam.i_Param < 1 || InLParam.i_Param > 8) {SendError(CA_UseHero1Skill); break;}

		if(InWParam.i_Param == -1){
			InWParam.i_Param = *(long*)CurrentTarget;
		}
		else if(OutWParam.i_Param == -2){
			InWParam.i_Param = myId;
		}

		if(Agents[InWParam.i_Param] == NULL) {SendError(CA_UseHero1Skill); break;}
		UseHeroSkill(*(long*)(MySectionA->HeroesStruct() + 0x4), InWParam.i_Param-1, InLParam.i_Param);
		break;
	case CA_UseHero2Skill: //Use hero 2 skill : No return
		if(InLParam.i_Param < 1 || InLParam.i_Param > 8) {SendError(CA_UseHero2Skill); break;}

		if(InWParam.i_Param == -1){
			InWParam.i_Param = *(long*)CurrentTarget;
		}
		else if(OutWParam.i_Param == -2){
			InWParam.i_Param = myId;
		}

		if(Agents[InWParam.i_Param] == NULL) {SendError(CA_UseHero2Skill); break;}
		UseHeroSkill(*(long*)(MySectionA->HeroesStruct() + 0x28), InWParam.i_Param-1, InLParam.i_Param);
		break;
	case CA_UseHero3Skill: //Use hero 3 skill : No return
		if(InLParam.i_Param < 1 || InLParam.i_Param > 8) {SendError(CA_UseHero3Skill); break;}

		if(InWParam.i_Param == -1){
			InWParam.i_Param = *(long*)CurrentTarget;
		}
		else if(OutWParam.i_Param == -2){
			InWParam.i_Param = myId;
		}

		if(Agents[InWParam.i_Param] == NULL) {SendError(CA_UseHero3Skill); break;}
		UseHeroSkill(*(long*)(MySectionA->HeroesStruct() + 0x4C), InWParam.i_Param-1, InLParam.i_Param);
	case CA_CancelAction: //Cancel movement : No return
		CancelAction();
		break;
	case CA_GetName: //Get name of agent : Return wchar_t*
		if(InWParam.i_Param == -1){
			InWParam.i_Param = *(long*)CurrentTarget;
		}
		else if(OutWParam.i_Param == -2){
			InWParam.i_Param = myId;
		}
		if(Agents[InWParam.i_Param] == NULL)  {SendError(CA_GetName); break;}
		OutWParam.d_Param = (dword)GetAgentName(InWParam.i_Param);
		myGWCAServer->SetResponse(0x42E, OutWParam);
		break;
	case CA_CommandHero1: //Command hero 1 to location : No return
		CommandHero(*(long*)(MySectionA->HeroesStruct() + 0x4), InWParam.f_Param, InLParam.f_Param);
		break;
	case CA_CommandHero2: //Command hero 2 to location : No return
		CommandHero(*(long*)(MySectionA->HeroesStruct() + 0x28), InWParam.f_Param, InLParam.f_Param);
		break;
	case CA_CommandHero3: //Command hero 3 to location : No return
		CommandHero(*(long*)(MySectionA->HeroesStruct() + 0x4C), InWParam.f_Param, InLParam.f_Param);
		break;
	case CA_CommandAll: //Command all to location : No return
		CommandAll(InWParam.f_Param, InLParam.f_Param);
		break;
	case CA_ChangeDistrict: //Change region and language : No return
		ChangeDistrict(InWParam.i_Param, InLParam.i_Param);
		break;
	case CA_Resign: //Send /resign to chat, effectively resigning : No return
		SendChat('/',"resign");
		break;
	case CA_ReturnToOutpost: //Send "Return to Outpost" packet : No return
		ReturnToOutpost();
		break;
	case CA_GoAgent: //Go to target : No return
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GoAgent); break;}
		GoAgent(InWParam.i_Param);
		break;
	case CA_DonateFaction: //Donate faction : No return
		DonateFaction(InWParam.i_Param);
		break;
	case CA_SetSkillbarSkill: //Set skillbar skill : No return
		SetSkillbarSkill(OutWParam.i_Param, InLParam.i_Param);
		break;
	case CA_ChangeSecondProfession: //Change second profession : No return
		if(OutWParam.i_Param < 1 || OutWParam.i_Param > 10){SendError(CA_ChangeSecondProfession); break;}
		ChangeSecondProfession(OutWParam.i_Param);
		break;
	case CA_TargetNextPartyMember: //Target next party member : No return
		TargetNextPartyMember();
		break;
	case CA_TargetNextFoe: //Target next foe : No return
		TargetNextFoe();
		break;
	case CA_SkipCinematic: //Skip cinematic : No return
		SkipCinematic();
		break;
	case CA_DismissBuff: //Dismiss Buff : No return
		if(InWParam.i_Param < 0 || InWParam.i_Param > 3){break;}
		if(InLParam.i_Param < 0){break;}
		DismissBuff(MyBuffHandler.GetBuff(InWParam.i_Param,InLParam.i_Param)->BuffId);
		break;
	case CA_SendChat: // Send Chat : No return
		if(InLParam.d_Param == 0){break;}
		SendChat((char)(InWParam.i_Param + 33),(wchar_t*)InLParam.d_Param);
		break;

		//SectionA related commands
	case CA_GetMapLoading: //Check if map is loading : Return int/long
		OutWParam.i_Param = mapLoading;
		myGWCAServer->SetResponse(CA_GetMapLoading, OutWParam);
		break;
	case CA_GetMapId: //Get map id : Return int/long
		OutWParam.i_Param = MySectionA->MapId();
		myGWCAServer->SetResponse(CA_GetMapId, OutWParam);
		break;
	case CA_GetPing: //Get ping : Return int/long
		OutWParam.i_Param = MySectionA->Ping();
		myGWCAServer->SetResponse(CA_GetPing, OutWParam);
		break;
	case CA_GetLoggedIn: //Check if logged in : Return int/long
		OutWParam.i_Param = MySectionA->LoggedIn();
		myGWCAServer->SetResponse(CA_GetLoggedIn, OutWParam);
		break;
	case CA_GetDead: //Check if you're dead : Return int/long
		OutWParam.i_Param = MySectionA->Dead();
		myGWCAServer->SetResponse(CA_GetDead, OutWParam);
		break;
	case CA_GetBalthFaction: //Get current and max balthazar faction : Return int/long & int/long
		OutWParam.i_Param = MySectionA->CurrentBalthFaction();
		OutLParam.i_Param = MySectionA->MaxBalthFaction();
		myGWCAServer->SetResponse(CA_GetBalthFaction, OutWParam, OutLParam);
		break;
	case CA_GetKurzFaction: //Get current and max kurzick faction : Return int/long & int/long
		OutWParam.i_Param = MySectionA->CurrentKurzickFaction();
		OutLParam.i_Param = MySectionA->MaxKurzickFaction();
		myGWCAServer->SetResponse(CA_GetKurzFaction, OutWParam, OutLParam);
		break;
	case CA_GetLuxonFaction: //Get current and max luxon faction : Return int/long & int/long
		OutWParam.i_Param = MySectionA->CurrentLuxonFaction();
		OutLParam.i_Param = MySectionA->MaxLuxonFaction();
		myGWCAServer->SetResponse(CA_GetLuxonFaction, OutWParam, OutLParam);
		break;
	case CA_GetTitleTreasure: //Get current Treasure Title (credits to ddarek): Return int/long
		OutWParam.i_Param = MySectionA->TitleTreasure();
		myGWCAServer->SetResponse(CA_GetTitleTreasure, OutWParam);
		break;
	case CA_GetTitleLucky: //Get current Lucky Title (credits to ddarek): Return int/long
		OutWParam.i_Param = MySectionA->TitleLucky();
		myGWCAServer->SetResponse(CA_GetTitleLucky, OutWParam);
		break;
	case CA_GetTitleUnlucky: //Get current Unlucky Title (credits to ddarek): Return int/long
		OutWParam.i_Param = MySectionA->TitleUnlucky();
		myGWCAServer->SetResponse(CA_GetTitleUnlucky, OutWParam);
		break;
	case CA_GetTitleWisdom: //Get current Wisdom Title (credits to ddarek): Return int/long
		OutWParam.i_Param = MySectionA->TitleWisdom();
		myGWCAServer->SetResponse(CA_GetTitleWisdom, OutWParam);
		break;
	case CA_GetTitleGamer: //Get current Gamer Title : Return int/long
		OutWParam.i_Param = MySectionA->TitleGamer();
		myGWCAServer->SetResponse(CA_GetTitleGamer, OutWParam);
	case CA_GetExperience: //Get current amount of Experience points : Return int/long
		OutWParam.i_Param = MySectionA->Experience();
		myGWCAServer->SetResponse(CA_GetExperience, OutWParam);
		break;

		//Agent Related Commands
	case CA_GetAgentExist: //Check for agent existency : Return int/bool
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL){
			OutWParam.i_Param = 0;
		}else{
			OutWParam.i_Param = 1;
		}
		myGWCAServer->SetResponse(CA_GetAgentExist, OutWParam);
		break;
	case CA_GetProfessions: //Get agent's primary and secondary profession : Return byte & byte
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetProfessions); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->Primary;
		OutLParam.i_Param = Agents[InWParam.i_Param]->Secondary;
		myGWCAServer->SetResponse(CA_GetProfessions, OutWParam, OutLParam);
		break;
	case CA_GetPlayerNumber: //Get player number of agent : Return word
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetPlayerNumber); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->PlayerNumber;
		myGWCAServer->SetResponse(CA_GetPlayerNumber, OutWParam);
		break;
	case CA_GetHP: //Get HP of agent : Return float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetHP); break;}
		OutWParam.f_Param = Agents[InWParam.i_Param]->HP;
		myGWCAServer->SetResponse(CA_GetHP, OutWParam);
		break;
	case CA_GetRotation: //Get rotation of agent in radian : Return float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetRotation); break;}
		OutWParam.f_Param = Agents[InWParam.i_Param]->Rotation;
		myGWCAServer->SetResponse(CA_GetRotation, OutWParam);
		break;
	case CA_GetSkill: //Get agent's current skill : Return word
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetSkill); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->Skill;
		myGWCAServer->SetResponse(CA_GetSkill, OutWParam);
		break;
	case CA_GetCoords: //Get X,Y coords of agent : Return float & float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetCoords); break;}
		OutWParam.f_Param = Agents[InWParam.i_Param]->X;
		OutLParam.f_Param = Agents[InWParam.i_Param]->Y;
		myGWCAServer->SetResponse(CA_GetCoords, OutWParam, OutLParam);
		break;
	case CA_GetWeaponSpeeds: //Get weapon speeds of agent (weapon attack speed, attack speed modifier) : Return float & float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetWeaponSpeeds); break;}
		OutWParam.f_Param = Agents[InWParam.i_Param]->WeaponAttackSpeed;
		OutLParam.f_Param = Agents[InWParam.i_Param]->AttackSpeedModifier;
		myGWCAServer->SetResponse(CA_GetWeaponSpeeds, OutWParam, OutLParam);
		break;
	case CA_GetSpiritRange: //Is agent in spirit range of me : Return int/long
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetSpiritRange); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->InSpiritRange;
		myGWCAServer->SetResponse(CA_GetSpiritRange, OutWParam);
		break;
	case CA_GetTeamId: //Get team ID of agent (0 = none) : Return byte
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetTeamId); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->TeamId;
		myGWCAServer->SetResponse(CA_GetMyMaxEnergy, OutWParam);
		break;
	case CA_GetCombatMode: //Get agent's combat mode : Return byte
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetCombatMode); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->CombatMode;
		myGWCAServer->SetResponse(CA_GetCombatMode, OutWParam);
		break;
	case CA_GetModelMode: //Get agent's model mode : Return float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetModelMode); break;}
		OutWParam.f_Param = Agents[InWParam.i_Param]->ModelMode;
		myGWCAServer->SetResponse(CA_GetModelMode, OutWParam);
		break;
	case CA_GetHpPips: //Get agent's health pips : Return int
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetHpPips); break;}
		OutWParam.i_Param = static_cast<int>(((Agents[InWParam.i_Param]->HPPips / 0.0038) > 0.0) ? floor((Agents[InWParam.i_Param]->HPPips / 0.0038) + 0.5) : ceil((Agents[InWParam.i_Param]->HPPips / 0.0038) - 0.5));
		myGWCAServer->SetResponse(CA_GetHpPips, OutWParam);
		break;
	case CA_GetEffects: //Get agent's effect bit map : Return int/long
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetEffects); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->Effects;
		myGWCAServer->SetResponse(CA_GetEffects, OutWParam);
		break;
	case CA_GetHex: //Get agent's hex bit map : Return byte
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetHex); break;}
		OutWParam.i_Param = 0;
		if((Agents[InWParam.i_Param]->Effects & 0x0800)) OutWParam.i_Param += 1;
		if((Agents[InWParam.i_Param]->Effects & 0x0400)) OutWParam.i_Param += 1;
		myGWCAServer->SetResponse(CA_GetHex, OutWParam);
		break;
	case CA_GetModelAnimation: //Get agent's model animation : Return int/dword
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetModelAnimation); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->ModelAnimation;
		myGWCAServer->SetResponse(CA_GetModelAnimation, OutWParam);
		break;
	case CA_GetEnergy: //Get agent's energy - ONLY WORKS FOR YOURSELF! : Return float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetEnergy); break;}
		OutWParam.f_Param = Agents[InWParam.i_Param]->Energy;
		myGWCAServer->SetResponse(CA_GetEnergy, OutWParam);
		break;
	case CA_GetAgentPtr: //Get pointer to agent : Return ptr
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetAgentPtr); break;}
		OutWParam.d_Param = (DWORD)Agents[InWParam.i_Param];
		myGWCAServer->SetResponse(CA_GetAgentPtr, OutWParam);
		break;
	case CA_GetType: //Get agent type : Return int/long
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetType); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->Type;
		myGWCAServer->SetResponse(CA_GetType, OutWParam);
		break;
	case CA_GetLevel: //Get agent level : Return byte
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetLevel); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->Level;
		myGWCAServer->SetResponse(CA_GetLevel, OutWParam);
		break;
	case CA_GetNameProperties: //Get agent's name properties : Return int/long
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetNameProperties); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->NameProperties;
		myGWCAServer->SetResponse(CA_GetNameProperties, OutWParam);
		break;
	case CA_GetMaxId: //Get max agent id : Return unsigned int/dword
		OutWParam.i_Param = maxAgent;
		myGWCAServer->SetResponse(CA_GetMaxId, OutWParam);
		break;
	case CA_GetMyNearestAgent: //Get nearest agent to yourself : Return int/long
		OutWParam.i_Param = GetNearestAgentToAgent(myId);
		myGWCAServer->SetResponse(CA_GetMyNearestAgent, OutWParam);
		break;
	case CA_GetMyDistanceToAgent: //Get distance between agent and you : Return float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetMyDistanceToAgent); break;}
		OutWParam.f_Param = GetDistanceFromAgentToAgent(myId, InWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetMyDistanceToAgent, OutWParam);
		break;
	case CA_GetNearestAgentToAgent: //Get nearest agent to agent : Return int/long
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetNearestAgentToAgent); break;}
		OutWParam.i_Param = GetNearestAgentToAgent(InWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetNearestAgentToAgent, OutWParam);
		break;
	case CA_GetDistanceFromAgentToAgent: //Get distance from agent (OutWParam.i_Param) to agent (OutLParam.i_Param) : Return float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(InLParam.i_Param == -1){InLParam.i_Param = *(long*)CurrentTarget;}
		else if(InLParam.i_Param == -2){InLParam.i_Param = myId;}

		if(InWParam.i_Param < 1 || (unsigned int)InWParam.i_Param > maxAgent || Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetDistanceFromAgentToAgent); break;}
		if(InLParam.i_Param < 1 || (unsigned int)InLParam.i_Param > maxAgent || Agents[InLParam.i_Param]==NULL)  {SendError(CA_GetDistanceFromAgentToAgent); break;}

		OutWParam.f_Param = GetDistanceFromAgentToAgent(InWParam.i_Param, InLParam.i_Param);
		myGWCAServer->SetResponse(CA_GetDistanceFromAgentToAgent, OutWParam);
		break;
	case CA_GetNearestAgentToAgentEx: //Get nearest agent to agent AND the distance between them : Return int/long & float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetNearestAgentToAgentEx); break;}
		OutWParam.i_Param = GetNearestAgentToAgent(OutWParam.i_Param);
		OutLParam.f_Param = GetDistanceFromAgentToAgent(InWParam.i_Param, OutWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetNearestAgentToAgentEx, OutWParam, OutLParam);
		break;
	case CA_GetModelState: //Get model state of agent : Return int/long
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetModelState); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->ModelState;
		myGWCAServer->SetResponse(CA_GetModelState, OutWParam);
		break;
	case CA_GetIsAttacking: //Check if agent is attacking : Return int/bool
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetIsAttacking); break;}
		if(Agents[InWParam.i_Param]->ModelState == 0x60||
			Agents[InWParam.i_Param]->ModelState == 0x440||
			Agents[InWParam.i_Param]->ModelState == 0x460)
		{
			OutWParam.i_Param = 1;
		}else{
			OutWParam.i_Param = 0;
		}
		myGWCAServer->SetResponse(CA_GetIsAttacking, OutWParam);
		break;
	case CA_GetIsKnockedDown: //Check if agent is knocked down : Return int/bool
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetIsKnockedDown); break;}
		if(Agents[InWParam.i_Param]->ModelState == 0x450){
			OutWParam.i_Param = 1;
		}else{
			OutWParam.i_Param = 0;
		}
		myGWCAServer->SetResponse(CA_GetIsKnockedDown, OutWParam);
		break;
	case CA_GetIsMoving: //Check if agent is moving : Return int/bool
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetIsMoving); break;}
		if(Agents[InWParam.i_Param]->ModelState == 0x0C||
			Agents[InWParam.i_Param]->ModelState == 0x4C||
			Agents[InWParam.i_Param]->ModelState == 0xCC)
		{
			OutWParam.i_Param = 1;
		}else{
			OutWParam.i_Param = 0;
		}
		myGWCAServer->SetResponse(CA_GetIsMoving, OutWParam);
		break;
	case CA_GetIsDead: //Check if agent is dead : Return int/bool
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetIsDead); break;}
		if(Agents[InWParam.i_Param]->ModelState == 0x400 || Agents[InWParam.i_Param]->HP == 0){
			OutWParam.i_Param = 1;
		}else{
			OutWParam.i_Param = 0;
		}
		myGWCAServer->SetResponse(CA_GetIsDead, OutWParam);
		break;
	case CA_GetIsCasting: //Check if agent is casting/using skill : Return int/bool
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetIsCasting); break;}
		if(Agents[InWParam.i_Param]->Skill != NULL){
			OutWParam.i_Param = 1;
		}else{
			OutWParam.i_Param = 0;
		}
		myGWCAServer->SetResponse(CA_GetIsCasting, OutWParam);
		break;
	case CA_GetFirstAgentByPlayerNumber: //Get agent by player number (and the corresponding TeamId) : Return int/long & byte
		OutWParam.i_Param = GetFirstAgentByPlayerNumber(InWParam.i_Param);
		if(Agents[OutWParam.i_Param] == NULL){
			myGWCAServer->SetResponse(CA_GetFirstAgentByPlayerNumber, OutWParam);
		}
		else {
			OutLParam.i_Param = Agents[OutWParam.i_Param]->TeamId;
			myGWCAServer->SetResponse(CA_GetFirstAgentByPlayerNumber, OutWParam, OutLParam);
		}
		break;
	case CA_GetAllegiance: //Get agents allegiance and team : Return word & byte
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetAllegiance); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->Allegiance;
		OutLParam.i_Param = Agents[InWParam.i_Param]->TeamId;
		myGWCAServer->SetResponse(CA_GetAllegiance, OutWParam, OutLParam);
		break;
	case CA_GetNearestEnemyToAgentEx: //Get nearest enemy (by TeamId) to agent and the distance between them : Return int/long & float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetNearestEnemyToAgentEx); break;}
		OutWParam.i_Param = GetNearestEnemyToAgent(InWParam.i_Param);
		OutLParam.f_Param = GetDistanceFromAgentToAgent(InWParam.i_Param, OutWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetNearestEnemyToAgentEx, OutWParam, OutLParam);
		break;
	case CA_GetIsAttackedMelee: //Check if agent is under attack from enemy melee (by TeamId) : Return int
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetIsAttackedMelee); break;}
		OutWParam.i_Param = IsAttackedMelee(InWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetIsAttackedMelee, OutWParam);
		break;
	case CA_GetNearestItemToAgentEx: //Get nearest item to agent : Return int/long & float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetNearestItemToAgentEx); break;}
		OutWParam.i_Param = GetNearestItemToAgent(InWParam.i_Param);
		OutLParam.f_Param = GetDistanceFromAgentToAgent(InWParam.i_Param, OutWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetNearestItemToAgentEx, OutWParam, OutLParam);
		break;
	case CA_GetNearestAgentByPlayerNumber: //Get nearest agent by player number to self : Return int/long & float
		OutWParam.i_Param = GetNearestAgentByPlayerNumber((WORD)myId);
		if(Agents[OutWParam.i_Param] == NULL){
			myGWCAServer->SetResponse(CA_GetNearestAgentByPlayerNumber, OutWParam);
		}else{
			OutLParam.f_Param = GetDistanceFromAgentToAgent(myId, OutWParam.i_Param);
			myGWCAServer->SetResponse(CA_GetNearestAgentByPlayerNumber, OutWParam, OutLParam);
		}
		break;
	case CA_GetSpeed: //Get current speed of agent : Return float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetSpeed); break;}
		OutWParam.f_Param = sqrt(pow(Agents[OutWParam.i_Param]->MoveX, 2) + pow(Agents[OutWParam.i_Param]->MoveY, 2));
		myGWCAServer->SetResponse(CA_GetSpeed, OutWParam);
		break;
	case CA_GetNearestEnemyToAgentByAllegiance: //Get nearest enemy to agent by allegiance and the distance between them : Return int/long & float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetNearestEnemyToAgentByAllegiance); break;}
		OutWParam.i_Param = GetNearestEnemyToAgentByAllegiance(InWParam.i_Param);
		OutLParam.f_Param = GetDistanceFromAgentToAgent(InWParam.i_Param, OutWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetNearestEnemyToAgentByAllegiance, OutWParam, OutLParam);
		break;
	case CA_GetNearestAliveEnemyToAgent: //Get nearest alive enemy to agent and the distance between them : Return int/long & float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetNearestAliveEnemyToAgent); break;}
		OutWParam.i_Param = GetNearestAliveEnemyToAgent(InWParam.i_Param);
		OutLParam.f_Param = GetDistanceFromAgentToAgent(InWParam.i_Param, OutWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetNearestAliveEnemyToAgent, OutWParam, OutLParam);
		break;
	case CA_GetWeaponType: //Get weapon type : Return int/long
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetWeaponType); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->WeaponType;
		myGWCAServer->SetResponse(CA_GetWeaponType, OutWParam);
		break;
	case CA_GetNearestSignpostToAgent: //Get nearest signpost to agent : Return int/long & float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetNearestSignpostToAgent); break;}
		OutWParam.i_Param = GetNearestSignpostToAgent(InWParam.i_Param);
		OutLParam.f_Param = GetDistanceFromAgentToAgent(InWParam.i_Param, OutWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetNearestSignpostToAgent, OutWParam, OutLParam);
		break;
	case CA_GetNearestNpcToAgentByAllegiance: //Get nearest npc to agent by allegiance : Return int/long & float
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetNearestNpcToAgentByAllegiance); break;}
		OutWParam.i_Param = GetNearestNpcToAgentByAllegiance(InWParam.i_Param);
		OutLParam.f_Param = GetDistanceFromAgentToAgent(InWParam.i_Param, OutWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetNearestNpcToAgentByAllegiance, OutWParam, OutLParam);
		break;
	case CA_GetNearestAgentToCoords: //Get nearest agent to coords : Return int/long
		OutWParam.i_Param = GetNearestAgentToCoords(InWParam.f_Param, InLParam.f_Param);
		myGWCAServer->SetResponse(CA_GetNearestAgentToCoords, OutWParam);
		break;
	case CA_GetNearestNpcToCoords: //Get nearest NPC to coords : Return int/long
		OutWParam.i_Param = GetNearestNPCToCoords(InWParam.f_Param, InLParam.f_Param);
		myGWCAServer->SetResponse(CA_GetNearestNpcToCoords, OutWParam);
		break;
	case CA_GetLoginNumber: //Get login number of agent : Return int/long
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		else if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL)  {SendError(CA_GetLoginNumber); break;}
		OutWParam.i_Param = Agents[InWParam.i_Param]->LoginNumber;
		myGWCAServer->SetResponse(CA_GetLoginNumber, OutWParam);
		break;
	case CA_GetNumberOfAgentsByPlayerNumber: //Get number of agents with certain player/model number : Return int/long
		if(InWParam.i_Param == NULL){SendError(CA_GetNumberOfAgentsByPlayerNumber); break;}
		OutWParam.i_Param = GetNumberOfAgentsByPlayerNumber(InWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetNumberOfAgentsByPlayerNumber, OutWParam);
		break;
	case CA_GetNumberOfAliveEnemyAgents: //Get number of alive enemy agents in total : Return int/long
		OutWParam.i_Param = GetNumberOfAliveEnemyAgents();
		myGWCAServer->SetResponse(CA_GetNumberOfAliveEnemyAgents, OutWParam);
		break;
	case CA_GetNextItem: //Get next item in iteration and the distance to it : Return int/long & float
		OutWParam.i_Param = GetNextItem(InWParam.i_Param);
		if(OutWParam.i_Param){
			OutLParam.f_Param = GetDistanceFromAgentToAgent(myId, OutWParam.i_Param);
		}else{
			OutLParam.f_Param = 0;
		}
		myGWCAServer->SetResponse(CA_GetNextItem, OutWParam, OutLParam);
		break;
	case CA_GetTarget: //Get target of agent : Return int/long
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(Agents[InWParam.i_Param]==NULL){SendError(CA_GetTarget); break;}
		OutWParam.i_Param = AgentTargets[InWParam.i_Param];
		myGWCAServer->SetResponse(CA_GetTarget, OutWParam);
		break;
	case CA_SetAttribute: //Set Attribute (Attribute,Value)
		if(InWParam.i_Param < 0 || InWParam.i_Param > 12){SendError(CA_SetAttribute); break;}
		SetAttribute(InWParam.i_Param,InLParam.i_Param);
		break;
	case CA_PlayerHasBuff: //Player has buff : Return bool
		if(InWParam.i_Param < 0 || InWParam.i_Param > 3500){SendError(CA_PlayerHasBuff); break;}
		OutWParam.i_Param = MyBuffHandler.HasBuff(myId, InWParam.i_Param);
		myGWCAServer->SetResponse(CA_Hero1HasBuff, OutWParam);
	case CA_Hero1HasBuff: //Hero1 has buff : Return bool
		if(InWParam.i_Param < 0 || InWParam.i_Param > 3500){SendError(CA_Hero1HasBuff); break;}
		OutWParam.i_Param = MyBuffHandler.HasBuff(*(long*)(MySectionA->HeroesStruct() + 0x4), InWParam.i_Param);
		myGWCAServer->SetResponse(CA_Hero1HasBuff, OutWParam);
		break;
	case CA_Hero2HasBuff: //Hero2 has buff : Return bool
		if(InWParam.i_Param < 0 || InWParam.i_Param > 3500){SendError(CA_Hero2HasBuff); break;}
		OutWParam.i_Param = MyBuffHandler.HasBuff(*(long*)(MySectionA->HeroesStruct() + 0x28), InWParam.i_Param);
		myGWCAServer->SetResponse(CA_Hero1HasBuff, OutWParam);
		break;
	case CA_Hero3HasBuff: //Hero3 has buff : Return bool
		if(InWParam.i_Param < 0 || InWParam.i_Param > 3500){SendError(CA_Hero3HasBuff); break;}
		OutWParam.i_Param = MyBuffHandler.HasBuff(*(long*)(MySectionA->HeroesStruct() + 0x4C), InWParam.i_Param);
		myGWCAServer->SetResponse(CA_Hero1HasBuff, OutWParam);
		break;

		//Item related commands
	case CA_GetGold: //Get gold : Return int/long & int/long
		OutWParam.i_Param = MySectionA->MoneySelf();
		OutLParam.i_Param = MySectionA->MoneyStorage();
		myGWCAServer->SetResponse(CA_GetGold, OutWParam, OutLParam);
		break;
	case CA_GetBagSize: //Get bag size : Return int/long & int/long
		OutWParam.i_Param = MyItemManager->GetBagSize(InWParam.i_Param);
		OutLParam.i_Param = MyItemManager->GetBagItems(InWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetBagSize, OutWParam, OutLParam);
		break;
	case CA_SetBag: //Select bag to work with : No return
		CurrentBag = OutWParam.i_Param;
		break;
	case CA_GetItemId: //Get current bag item id and model id : Return int/long & int/long
		if(!CurrentBag){SendError(CA_GetItemId); break;}
		OutWParam.i_Param = MyItemManager->GetItemId(CurrentBag, InWParam.i_Param);
		OutLParam.i_Param = MyItemManager->GetItemModelId(CurrentBag, InWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetItemId, OutWParam, OutLParam);
		break;
	case CA_GetIdKit: //Get first ID kit item id : Return int/long & int/long
		OutWParam.i_Param = MyItemManager->FindIdKit();
		myGWCAServer->SetResponse(CA_GetIdKit, OutWParam);
		break;
	case CA_IdentifyItem: //Identify item by indexes : No return
		OutWParam.i_Param = MyItemManager->FindIdKit();
		OutLParam.i_Param = MyItemManager->GetItemId(InWParam.i_Param, InLParam.i_Param);
		if(!OutWParam.i_Param || !OutLParam.i_Param){SendError(CA_IdentifyItem); break;}
		IdentifyItem(OutWParam.i_Param, OutLParam.i_Param);
		break;
	case CA_IdentifyItemById: //Identify item by item id : No return
		OutWParam.i_Param = MyItemManager->FindIdKit();
		if(!OutWParam.i_Param){SendError(CA_IdentifyItemById); break;}
		IdentifyItem(OutWParam.i_Param, InWParam.i_Param);
		break;
	case CA_DepositGold: //Deposit gold in storage : No return
		OutWParam.i_Param = MySectionA->MoneySelf();
		OutLParam.i_Param = MySectionA->MoneyStorage();
		if(OutWParam.i_Param == -1){
			if((OutLParam.i_Param + OutWParam.i_Param) > 1000000){ OutWParam.i_Param = 1000000 - OutLParam.i_Param; }
			OutLParam.i_Param += OutWParam.i_Param;
			OutWParam.i_Param = MySectionA->MoneySelf() - OutWParam.i_Param;
		}else{
			OutLParam.i_Param += InWParam.i_Param;
			OutWParam.i_Param -= InWParam.i_Param;
		}
		ChangeGold(OutWParam.i_Param, OutLParam.i_Param);
		break;
	case CA_WithdrawGold: //Withdraw gold from storage : No return
		OutWParam.i_Param = MySectionA->MoneySelf();
		OutLParam.i_Param = MySectionA->MoneyStorage();
		if(InWParam.i_Param == -1){
			if((OutLParam.i_Param - (100000 - OutWParam.i_Param)) < 0){
				OutWParam.i_Param += OutLParam.i_Param;
				OutLParam.i_Param = 0;
			}else{
				OutLParam.i_Param -= 100000 - OutWParam.i_Param;
				OutWParam.i_Param = 100000;
			}
		}else{
			OutLParam.i_Param -= OutWParam.i_Param;
			OutWParam.i_Param += OutWParam.i_Param;
		}
		ChangeGold(OutWParam.i_Param, OutLParam.i_Param);
		break;
	case CA_SellItem: //Sell item by indexes : No return
		OutWParam.i_Param = MyItemManager->GetItemId(InWParam.i_Param, InLParam.i_Param);
		if(!SellSessionId || !OutWParam.i_Param){SendError(CA_SellItem); break;}
		SellItem(OutWParam.i_Param);
		break;
	case CA_SellItemById: //Sell item by item id : No return
		if(!SellSessionId){SendError(CA_SellItemById); break;}
		SellItem(InWParam.i_Param);
		break;
	case CA_BuyIdKit: //Buy ID kit : No return
		if(!MySectionA->MerchantItems()){SendError(CA_BuyIdKit); break;}
		BuyItem(*(long*)(MySectionA->MerchantItems() + 0x10), 1, 100);
		break;
	case CA_BuySuperiorIdKit: //Buy superior ID kit : No return
		if(!MySectionA->MerchantItems()){SendError(CA_BuySuperiorIdKit); break;}
		BuyItem(*(long*)(MySectionA->MerchantItems() + 0x14), 1, 500);
		break;
	case CA_PrepareMoveItem: //Prepare MoveItem by setting item id (internal) : No return
		if(InWParam.i_Param && InLParam.i_Param){
			MoveItemId = MyItemManager->GetItemId(InWParam.i_Param, InLParam.i_Param);
		}else{
			MoveItemId = InWParam.i_Param;
		}
		break;
	case CA_MoveItem: //Move the item specified by CA_PrepareMoveItem : No return
		if(!MoveItemId)  {SendError(CA_MoveItem); break;}
		MoveItem(MoveItemId, MyItemManager->GetBagPtr(InWParam.i_Param)->id, (InLParam.i_Param - 1));
		break;
	case CA_GetItemInfo: //Get current bag item rarity and quantity : Return byte & byte
		if(!CurrentBag || !MyItemManager->GetItemPtr(CurrentBag, InWParam.i_Param)) {SendError(CA_GetItemInfo); break;}
		OutWParam.i_Param = MyItemManager->GetItemPtr(CurrentBag, InWParam.i_Param)->extraItemInfo->rarity;
		OutLParam.i_Param = MyItemManager->GetItemPtr(CurrentBag, InWParam.i_Param)->quantity;
		myGWCAServer->SetResponse(CA_GetItemInfo, OutWParam, OutLParam);
		break;
	case CA_UseItem: //Use item by indexes : No return
		OutWParam.i_Param = MyItemManager->GetItemId(InWParam.i_Param, InLParam.i_Param);
		if(!OutWParam.i_Param){SendError(CA_UseItem); break;}
		UseItem(OutWParam.i_Param);
		break;
	case CA_UseItemById: //Use item by item id : No return
		UseItem(InWParam.i_Param);
		break;
	case CA_DropItem: //Drop item by indexes : No return
		if(MyItemManager->GetItemPtr(InWParam.i_Param, InLParam.i_Param)){
			DropItem(MyItemManager->GetItemId(InWParam.i_Param, InLParam.i_Param),
				MyItemManager->GetItemPtr(InWParam.i_Param, InLParam.i_Param)->quantity);
		}
		break;
	case CA_DropItemById: //Drop item by id and specifying amount : No return
		if(InLParam.i_Param == -1 && MyItemManager->GetItemPtr(InWParam.i_Param)){
			InLParam.i_Param = MyItemManager->GetItemPtr(OutWParam.i_Param)->quantity;
		}
		DropItem(InWParam.i_Param, InLParam.i_Param);
		break;
	case CA_AcceptAllItems: //Accept all unclaimed items : No return
		if(!MyItemManager->GetBagPtr(7)){SendError(CA_AcceptAllItems); break;}
		AcceptAllItems(MyItemManager->GetBagPtr(7)->id);
		break;
	case CA_GetItemLastModifier: //Get current bag item customized and last modifier : Return wchar_t* & byte
		if(!CurrentBag || !MyItemManager->GetItemPtr(CurrentBag, OutWParam.i_Param))  {SendError(CA_GetItemLastModifier); break;}
		OutWParam.d_Param = (dword)MyItemManager->GetItemPtr(CurrentBag, OutWParam.i_Param)->customized;
		OutLParam.i_Param = MyItemManager->GetItemPtr(CurrentBag, OutWParam.i_Param)->extraItemInfo->lastModifier;
		myGWCAServer->SetResponse(CA_GetItemLastModifier, OutWParam, OutLParam);
		break;
	case CA_FindItemByModelId: //Find item by item model id : Return int/long
		if(InWParam.i_Param==NULL) {SendError(CA_FindItemByModelId); break;}
		OutWParam.i_Param = MyItemManager->GetItemByModelId(InWParam.i_Param);
		myGWCAServer->SetResponse(CA_FindItemByModelId, OutWParam);
		break;
	case CA_FindEmptySlot: //Find next empty inventory/storage slot : Return int/long & int/long
		OutWParam.i_Param = MyItemManager->FindEmptySlot(InWParam.i_Param, 1);
		OutLParam.i_Param = MyItemManager->FindEmptySlot(InWParam.i_Param, 2);
		myGWCAServer->SetResponse(CA_FindEmptySlot, OutWParam, OutLParam);
		break;
	case CA_FindGoldItem: //Find next gold item in inventory/storage : Return int/long & int/long
		if(InWParam.i_Param == NULL) {InWParam.i_Param = 15;}
		OutLParam.i_Param = MyItemManager->FindNextGoldItem(InWParam.i_Param);
		OutWParam.i_Param = MyItemManager->GetItemPtr(OutLParam.i_Param)->id;
		myGWCAServer->SetResponse(CA_FindGoldItem, OutWParam, OutLParam);
		break;
	case CA_GetItemPositionByItemId: //Get item position by item id : Return int/long & int/long
		if(InWParam.i_Param == NULL) {SendError(CA_GetItemPositionByItemId); break;}
		OutWParam.i_Param = MyItemManager->GetItemPositionByItemId(InWParam.i_Param, 1);
		OutLParam.i_Param = MyItemManager->GetItemPositionByItemId(InWParam.i_Param, 2);
		myGWCAServer->SetResponse(CA_GetItemPositionByItemId, OutWParam, OutLParam);
		break;
	case CA_GetItemPositionByModelId: //Get item position by model id : Return int/long & int/long
		if(InWParam.i_Param==NULL) {SendError(CA_GetItemPositionByModelId); break;}
		OutWParam.i_Param = MyItemManager->GetItemPositionByModelId(InWParam.i_Param, 1);
		OutLParam.i_Param = MyItemManager->GetItemPositionByModelId(InWParam.i_Param, 2);
		myGWCAServer->SetResponse(CA_GetItemPositionByModelId, OutWParam, OutLParam);
		break;
	case CA_GetItemPositionByRarity: //Get item position by rarity : Return int/long & int/long
		if(InWParam.i_Param==NULL) {SendError(CA_GetItemPositionByRarity); break;}
		OutWParam.i_Param = MyItemManager->GetItemPositionByRarity(InWParam.i_Param, 1);
		OutLParam.i_Param = MyItemManager->GetItemPositionByRarity(InWParam.i_Param, 2);
		myGWCAServer->SetResponse(CA_GetItemPositionByRarity, OutWParam, OutLParam);
		break;
	case CA_GetItemModelIdById: //Get item model id by item id : Return int/long
		if(InWParam.i_Param==NULL){SendError(CA_GetItemModelIdById); break;}
		OutWParam.i_Param = MyItemManager->GetItemModelId(InWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetItemModelIdById, OutWParam);
		break;
	case CA_GetItemInfoById: //Get item rarity and quantity by item id : Return byte & byte
		if(!MyItemManager->GetItemPtr(InWParam.i_Param)) {SendError(CA_GetItemInfoById); break;}
		OutWParam.i_Param = MyItemManager->GetItemPtr(OutWParam.i_Param)->extraItemInfo->rarity;
		OutLParam.i_Param = MyItemManager->GetItemPtr(OutWParam.i_Param)->quantity;
		myGWCAServer->SetResponse(CA_GetItemInfoById, OutWParam, OutLParam);
		break;
	case CA_GetItemLastModifierById: //Get item customized and last modifier : Return wchar_t* & byte
		if(!MyItemManager->GetItemPtr(InWParam.i_Param)) {SendError(CA_GetItemLastModifierById); break;}
		OutWParam.d_Param = (dword)MyItemManager->GetItemPtr(InWParam.i_Param)->customized;
		OutLParam.i_Param = MyItemManager->GetItemPtr(InWParam.i_Param)->extraItemInfo->lastModifier;
		myGWCAServer->SetResponse(CA_GetItemLastModifierById, OutWParam, OutLParam);
		break;
	case CA_EquipItem: //Equip item by indexes : No return
		if(MyItemManager->GetItemPtr(InWParam.i_Param, InLParam.i_Param)){
			EquipItem(MyItemManager->GetItemId(InWParam.i_Param, InLParam.i_Param));
		}
		break;
	case CA_EquipItemById: //Equip item by item id : No return
		EquipItem(InWParam.i_Param);
		break;
	case CA_SalvageItem: //Salvage Item : No return
		//currently not working
		/*if(InWParam.i_Param==NULL){SendError(CA_SalvageItem); break;}
		if(InLParam.i_Param==NULL){
			SalvageItem(InWParam.i_Param, MyItemManager->FindSalvageKit());
		}else{
			SalvageItem(MyItemManager->GetItemId(InWParam.i_Param, InLParam.i_Param), MyItemManager->FindSalvageKit());
		}*/
		break;
	case CA_BuyItem: //Buy item by index and cost : No return
		if(!MySectionA->MerchantItems()){break;}
		if(InWParam.i_Param < 1 || InWParam.i_Param > (int)MySectionA->MerchantItemsSize()){SendError(CA_BuyItem); break;}
		BuyItem(*(long*)(MySectionA->MerchantItems() + ((InWParam.i_Param - 1) * 4)), 1, InLParam.i_Param);
		break;
	case CA_GetSalvageKit: //Find salvage kit : Return int/long
		OutWParam.i_Param = MyItemManager->FindSalvageKit();
		myGWCAServer->SetResponse(CA_GetSalvageKit, OutWParam);
		break;
	case CA_GetItemIdByAgent: //Get item id and model id by agent id : Return int/long & int/long
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(!MyItemManager->GetItemPtrByAgentId(InWParam.i_Param)){SendError(CA_GetItemIdByAgent); break;}
		OutWParam.i_Param = MyItemManager->GetItemPtrByAgentId(InWParam.i_Param)->id;
		OutLParam.i_Param = MyItemManager->GetItemPtrByAgentId(InWParam.i_Param)->modelId;
		myGWCAServer->SetResponse(CA_GetItemIdByAgent, OutWParam, OutLParam);
		break;
	case CA_GetItemInfoByAgent: //Get item rarity and quantity by agent id : Return byte & byte
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(!MyItemManager->GetItemPtrByAgentId(InWParam.i_Param)){SendError(CA_GetItemInfoByAgent); break;}
		OutWParam.i_Param = MyItemManager->GetItemPtrByAgentId(InWParam.i_Param)->extraItemInfo->rarity;
		OutLParam.i_Param = MyItemManager->GetItemPtrByAgentId(InWParam.i_Param)->quantity;
		myGWCAServer->SetResponse(CA_GetItemInfoByAgent, OutWParam, OutLParam);
		break;
	case CA_GetItemLastModifierByAgent: //Get item last modifier and customized by agent id : Return byte & wchar_t*
		if(InWParam.i_Param == -1){InWParam.i_Param = *(long*)CurrentTarget;}
		if(InWParam.i_Param == -2){InWParam.i_Param = myId;}
		if(!MyItemManager->GetItemPtrByAgentId(InWParam.i_Param)){SendError(CA_GetItemLastModifierByAgent); break;}
		OutWParam.i_Param = MyItemManager->GetItemPtrByAgentId(InWParam.i_Param)->extraItemInfo->lastModifier;
		OutLParam.d_Param = (DWORD)MyItemManager->GetItemPtrByAgentId(InWParam.i_Param)->customized;
		myGWCAServer->SetResponse(CA_GetItemLastModifierByAgent, OutWParam, OutLParam);
		break;
	case CA_GetNearestItemByModelId: //Get nearest item by model id : Return int/long
		if(InWParam.i_Param == 0){break;}
		OutWParam.i_Param = GetNearestItemByModelId(InWParam.i_Param);
		OutLParam.f_Param = GetDistanceFromAgentToAgent(myId, OutWParam.i_Param);
		myGWCAServer->SetResponse(CA_GetNearestItemByModelId, OutWParam, OutLParam);
		break;

		//Title related commands
	case CA_GetTitleSunspear: //Get current Sunspear Title: Return int/long
		OutWParam.i_Param = MySectionA->TitleSunspear();
		myGWCAServer->SetResponse(CA_GetTitleSunspear, OutWParam);
		break;
	case CA_GetTitleLightbringer: //Get current Lightbringer Title: Return int/long
		OutWParam.i_Param = MySectionA->TitleLightbringer();
		myGWCAServer->SetResponse(CA_GetTitleLightbringer, OutWParam);
		break;
	case CA_GetTitleVanguard: //Get current Vanguard Title: Return int/long
		OutWParam.i_Param = MySectionA->TitleVanguard();
		myGWCAServer->SetResponse(CA_GetTitleVanguard, OutWParam);
		break;
	case CA_GetTitleNorn: //Get current Norn Title: Return int/long
		OutWParam.i_Param = MySectionA->TitleNorn();
		myGWCAServer->SetResponse(CA_GetTitleNorn, OutWParam);
		break;
	case CA_GetTitleAsura: //Get current Asura Title: Return int/long
		OutWParam.i_Param = MySectionA->TitleAsura();
		myGWCAServer->SetResponse(CA_GetTitleAsura, OutWParam);
		break;
	case CA_GetTitleDeldrimor: //Get current Deldrimor Title: Return int/long
		OutWParam.i_Param = MySectionA->TitleDeldrimor();
		myGWCAServer->SetResponse(CA_GetTitleDeldrimor, OutWParam);
		break;
	case CA_GetTitleNorthMastery: //Get current North Mastery Title: Return int/long
		OutWParam.i_Param = MySectionA->TitleNorthMastery();
		myGWCAServer->SetResponse(CA_GetTitleNorthMastery, OutWParam);
		break;
	case CA_GetTitleDrunkard: //Get current Drunkard Title : Return int/long
		OutWParam.i_Param = MySectionA->TitleDrunkard();
		myGWCAServer->SetResponse(CA_GetTitleDrunkard, OutWParam);
		break;
	case CA_GetTitleSweet: //Get current Sweet Title: Return int/long
		OutWParam.i_Param = MySectionA->TitleSweet();
		myGWCAServer->SetResponse(CA_GetTitleSweet, OutWParam);
		break;
	case CA_GetTitleParty: //Get current Party Title: Return int/long
		OutWParam.i_Param = MySectionA->TitleParty();
		myGWCAServer->SetResponse(CA_GetTitleParty, OutWParam);
		break;
	case CA_GetTitleCommander: //Get current Commander Title: Return int/long
		OutWParam.i_Param = MySectionA->TitleCommander();
		myGWCAServer->SetResponse(CA_GetTitleCommander, OutWParam);
		break;
	case CA_GetTitleLuxon: //Get current Luxon Title Track: Return int/long
		OutWParam.i_Param = MySectionA->TitleTrackLuxon();
		myGWCAServer->SetResponse(CA_GetTitleLuxon, OutWParam);
		break;
	case CA_GetTitleKurzick: //Get current Kurzick Title Track: Return int/long
		OutWParam.i_Param = MySectionA->TitleTrackKurzick();
		myGWCAServer->SetResponse(CA_GetTitleKurzick, OutWParam);
		break;

		//More general commands
	case CA_AddHero: //Add hero to party: No Return
		AddHero(InWParam.i_Param);
		break;
	case CA_KickHero: //Kick hero from party : No Return
		KickHero(InWParam.i_Param);
		break;
	case CA_SwitchMode: //Switch between NM and HM: No Return
		SwitchMode(InWParam.i_Param != 0);
		break;
	case CA_AddNpc: //Add henchman to party : No Return
		AddNpc(InWParam.i_Param);
		break;
	case CA_KickNpc: //Kick henchman from party : No Return
		KickNpc(InWParam.i_Param);
		break;
	case CA_TravelGH: //Travel to Guild Hall : No return
		TravelGH();
		break;
	case CA_LeaveGH: //Leave Guild Hall : No return
		LeaveGH();
		break;
	case CA_InitMapLoad: //Init Map Load : No return
		FinishedLoading = false;
		break;
	case CA_MapIsLoaded:  //Map Is Loaded : Return bool/int
		if(FinishedLoading == true){
			OutWParam.i_Param = 1;
		}else{
			OutWParam.i_Param = 0;
		}
		myGWCAServer->SetResponse(CA_MapIsLoaded, OutWParam);
		break;
	case CA_GetMapOverlayCoords: //Get coordinates of MapOverlay by index : Return float & float
		if(InWParam.i_Param < 1 || InWParam.i_Param > (MySectionA->MapOverlaySize() - 1)) {SendError(CA_GetMapOverlayCoords); break;}
		if(MySectionA->MapOverlayPointer() == NULL) {SendError(CA_GetMapOverlayCoords); break;}
		OutWParam.f_Param = MySectionA->MapOverlayPointer()[InWParam.i_Param].X;
		OutLParam.f_Param = MySectionA->MapOverlayPointer()[InWParam.i_Param].Y;
		myGWCAServer->SetResponse(CA_GetMapOverlayCoords, OutWParam, OutLParam);
		break;
	case CA_GetMapOverlayInfo: //Get MapOverlay option and model id by index : Return int/long
		if(InWParam.i_Param < 1 || InWParam.i_Param > (MySectionA->MapOverlaySize() - 1)){SendError(CA_GetMapOverlayInfo); break;}
		if(MySectionA->MapOverlayPointer() == NULL) {SendError(CA_GetMapOverlayInfo); break;}
		OutWParam.i_Param = MySectionA->MapOverlayPointer()[InWParam.i_Param].option;
		OutLParam.i_Param = MySectionA->MapOverlayPointer()[InWParam.i_Param].modelId;
		myGWCAServer->SetResponse(CA_GetMapOverlayInfo, OutWParam, OutLParam);
		break;
	case CA_GetNearestMapOverlayToCoords: //Get nearest MapOverlay to coordinates : No return (use 0x47E to return)
		OutWParam.i_Param = GetNearestMapOverlayToCoords(InWParam.f_Param, InLParam.f_Param);
		myGWCAServer->SetResponse(CA_GetNearestMapOverlayToCoords, OutWParam);
		break;
	case CA_GetPartyInfo: //Get party info : Return in WM_COPYDATA
		SendPartyInfo((HWND)InLParam.i_Param, InWParam.i_Param);
		break;
	case CA_ClearPacketQueue: //Clear PacketQueue : Return int/long
		if(WaitForSingleObject(PacketMutex, 200) == WAIT_TIMEOUT) {SendError(CA_ClearPacketQueue); break;}
		OutWParam.i_Param = PacketQueue.size();
		PacketQueue.clear();
		ReleaseMutex(PacketMutex);
		break;
	case CA_SetHeroMode: //Set hero combat mode : No return
		switch(InWParam.i_Param){
		case 1:
			InWParam.i_Param = *(long*)(MySectionA->HeroesStruct() + 0x4); break;
		case 2:
			InWParam.i_Param = *(long*)(MySectionA->HeroesStruct() + 0x28); break;
		case 3:
			InWParam.i_Param = *(long*)(MySectionA->HeroesStruct() + 0x4C); break;
		}
		SetHeroMode(InWParam.i_Param, InLParam.i_Param);
		break;
	case CA_QuestCheck: //Check if you have a certain quest in your log : Return int/long
		if(InWParam.i_Param == -1) {InWParam.i_Param = MySectionA->ActiveQuest();}
		if(InWParam.i_Param == NULL) {SendError(CA_QuestCheck); break;}
		OutWParam.d_Param = 0;
		OutLParam.d_Param = 0;
		for(MsgLoop = 0;MsgLoop < MySectionA->QuestLogSize();MsgLoop++){
			if(MySectionA->QuestLogPointer()[MsgLoop].Id == InWParam.i_Param){
				OutWParam.i_Param = InWParam.i_Param;
				OutLParam.i_Param = MySectionA->QuestLogPointer()[MsgLoop].LogState;
				break;
			}
		}
		myGWCAServer->SetResponse(CA_QuestCheck, OutWParam, OutLParam);
		break;
	case CA_QuestCoords: //Get coordinates of quest marker : Return float & float
		if(InWParam.i_Param == -1) {InWParam.i_Param = MySectionA->ActiveQuest();}
		if(InWParam.i_Param == NULL) {SendError(CA_QuestCoords); break;}
		OutWParam.f_Param = 0.0f;
		OutLParam.f_Param = 0.0f;
		for(MsgLoop = 0;MsgLoop < MySectionA->QuestLogSize();MsgLoop++){
			if(MySectionA->QuestLogPointer()[MsgLoop].Id == InWParam.i_Param){
				OutWParam.f_Param = MySectionA->QuestLogPointer()[MsgLoop].X;
				OutLParam.f_Param = MySectionA->QuestLogPointer()[MsgLoop].Y;
				break;
			}
		}
		myGWCAServer->SetResponse(CA_QuestCoords, OutWParam, OutLParam);
		break;
	case CA_QuestActive: //Get id of currently active quest : Return int/long
		OutWParam.i_Param = MySectionA->ActiveQuest();
		myGWCAServer->SetResponse(CA_QuestActive, OutWParam);
		break;
	case CA_QuestAbandon: //Abandon specified quest : No return
		if(InWParam.i_Param == -1){InWParam.i_Param = MySectionA->ActiveQuest();}
		if(InWParam.i_Param == NULL){SendError(CA_QuestAbandon); break;}
		AbandonQuest(InWParam.i_Param);
		break;
	case CA_SetTeamSize: //Set team size to use with SendPartyInfo
		if(InWParam.i_Param < 0 || InWParam.i_Param > 8){SendError(CA_SetTeamSize); break;}
		PartyTeamSize = InWParam.i_Param;
		break;
	case CA_AllocMem: //Alloc Mem. Returns ptr
		if(InWParam.i_Param == 0){SendError(CA_AllocMem); break;}
		OutWParam.d_Param = (DWORD)malloc(InWParam.i_Param);
		myGWCAServer->SetResponse(CA_AllocMem, OutWParam);
		break;
	case CA_FreeMem: //Free Mem.
		if(InWParam.i_Param == 0){SendError(CA_FreeMem); break;}
		free((byte*)InWParam.d_Param);
		break;
	case CA_GetRegionAndLanguage: //Get region and language numbers : Return int/long & int/long
		OutWParam.i_Param = MySectionA->Region();
		OutLParam.i_Param = MySectionA->Language();
		myGWCAServer->SetResponse(CA_GetRegionAndLanguage, OutWParam, OutLParam);
		break;
	case CA_TraderRequest: //Request quote from trader : No return
		if(!MySectionA->MerchantItems() || !MySectionA->MerchantItemsSize()){SendError(CA_TraderRequest); break;}
		if(InWParam.i_Param > (int)MySectionA->MerchantItemsSize()){SendError(CA_TraderRequest); break;}
		RequestQuote(*(long*)(MySectionA->MerchantItems() + ((InWParam.i_Param - 1) * 4)));
		break;
	case CA_TraderCheck: //Check TraderCostId and TraderCostValue : Return int/long & int/long
		OutWParam.i_Param = TraderCostId;
		OutLParam.i_Param = TraderCostValue;
		myGWCAServer->SetResponse(CA_TraderCheck, OutWParam, OutLParam);
		break;
	case CA_TraderBuy: //Buy item from trader : Return int/boolean
		OutWParam.d_Param = TraderBuy() ? 1 : 0;
		myGWCAServer->SetResponse(CA_TraderBuy, OutWParam);
		break;
	case CA_TraderRequestSell: //Request quote on item by index : No return
		if(!InWParam.i_Param || !InLParam.i_Param){SendError(CA_TraderRequestSell); break;}
		RequestQuoteSell(MyItemManager->GetItemId(InWParam.i_Param, InLParam.i_Param));
		break;
	case CA_TraderRequestSellById: //Request quote on item by id : No return
		if(!InWParam.i_Param){SendError(CA_TraderRequestSellById); break;}
		RequestQuoteSell(InWParam.i_Param);
		break;
	case CA_TraderSell: //Sell item to trader : Return int/boolean
		OutWParam.d_Param = TraderSell() ? 1 : 0;
		myGWCAServer->SetResponse(CA_TraderSell, OutWParam);
		break;
	}
}

void HandleCommands()
{
	while (myGWCAServer->GetCommand())
	{
		HandleMessages(myGWCAServer->GetCommandHeader(), myGWCAServer->GetCommandWParam(), myGWCAServer->GetCommandLParam());
	}
}

void _declspec(naked) EngineHookEx(){
	_asm pushad;

	HandleCommands();

	_asm popad
	_asm MOV EBP,ESP
	_asm SUB ESP,0x14
	_asm JMP EngineExRet
}

void ReloadSkillbar(){
	MySkillbar = (Skillbar*)(MySectionA->SkillbarPtr());
}

void SellItem(long itemId){
	SellItemStruct* pSell = new SellItemStruct;
	pSell->sessionId = SellSessionId;
	pSell->itemId = itemId;

	_asm {
		MOV ECX,pSell
		CALL SellItemFunction
	}

	delete[] pSell;
}

void BuyItem(long id, long quantity, long value){
	long* itemQuantity = &quantity;
	long* itemId = &id;

	_asm {
		PUSH itemQuantity
		PUSH itemId
		PUSH 1
		PUSH 0
		PUSH 0
		PUSH 0
		PUSH 0
		MOV EDX,value
		MOV ECX,1
		CALL BuyItemFunction
	}
}

void RequestQuote(long id){
	long* item = &id;
	
	TraderCostId = 0;
	TraderCostValue = 0;
	
	_asm {
		PUSH item
		PUSH 1
		PUSH 0
		PUSH 0
		PUSH 0
		PUSH 0
		MOV ECX,0xC
		MOV EDX,0
		CALL RequestQuoteFunction
	}
}

void RequestQuoteSell(long id){
	long* item = &id;

	TraderCostId = 0;
	TraderCostValue = 0;
	
	_asm {
		PUSH 0
		PUSH 0
		PUSH 0
		PUSH item
		PUSH 1
		PUSH 0
		MOV ECX,0xD
		MOV EDX,0
		CALL RequestQuoteFunction
	}
}

bool TraderBuy(){
	if(!TraderCostId || !TraderCostValue){ return false; }
	
	long* info = new long[2];
	info[0] = TraderCostId;
	info[1] = TraderCostValue;
	
	_asm {
		PUSH 0
		PUSH info
		PUSH 1
		PUSH 0
		PUSH 0
		PUSH 0
		PUSH 0
		MOV ECX,0xC
		MOV EDX,TraderCostValue
		CALL TraderFunction
	}
	
	TraderCostId = 0;
	TraderCostValue = 0;
	
	return true;
}

bool TraderSell(){
	if(!TraderCostId || !TraderCostValue){ return false; }
	
	long* info = new long[2];
	info[0] = TraderCostId;
	info[1] = TraderCostValue;
	
	_asm {
		PUSH 0
		PUSH 0
		PUSH 0
		PUSH TraderCostValue
		PUSH 0
		PUSH info
		PUSH 1
		MOV ECX,0xD
		MOV EDX,0
		CALL TraderFunction
	}
	
	TraderCostId = 0;
	TraderCostValue = 0;
	
	return true;
}

void SendPartyInfo(HWND hwndReceiver, long teamId, long teamSize){
	PartyInfo* PtInfo = new PartyInfo;
	memset(PtInfo, '\0', sizeof(PartyInfo));

	__try {
		PtInfo->HwndReceiver = hwndReceiver;
		PtInfo->TeamId = teamId;
		PtInfo->TeamSize = teamSize;

		TeamAgents.clear();
		for(unsigned int i = 1;i < 30;i++){
			if(TeamAgents.size() == teamSize)
				break;

			long Agent = GetFirstAgentByPlayerNumber(i);
			if(Agents[Agent] == NULL)
				continue;
			if(Agents[Agent]->TeamId == teamId && Agents[Agent]->LoginNumber != 0)
				TeamAgents.push_back(Agent);
		}

		for(unsigned int i = 0;i < TeamAgents.size();i++){
			if(!Agents[TeamAgents[i]])
				continue;

			PtInfo->Players[i].AgentId = TeamAgents[i];
			PtInfo->Players[i].Effects = Agents[TeamAgents[i]]->Effects;
			PtInfo->Players[i].Hex = 0;
			if((Agents[TeamAgents[i]]->Effects & 0x0800)) PtInfo->Players[i].Hex += 1;
			if((Agents[TeamAgents[i]]->Effects & 0x0400)) PtInfo->Players[i].Hex += 1;
			PtInfo->Players[i].X = Agents[TeamAgents[i]]->X;
			PtInfo->Players[i].Y = Agents[TeamAgents[i]]->Y;
			PtInfo->Players[i].HP = Agents[TeamAgents[i]]->HP;
			PtInfo->Players[i].NamePtr = GetAgentName(TeamAgents[i]);
			PtInfo->Players[i].Primary = Agents[TeamAgents[i]]->Primary;
			PtInfo->Players[i].Secondary = Agents[TeamAgents[i]]->Secondary;
			PtInfo->Players[i].Target = AgentTargets[TeamAgents[i]];
			PtInfo->Players[i].Skill = Agents[TeamAgents[i]]->Skill;
			PtInfo->Players[i].Weapon = Agents[TeamAgents[i]]->WeaponType;
		}

		if(WaitForSingleObject(PartyMutex, 1000) != WAIT_TIMEOUT){
			PartyInfoQueue.push_back(PtInfo);
			ReleaseMutex(PartyMutex);
		}else{
			delete PtInfo;
		}
	}
	__except(1) {
		delete PtInfo;
		return;
	}
}

void WriteWhisper(const wchar_t* chatMsg, const wchar_t* chatName){
	 _asm {
		MOV EAX,chatMsg
		MOV EDX,chatName
		MOV ECX,0
		PUSH EAX
		CALL WriteWhisperStart
		MOV EAX,1
	}
}

wchar_t* GetAgentName(int agentId){
	__try {
		wchar_t* NamePtr = ReadPtrChain<wchar_t*>(MySectionA->BasePointer(), 0x18, 0x2C, 0x794,
			(Agents[agentId]->LoginNumber * 76) + 0x28);
		return NamePtr;
	}
	__except(1) {
		return NULL;
	}
}

void TargetNearestFoe(){
	_asm {
		MOV EBX,0
		MOV EAX,TargetFunctions
		CALL EAX
	}
}

void TargetNearestAlly(){
	_asm {
		MOV EBX,0
		MOV EAX,TargetFunctions
		ADD EAX,0x1D
		CALL EAX
	}
}

void TargetNearestItem(){
	_asm {
		MOV EBX,0
		MOV EAX,TargetFunctions
		ADD EAX,0x3A
		CALL EAX
	}
}

void TargetCalledTarget(){
	_asm {
		MOV EBX,0
		MOV EAX,TargetFunctions
		ADD EAX,0x115
		CALL EAX
	}
}

void TargetNextPartyMember(){
	_asm {
		MOV ECX,0
		MOV EAX,TargetFunctions
		ADD EAX,0x63
		CALL EAX
	}
}

void TargetNextFoe(){
	_asm {
		MOV EAX,TargetFunctions
		ADD EAX,0x57
		CALL EAX
	}
}

void GoAgent(long agentId){
	byte* pGoAgent = (byte*)((TargetFunctions - 0x616) + *(dword*)(TargetFunctions - 0x616) + 4);

	_asm {
		MOV ECX,agentId
		MOV EDX,1
		CALL pGoAgent
	}
}

void UseHeroSkill(long HeroId, long SkillNumber, long Target){
	_asm {
		MOV EDX,SkillNumber
		MOV ECX,HeroId
		PUSH Target
		CALL HeroSkillFunction
	}
}

void UseSkillNew(long SkillId, long Target, long Event){
	SkillType* tmpSkillType = (SkillType*)(SkillTypeBase + (SkillId * 160));
	if(tmpSkillType == NULL){return;}
	if(tmpSkillType->Type == GW_SKILL_TYPE_ATTACKS){
		UseAttackSkill(SkillId, Target, Event);
	}else{
		UseSkill(SkillId, Target, Event);
	}
}

bool CompareAccName(wchar_t* cmpName){
	if(wcscmp(cmpName, MySectionA->Email()) == NULL)
		return true;
	else
		return false;
}

bool CompareCharName(wchar_t* cmpName){
	if(wcscmp(cmpName, MySectionA->Name()) == NULL)
		return true;
	else
		return false;
}

void ChangeMaxZoom(float fZoom){
	DWORD dwOldProtection;
	VirtualProtect(MaxZoomStill, sizeof(float), PAGE_EXECUTE_READWRITE, &dwOldProtection);
	memcpy(MaxZoomStill, &fZoom, sizeof(float));
	VirtualProtect(MaxZoomStill, sizeof(float), dwOldProtection, NULL);
	VirtualProtect(MaxZoomMobile, sizeof(float), PAGE_EXECUTE_READWRITE, &dwOldProtection);
	memcpy(MaxZoomMobile, &fZoom, sizeof(float));
	VirtualProtect(MaxZoomStill, sizeof(float), dwOldProtection, NULL);
}

void SetEngineHook(int Enable){
	if(Enable){
		DWORD dwOldProtection;
		VirtualProtect(EngineStart, 2, PAGE_EXECUTE_READWRITE, &dwOldProtection);
		memset(EngineStart, 0x90, 2);
		VirtualProtect(EngineStart, 2, dwOldProtection, NULL);
		WriteJMP(EngineStart+0x13, (byte*)EngineHook);
	}else{
		DWORD dwOldProtection;
		VirtualProtect(EngineStart, 32, PAGE_EXECUTE_READWRITE, &dwOldProtection);
		memcpy(EngineStart, EngineHookSave, 32);
		VirtualProtect(EngineStart, 32, dwOldProtection, NULL);
	}
}

void SendPacket(CPacket* pak){
	if(WaitForSingleObject(PacketMutex, 1000) == WAIT_TIMEOUT) return;
	PacketQueue.push_back(pak);
	ReleaseMutex(PacketMutex);
}

template <typename T> T ReadPtrChain(dword pBase, long pOffset1, long pOffset2, long pOffset3, long pOffset4){
	dword pRead = pBase;
	if(pRead == NULL){return 0;}

	if(pOffset1){pRead = *(dword*)(pRead + pOffset1);}
	if(pRead == NULL){return 0;}

	if(pOffset2){pRead = *(dword*)(pRead + pOffset2);}
	if(pRead == NULL){return 0;}

	if(pOffset3){pRead = *(dword*)(pRead + pOffset3);}
	if(pRead == NULL){return 0;}

	if(pOffset4){pRead = *(dword*)(pRead + pOffset4);}
	if(pRead == NULL){return 0;}

	return (T)pRead;
}

void SendPacketQueueThread(){
	while(true){
		Sleep(10);

		if(WaitForSingleObject(PacketMutex, 100) == WAIT_TIMEOUT) continue;
		if(PacketQueue.size() < 1 || mapLoading == 2) goto nextLoop;
		if(MySectionA->LoggedIn() != 1 && mapLoading != 1) goto nextLoop;

		{
			std::vector<CPacket*>::iterator itrPak = PacketQueue.begin();
			CPacket* CurPacket = *itrPak;

			//dword testValue = 0x99;
			//_asm {
			//	MOV ECX, FlagLocation
			//	MOV ECX, DWORD PTR DS:[ECX]
			//	MOVZX ECX, BYTE PTR DS:[ECX+8]
			//	MOV testValue, ECX
			//}
			//if((testValue & 1)){
				{
					byte* buffer = CurPacket->Buffer;
					dword psize = CurPacket->Size;
				
					_asm {
						MOV EAX, PacketLocation
						MOV EAX, DWORD PTR DS:[EAX]
						MOV ECX, EAX
						MOV EDX, psize
						PUSH buffer
						CALL PacketSendFunction
					}
				}

				delete [] CurPacket->Buffer;
				delete CurPacket;
			//}

			PacketQueue.erase(itrPak);
		}

nextLoop:
		ReleaseMutex(PacketMutex);
	}
}

void SkillLogQueueThread(){
	COPYDATASTRUCT SkillLogCDS;
	COPYDATASTRUCT PartyInfoCDS;
	LoggedSkillStruct SkillInfo;

	SkillLogCDS.dwData = 1;
	SkillLogCDS.lpData = &SkillInfo;
	SkillLogCDS.cbData = sizeof(LoggedSkillStruct);

	PartyInfoCDS.dwData = 2;
	PartyInfoCDS.cbData = sizeof(PartyInfo);

	wchar_t* sWindowText = new wchar_t[50];
	dword tTicks = 0;

	while(true){
		Sleep(10);

		if(SkillLogQueue.size() > 0 && LogSkills){
			SkillInfo.AgentId = SkillLogQueue.front().AgentId;
			
			if(Agents[SkillInfo.AgentId] != NULL){
				SkillInfo.TeamId = Agents[SkillInfo.AgentId]->TeamId;
				SkillInfo.Allegiance = Agents[SkillInfo.AgentId]->Allegiance;
			}else{
				SkillInfo.TeamId = 0;
				SkillInfo.Allegiance = 0;
			}

			SkillInfo.Distance = GetDistanceFromAgentToAgent(myId, SkillInfo.AgentId);
			SkillInfo.MyId = myId;
			SkillInfo.SkillId = SkillLogQueue.front().Skill;
			SkillInfo.Activation = SkillLogQueue.front().Activation;
			SkillInfo.Ping = MySectionA->Ping();
			SkillInfo.TargetId = AgentTargets[SkillInfo.AgentId];

			SendMessage(ScriptHwnd, WM_COPYDATA, 0, (LPARAM)(LPVOID)&SkillLogCDS);

			SkillLogQueue.erase(SkillLogQueue.begin()); //Remove handled skill from queue
		}else{
			SkillLogQueue.clear();
		}

		if(SkillCancelQueue.size() > 0 && LogSkills){
			PostMessage(ScriptHwnd, 0x501, SkillCancelQueue.front().AgentId, SkillCancelQueue.front().Skill);
			SkillCancelQueue.erase(SkillCancelQueue.begin());
		}else{
			SkillCancelQueue.clear();
		}

		if(WaitForSingleObject(PartyMutex, 50) != WAIT_TIMEOUT){
			if(PartyInfoQueue.size() > 0){
				PartyInfoCDS.lpData = PartyInfoQueue.front();
				SendMessage(PartyInfoQueue.front()->HwndReceiver, WM_COPYDATA, 0, (LPARAM)(LPVOID)&PartyInfoCDS);
				delete PartyInfoQueue.front();
				PartyInfoQueue.erase(PartyInfoQueue.begin());
			}
			ReleaseMutex(PartyMutex);
		}

		if(*(HWND*)WinHandle){
			if((GetTickCount() - tTicks) > 5000){
				tTicks = GetTickCount();

				if(!MySectionA->Name()[0]){
					SetWindowTextW(*(HWND*)WinHandle, L"Guild Wars");
				}else{
					swprintf(sWindowText, 50, L"Guild Wars - %s", MySectionA->Name());
					SetWindowTextW(*(HWND*)WinHandle, sWindowText);
				}
			}
		}
	}
}

void FindOffsets(){
	byte* start = (byte*)0x00401000;
	byte* end = (byte*)0x00900000;

	byte PacketSendCode[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x2C, 0x53, 0x56, 0x57, 0x8B,
		0xF9, 0x85 };

	byte BaseOffsetCode[] = { 0x56, 0x33, 0xF6, 0x3B, 0xCE, 0x74, 0x0E, 0x56, 0x33, 0xD2 };

	byte AgentBaseCode[] = { 0x56, 0x8B, 0xF1, 0x3B, 0xF0, 0x72, 0x04 };

	byte MessageHandlerCode[] = { 0x8B, 0x86, 0xA4, 0x0C, 0x00, 0x00, 0x85, 0xC0, 0x0F };

	byte SkillLogCode[] = { 0x8B, 0x46, 0x10, 0x5F, 0x40 };

	byte MapIdLocationCode[] = { 0xB0, 0x7F, 0x8D, 0x55 };

	byte WriteWhisperCode[] = { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x89, 0x4D, 0xFC, 0x8B, 0x4D,
		0x08, 0x56, 0x57, 0x8B };

	byte TargetFunctionsCode[] = { 0xBA, 0x01, 0x00, 0x00, 0x00, 0xB9, 0x00, 0x80, 0x00, 0x00,
		0xE8 };

	byte HeroSkillFunctionCode[] = { 0x5E, 0xC3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x50 };

	byte ClickToMoveCode[] = { 0x3D, 0xD3, 0x01, 0x00, 0x00, 0x74 };

	byte BuildNumberCode[] = { 0x8D, 0x85, 0x00, 0xFC, 0xFF, 0xFF, 0x8D };

	byte ChangeTargetCode[] = { 0x33, 0xC0, 0x3B, 0xDA, 0x0F, 0x95, 0xC0, 0x33 };

	byte MaxZoomStillCode[] = { 0x3B, 0x44, 0x8B, 0xCB };

	byte MaxZoomMobileCode[] = { 0x50, 0xEB, 0x11, 0x68, 0x00, 0x80, 0x3B, 0x44, 0x8B, 0xCE };

	byte SkillCancelCode[] = { 0x85, 0xC0, 0x74, 0x1D, 0x6A, 0x00, 0x6A, 0x42 };

	byte SellSessionCode[] = { 0x33, 0xD2, 0x8B, 0xCF, 0xC7, 0x46, 0x0C };

	byte SellItemCode[] = { 0x8B, 0x46, 0x0C, 0x8D, 0x7E, 0x0C, 0x85 };

	byte BuyItemCode[] = { 0x64, 0x8B, 0x0D, 0x2C, 0x00, 0x00, 0x00, 0x89, 0x55, 0xFC,
		0x8B };

	byte PingLocationCode[] = { 0x90, 0x8D, 0x41, 0x24, 0x8B, 0x49, 0x18, 0x6A, 0x30 };

	byte LoggedInLocationCode[] = { 0x85, 0xC0, 0x74, 0x11, 0xB8, 0x07 };

	byte NameLocationCode[] = { 0x6A, 0x14, 0x8D, 0x96, 0xBC };

	byte DeadLocationCode[] = { 0x85, 0xC0, 0x74, 0x11, 0xB8, 0x02 };

	byte BasePointerLocationCode[] = { 0x85, 0xC9, 0x74, 0x3D, 0x8B, 0x46 };

	byte DialogCode[] = { 0x55, 0x8B, 0xEC, 0x8B, 0x41, 0x08, 0xA8, 0x01, 0x75, 0x24 };

	byte EngineCode[] = { 0x53, 0x56, 0xDF, 0xE0, 0xF6, 0xC4, 0x41 };

	byte SkillTypeBaseCode[] = { 0x8D, 0x04, 0xB6, 0x5E, 0xC1, 0xE0, 0x05, 0x05 };

	byte WinHandleCode[] = { 0x56, 0x8B, 0xF1, 0x85, 0xC0, 0x89, 0x35 };

	byte LoadFinishedCode[] = { 0x89, 0x4D, 0xD8, 0x8B, 0x4D, 0x0C, 0x89, 0x55, 0xDC, 0x8B };

	byte TargetLogCode[] = { 0x53, 0x89, 0x4D, 0xF4, 0x56, 0x64 };
	
	byte SetAttrisCode[] = { 0x8B, 0x56, 0x08, 0x8D, 0x46, 0x3C, 0x8D, 0x4E, 0x0C, 0x50, 0x51, 0x8B, 0xCF, 0xE8 };

	byte LoginCode[] = { 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00, 0x56, 0x57 };

	byte RegionLocationCode[] = { 0x83, 0xF9, 0xFD, 0x74, 0x06 };

	byte LanguageLocationCode[] = { 0xC3, 0x8B, 0x75, 0xFC, 0x8B, 0x04, 0xB5 };

	byte RequestQuoteCode[] = { 0x81, 0xEC, 0x9C, 0x00, 0x00, 0x00, 0x53, 0x56, 0x8B };

	byte TraderCostCode[] = { 0x89, 0x55, 0xFC, 0x6A, 0x00, 0x8D, 0x55, 0xF8, 0xB9, 0xB8 };

	byte TraderFunctionCode[] = { 0x8B, 0x45, 0x18, 0x8B, 0x55, 0x10, 0x85 };

	while(start!=end){
		if(!memcmp(start, AgentBaseCode, sizeof(AgentBaseCode))){
			AgentArrayPtr = (byte*)(*(dword*)(start+0xC));
			AgentArrayMaxPtr = AgentArrayPtr+0x8;
			CurrentTarget = AgentArrayPtr-0x500;
		}
		if(!memcmp(start, BaseOffsetCode, sizeof(BaseOffsetCode))){
			BaseOffset = start;
		}
		if(!memcmp(start, PacketSendCode, sizeof(PacketSendCode))){
			PacketSendFunction = start;
		}
		if(!memcmp(start, MessageHandlerCode, sizeof(MessageHandlerCode))){
			MessageHandlerStart = start-0x95;
			MessageHandlerReturn = MessageHandlerStart+9;
		}
		if(!memcmp(start, SkillLogCode, sizeof(SkillLogCode))){
			SkillLogStart = start;
			SkillLogReturn = SkillLogStart+8;
		}
		if(!memcmp(start, MapIdLocationCode, sizeof(MapIdLocationCode))){
			MapIdLocation = (byte*)(*(dword*)(start+0x46));
		}
		if(!memcmp(start, WriteWhisperCode, sizeof(WriteWhisperCode))){
			WriteWhisperStart = start;
		}
		if(!memcmp(start, TargetFunctionsCode, sizeof(TargetFunctionsCode))){
			TargetFunctions = start;
		}
		if(!memcmp(start, HeroSkillFunctionCode, sizeof(HeroSkillFunctionCode))){
			HeroSkillFunction = start+0xC;
		}
		if(!memcmp(start, ClickToMoveCode, sizeof(ClickToMoveCode))){
			ClickToMoveFix = start;
		}
		if(!memcmp(start, BuildNumberCode, sizeof(BuildNumberCode))){
			BuildNumber = start+0x53;
		}
		if(!memcmp(start, ChangeTargetCode, sizeof(ChangeTargetCode))){
			ChangeTargetFunction = start-0x78;
		}
		if(!memcmp(start, MaxZoomStillCode, sizeof(MaxZoomStillCode))){
			MaxZoomStill = start-2;
		}
		if(!memcmp(start, MaxZoomMobileCode, sizeof(MaxZoomMobileCode))){
			MaxZoomMobile = start+4;
		}
		if(!memcmp(start, SkillCancelCode, sizeof(SkillCancelCode))){
			SkillCancelStart = start-0xE;
			SkillCancelReturn = SkillCancelStart+7;
		}
		if(!memcmp(start, SellSessionCode, sizeof(SellSessionCode))){
			SellSessionStart = start-0x48;
			SellSessionReturn = SellSessionStart+9;
		}
		if(!memcmp(start, SellItemCode, sizeof(SellItemCode))){
			SellItemFunction = start-8;
		}
		if(!memcmp(start, BuyItemCode, sizeof(BuyItemCode))){
			BuyItemFunction = start-0xE;
		}
		if(!memcmp(start, PingLocationCode, sizeof(PingLocationCode))){
			PingLocation = (byte*)(*(dword*)(start-9));
		}
		if(!memcmp(start, LoggedInLocationCode, sizeof(LoggedInLocationCode))){
			LoggedInLocation = (byte*)(*(dword*)(start-4) + 4);
		}
		if(!memcmp(start, NameLocationCode, sizeof(NameLocationCode))){
			NameLocation = (byte*)(*(dword*)(start+9));
			EmailLocation = (byte*)(*(dword*)(start-9));
		}
		if(!memcmp(start, DeadLocationCode, sizeof(DeadLocationCode))){
			DeadLocation = (byte*)(*(dword*)(start-4));
		}
		if(!memcmp(start, BasePointerLocationCode, sizeof(BasePointerLocationCode))){
			BasePointerLocation = (byte*)(*(dword*)(start-4));
		}
		if(!memcmp(start, DialogCode, sizeof(DialogCode))){
			DialogStart = start;
			DialogReturn = DialogStart+8;
		}
		if(!memcmp(start, EngineCode, sizeof(EngineCode))){
			EngineStart = start+0x65;
			EngineExStart = EngineStart-0x76;
			EngineExRet = EngineExStart + 5;
			memcpy(EngineHookSave, EngineStart, 0x20);
		}
		if(!memcmp(start, SkillTypeBaseCode, sizeof(SkillTypeBaseCode))){
			SkillTypeBase = (byte*)(*(dword*)(start+8));
		}
		if(!memcmp(start, WinHandleCode, sizeof(WinHandleCode))){
			WinHandle = (byte*)(*(dword*)(start+7));
		}
		if(!memcmp(start, LoadFinishedCode, sizeof(LoadFinishedCode))){
			LoadFinished = start+0x4E;
		}
		if(!memcmp(start, TargetLogCode, sizeof(TargetLogCode))){
			TargetLogStart = start;
			TargetLogReturn = TargetLogStart+5;
		}
		if(!memcmp(start, SetAttrisCode, sizeof(SetAttrisCode))){
			SetAttrisFunc = start + 0xE + *(dword*)(start+0xE) + 4;
		}
		if(!memcmp(start, LoginCode, sizeof(LoginCode))){
			LoginFunc = start - 0x3;
		}
		if(!memcmp(start, RegionLocationCode, sizeof(RegionLocationCode))){
			RegionLocation = (byte*)(*(dword*)(start+7));
		}
		if(!memcmp(start, LanguageLocationCode, sizeof(LanguageLocationCode))){
			LanguageLocation = (byte*)(*(dword*)(start+7));
		}
		if(!memcmp(start, RequestQuoteCode, sizeof(RequestQuoteCode))){
			RequestQuoteFunction = start - 3;
		}
		if(!memcmp(start, TraderCostCode, sizeof(TraderCostCode))){
			TraderCostStart = start - 8;
			TraderCostReturn = TraderCostStart + 5;
		}
		if(!memcmp(start, TraderFunctionCode, sizeof(TraderFunctionCode))){
			TraderFunction = start - 0x48;
		}
		if(	CurrentTarget &&
			BaseOffset &&
			PacketSendFunction &&
			MessageHandlerStart &&
			SkillLogStart &&
			MapIdLocation &&
			WriteWhisperStart &&
			TargetFunctions &&
			HeroSkillFunction &&
			ClickToMoveFix &&
			BuildNumber &&
			ChangeTargetFunction &&
			MaxZoomStill &&
			MaxZoomMobile &&
			SkillCancelStart &&
			SellSessionStart &&
			SellItemFunction &&
			BuyItemFunction &&
			PingLocation &&
			LoggedInLocation &&
			NameLocation &&
			DeadLocation &&
			BasePointerLocation &&
			DialogStart &&
			EngineStart &&
			SkillTypeBase &&
			WinHandle &&
			LoadFinished &&
			TargetLogStart &&
			SetAttrisFunc &&
			LoginFunc &&
			RegionLocation &&
			LanguageLocation &&
			RequestQuoteFunction &&
			TraderCostStart &&
			TraderFunction){
			return;
		}
		start++;
	}
}

void WriteJMP(byte* location, byte* newFunction){
	DWORD dwOldProtection;
	VirtualProtect(location, 5, PAGE_EXECUTE_READWRITE, &dwOldProtection);
		location[0] = 0xE9;
		*((dword*)(location + 1)) = (dword)(newFunction - location) - 5;
	VirtualProtect(location, 5, dwOldProtection, &dwOldProtection);
}

void InjectErr(const char* lpzText){
	char* buf = new char[100];
	sprintf_s(buf, 100, "The %s could not be found!\nPlease contact SOMEONE about this issue.", lpzText);
	MessageBox(NULL, buf, "Hooking error!", MB_OK);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch(dwReason){
		case DLL_PROCESS_ATTACH:
			FindOffsets();
			if(!BaseOffset){
				InjectErr("BaseOffset");
				return false;
			}
			if(!PacketSendFunction){
				InjectErr("PacketSendFunction");
				return false;
			}else{
				PacketLocation = *(reinterpret_cast<dword*>(BaseOffset - 4));
				FlagLocation = PacketLocation - 0x130;
				PacketMutex = CreateMutex(NULL, false, NULL);
				CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&SendPacketQueueThread, 0, 0, 0);
			}
			if(!AgentArrayPtr){
				InjectErr("AgentArrayPtr");
				return false;
			}
			if(!MessageHandlerStart){
				InjectErr("MessageHandler");
				return false;
			}else{
				//DWORD dwOldProtection;
				//VirtualProtect(MessageHandlerStart, 9, PAGE_EXECUTE_READWRITE, &dwOldProtection);
				//memset(MessageHandlerStart, 0x90, 9);
				//VirtualProtect(MessageHandlerStart, 9, dwOldProtection, NULL);
				//WriteJMP(MessageHandlerStart, (byte*)CustomMsgHandler);
			}
			if(!SkillLogStart){
				InjectErr("SkillLog");
				return false;
			}else{
				DWORD dwOldProtection;
				VirtualProtect(SkillLogStart, 8, PAGE_EXECUTE_READWRITE, &dwOldProtection);
				memset(SkillLogStart, 0x90, 8);
				VirtualProtect(SkillLogStart, 8, dwOldProtection, NULL);
				WriteJMP(SkillLogStart, (byte*)SkillLogHook);
				PartyMutex = CreateMutex(NULL, false, NULL);
				CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&SkillLogQueueThread, 0, 0, 0);
			}
			if(!MapIdLocation){
				InjectErr("MapIdLocation");
				return false;
			}
			if(!WriteWhisperStart){
				InjectErr("WriteWhisperStart");
				return false;
			}
			if(!TargetFunctions){
				InjectErr("TargetFunctions");
				return false;
			}
			if(!HeroSkillFunction){
				InjectErr("HeroSkillFunction");
				return false;
			}
			if(!ClickToMoveFix){
				InjectErr("ClickToMoveFix");
				return false;
			}else{
				DWORD dwOldProtection;
				VirtualProtect(ClickToMoveFix, 5, PAGE_EXECUTE_READWRITE, &dwOldProtection);
				byte ClickToMoveFixCode[] = { 0x83, 0xF8, 0x00, 0x90, 0x90 };
				memcpy(ClickToMoveFix, ClickToMoveFixCode, 5);
				VirtualProtect(ClickToMoveFix, 5, dwOldProtection, NULL);
			}
			if(!BuildNumber){
				InjectErr("BuildNumber");
				return false;
			}
			if(!ChangeTargetFunction){
				InjectErr("ChangeTargetFunction");
				return false;
			}
			if(!MaxZoomStill){
				InjectErr("MaxZoomStill");
				return false;
			}
			if(!MaxZoomMobile){
				InjectErr("MaxZoomMobile");
				return false;
			}
			if(!SkillCancelStart){
				InjectErr("SkillCancelStart");
				return false;
			}else{
				DWORD dwOldProtection;
				VirtualProtect(SkillCancelStart, 7, PAGE_EXECUTE_READWRITE, &dwOldProtection);
				memset(SkillCancelStart, 0x90, 7);
				VirtualProtect(SkillCancelStart, 7, dwOldProtection, NULL);
				WriteJMP(SkillCancelStart, (byte*)SkillCancelHook);
			}
			if(!SellSessionStart){
				InjectErr("SellSessionStart");
				return false;
			}else{
				DWORD dwOldProtection;
				VirtualProtect(SellSessionStart, 9, PAGE_EXECUTE_READWRITE, &dwOldProtection);
				memset(SellSessionStart, 0x90, 9);
				VirtualProtect(SellSessionStart, 9, dwOldProtection, NULL);
				WriteJMP(SellSessionStart, (byte*)SellSessionHook);
			}
			if(!SellItemFunction){
				InjectErr("SellItemFunction");
				return false;
			}
			if(!BuyItemFunction){
				InjectErr("BuyItemFunction");
				return false;
			}
			if(!PingLocation){
				InjectErr("PingLocation");
				return false;
			}
			if(!LoggedInLocation){
				InjectErr("LoggedInLocation");
				return false;
			}
			if(!NameLocation){
				InjectErr("NameLocation");
				return false;
			}
			if(!DeadLocation){
				InjectErr("DeadLocation");
				return false;
			}
			if(!BasePointerLocation){
				InjectErr("BasePointerLocation");
				return false;
			}
			if(!DialogStart){
				InjectErr("DialogStart");
				return false;
			}else{
				DWORD dwOldProtection;
				VirtualProtect(DialogStart, 8, PAGE_EXECUTE_READWRITE, &dwOldProtection);
				memset(DialogStart, 0x90, 8);
				VirtualProtect(DialogStart, 8, dwOldProtection, NULL);
				WriteJMP(DialogStart, (byte*)DialogHook);
			}
			if(!EngineStart){
				InjectErr("EngineStart");
				return false;
			}else{
				DWORD dwOldProtection;
				VirtualProtect(EngineExStart, 5, PAGE_EXECUTE_READWRITE, &dwOldProtection);
				memset(EngineExStart, 0x90, 5);
				VirtualProtect(EngineExStart, 5, dwOldProtection, NULL);
				WriteJMP(EngineExStart, (byte*)EngineHookEx);
			}
			if(!SkillTypeBase){
				InjectErr("SkillTypeBase");
				return false;
			}
			if(!WinHandle){
				InjectErr("WinHandle");
				return false;
			}
			if(!LoadFinished){
				InjectErr("LoadFinished");
				return false;
			}else{
				DWORD dwOldProtection;
				VirtualProtect(LoadFinished, 6, PAGE_EXECUTE_READWRITE, &dwOldProtection);
				memset(LoadFinished, 0x90, 6);
				VirtualProtect(LoadFinished, 6, dwOldProtection, NULL);
				WriteJMP(LoadFinished, (byte*)LoadHook);
			}
			if(!TargetLogStart){
				InjectErr("TargetLogStart");
				return false;
			}else{
				for(int i = 1;i < 2560;i++){ AgentTargets[i] = 0; }
				WriteJMP(TargetLogStart, (byte*)TargetLogHook);
			}
			if(!SetAttrisFunc){
				InjectErr("SetAttrisFunc");
				return false;
			}
			if(!LoginFunc){
				InjectErr("LoginFunc");
				return false;
			}
			if(!RegionLocation){
				InjectErr("RegionLocation");
				return false;
			}
			if(!LanguageLocation){
				InjectErr("LanguageLocation");
				return false;
			}
			if(!RequestQuoteFunction){
				InjectErr("RequestQuoteFunction");
				return false;
			}
			if(!TraderCostStart){
				InjectErr("TraderCostStart");
				return false;
			}else{
				DWORD dwOldProtection;
				VirtualProtect(TraderCostStart, 5, PAGE_EXECUTE_READWRITE, &dwOldProtection);
				memset(TraderCostStart, 0x90, 5);
				VirtualProtect(TraderCostStart, 5, dwOldProtection, NULL);
				WriteJMP(TraderCostStart, (byte*)TraderCostHook);
			}
			if(!TraderFunction){
				InjectErr("TraderFunction");
				return false;
			}

			myGWCAServer->SetRequestFunction(HandleMessages);

			/*
			AllocConsole();
			SetConsoleTitleA("GWCA Console");
			FILE *fh;
			freopen_s(&fh, "CONOUT$", "wb", stdout);
			printf("BaseOffset=0x%06X\n", BaseOffset);
			printf("PacketSendFunction=0x%06X\n", PacketSendFunction);
			printf("CurrentTarget=0x%06X\n", CurrentTarget);
			printf("AgentArrayPtr=0x%06X\n", AgentArrayPtr);
			printf("AgentArrayMaxPtr=0x%06X\n", AgentArrayMaxPtr);
			printf("MessageHandlerStart=0x%06X\n", MessageHandlerStart);
			printf("MessageHandlerReturn=0x%06X\n", MessageHandlerReturn);
			printf("SkillLogStart=0x%06X\n", SkillLogStart);
			printf("SkillLogReturn=0x%06X\n", SkillLogReturn);
			printf("WriteWhisperStart=0x%06X\n", WriteWhisperStart);
			printf("TargetFunctions=0x%06X\n", TargetFunctions);
			printf("HeroSkillFunction=0x%06X\n", HeroSkillFunction);
			printf("ClickToMoveFix=0x%06X\n", ClickToMoveFix);
			printf("BuildNumber=0x%06X\n", BuildNumber);
			printf("ChangeTargetFunction=0x%06X\n", ChangeTargetFunction);
			printf("MaxZoomStill=0x%06X\n", MaxZoomStill);
			printf("MaxZoomMobile=0x%06X\n", MaxZoomMobile);
			printf("SkillCancelStart=0x%06X\n", SkillCancelStart);
			printf("SkillCancelReturn=0x%06X\n", SkillCancelReturn);
			printf("SellSessionStart=0x%06X\n", SellSessionStart);
			printf("SellItemFunction=0x%06X\n", SellItemFunction);
			printf("BuyItemFunction=0x%06X\n", BuyItemFunction);
			printf("PingLocation=0x%06X\n", PingLocation);
			printf("LoggedInLocation=0x%06X\n", LoggedInLocation);
			printf("NameLocation=0x%06X\n", NameLocation);
			printf("DeadLocation=0x%06X\n", DeadLocation);
			printf("BasePointerLocation=0x%06X\n", BasePointerLocation);
			printf("DialogStart=0x%06X\n", DialogStart);
			printf("DialogReturn=0x%06X\n", DialogReturn);
			printf("EngineStart=0x%06X\n", EngineStart);
			printf("SkillTypeBase=0x%06X\n", SkillTypeBase);
			printf("WinHandle=0x%06X\n", WinHandle);
			printf("LoadFinished=0x%06X\n", LoadFinished);
			printf("TargetLogStart=0x%06X\n", TargetLogStart);
			printf("TargetLogReturn=0x%06X\n", TargetLogReturn);
			printf("LoginFunc=0x%06X\n", LoginFunc);
			printf("RegionLocation=0x%06X\n", RegionLocation);
			printf("LanguageLocation=0x%06X\n", LanguageLocation);
			printf("RequestQuoteFunction=0x%06X\n", RequestQuoteFunction);
			printf("TraderCostStart=0x%06X\n", TraderCostStart);
			printf("TraderCostReturn=0x%06X\n", TraderCostReturn);
			printf("TraderFunction=0x%06X\n", TraderFunction);
			*/
			break;

		case DLL_PROCESS_DETACH:
			break;
	}
	return true;
}