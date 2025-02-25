#include <utils.h>
#include "Globals.h"
#include "PluginImpl.h"
#include "BackgroundTasks.h"
#include "UI/Activities/BackgroundTasksUI.h"
#include "Protocol/Protocol.h"
#include <fcntl.h>
#include <set>
#ifndef __APPLE__
# include <elf.h>
#endif
#include <sudo.h>

//LPCSTR      DiskMenuStrings[ 1+FTP_MAXBACKUPS ];

SHAREDSYMBOL void WINPORT_DllStartup(const char *path)
{
	MB2Wide(path, G.plugin_path);
	//G.plugin_path = path;
//	fprintf(stderr, "NetRocks::WINPORT_DllStartup\n");
}

SHAREDSYMBOL int WINAPI _export GetMinFarVersionW(void)
{
	#define MAKEFARVERSION(major,minor) ( ((major)<<16) | (minor))
	return MAKEFARVERSION(2, 2);
}


SHAREDSYMBOL void WINAPI _export SetStartupInfoW(const struct PluginStartupInfo *Info)
{
	G.Startup(Info);
//	fprintf(stderr, "FSF=%p ExecuteLibrary=%p\n", G.info.FSF, G.info.FSF ? G.info.FSF->ExecuteLibrary : nullptr);

}
/*
SHAREDSYMBOL HANDLE WINAPI _export OpenFilePluginw(const wchar_t *Name, const unsigned wchar_t *Data, int DataSize, int OpMode)
{
	if (!G.IsStarted())
		return INVALID_HANDLE_VALUE;


	PluginImpl *out = nullptr;
#ifndef __APPLE__
	if (elf) {
		out = new PluginImplELF(Name, Data[4], Data[5]);

	} else 
#endif
	if (plain) {
		out = new PluginImplPlain(Name, plain);

	} else
		abort();

	return out ? (HANDLE)out : INVALID_HANDLE_VALUE;
}
*/

SHAREDSYMBOL HANDLE WINAPI _export OpenPluginW(int OpenFrom, INT_PTR Item)
{
	fprintf(stderr, "NetRocks: OpenPlugin(%d, '0x%lx')\n", OpenFrom, (unsigned long)Item);

	if (!G.IsStarted())// || OpenFrom != OPEN_COMMANDLINE)
		return INVALID_HANDLE_VALUE;

	if (OpenFrom == OPEN_PLUGINSMENU) {
		if (Item == 1) {
			BackgroundTasksList();
			return INVALID_HANDLE_VALUE;
		}

		Item = 0;
//		static const wchar_t s_curdir_cmd[] = L"file:.";
//		Item = (INT_PTR)&s_curdir_cmd[0];
	}


	const wchar_t *path = (Item > 0xfff) ? (const wchar_t *)Item : nullptr;

	try {
		return new PluginImpl(path);

	} catch (std::exception &ex) {
		const std::wstring &tmp_what = MB2Wide(ex.what());
		const wchar_t *msg[] = { G.GetMsgWide(MError), tmp_what.c_str(), G.GetMsgWide(MOK)};
		G.info.Message(G.info.ModuleNumber, FMSG_WARNING, nullptr, msg, ARRAYSIZE(msg), 1);
		return INVALID_HANDLE_VALUE;
	}
}

SHAREDSYMBOL void WINAPI _export ClosePluginW(HANDLE hPlugin)
{
	delete (PluginImpl *)hPlugin;
}


SHAREDSYMBOL int WINAPI _export GetFindDataW(HANDLE hPlugin,struct PluginPanelItem **pPanelItem,int *pItemsNumber,int OpMode)
{
	return ((PluginImpl *)hPlugin)->GetFindData(pPanelItem, pItemsNumber, OpMode);
}


SHAREDSYMBOL void WINAPI _export FreeFindDataW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber)
{
	((PluginImpl *)hPlugin)->FreeFindData(PanelItem, ItemsNumber);
}


SHAREDSYMBOL int WINAPI _export SetDirectoryW(HANDLE hPlugin,const wchar_t *Dir,int OpMode)
{
	return ((PluginImpl *)hPlugin)->SetDirectory(Dir, OpMode);
}


SHAREDSYMBOL int WINAPI _export DeleteFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int OpMode)
{
	return ((PluginImpl *)hPlugin)->DeleteFiles(PanelItem, ItemsNumber, OpMode);
}


SHAREDSYMBOL int WINAPI _export GetFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,
                   int ItemsNumber,int Move, const wchar_t **DestPath,int OpMode)
{
	return ((PluginImpl *)hPlugin)->GetFiles(PanelItem, ItemsNumber, Move, DestPath ? *DestPath : nullptr, OpMode);
}


SHAREDSYMBOL int WINAPI _export PutFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,
                   int ItemsNumber,int Move,const wchar_t *SrcPath, int OpMode)
{
	return ((PluginImpl *)hPlugin)->PutFiles(PanelItem, ItemsNumber, Move, SrcPath, OpMode);
}

SHAREDSYMBOL int WINAPI _export MakeDirectoryW(HANDLE hPlugin, const wchar_t **Name, int OpMode)
{
	return ((PluginImpl *)hPlugin)->MakeDirectory(Name, OpMode);
}

SHAREDSYMBOL int WINAPI ProcessEventW(HANDLE hPlugin, int Event, void * Param)
{
	switch (Event) {
		case FE_COMMAND:
			if (Param)
				return ((PluginImpl *)hPlugin)->ProcessEventCommand((const wchar_t *)Param);
		break;

		default:
			;
	}

	return 0;
}

SHAREDSYMBOL void WINAPI _export ExitFARW()
{
	BackgroundTasksInfo info;
	GetBackgroundTasksInfo(info);
	for (const auto &task_info : info) {
		if (task_info.status == BTS_ACTIVE || task_info.status == BTS_PAUSED) {
			fprintf(stderr, "NetRocks::ExitFARW: aborting task id=%lu info='%s'\n", task_info.id, task_info.information.c_str());
			AbortBackgroundTask(task_info.id);
		}
	}

	while (CountOfPendingBackgroundTasks() != 0) {
		if (G.info.FSF->DispatchInterThreadCalls() > 0) {
			usleep(1000);
		} else {
			usleep(100000);
		}
	}
}

SHAREDSYMBOL int WINAPI _export MayExitFARW()
{
	const size_t background_tasks_count = CountOfPendingBackgroundTasks();
	if ( background_tasks_count != 0 && !ConfirmExitFAR(background_tasks_count).Ask()) {
		return 0;
	}

	return 1;
}

static std::wstring CombineAllProtocolPrefixes()
{
	std::wstring out;
	for (auto pi = ProtocolInfoHead(); pi->name; ++pi) {
		out+= MB2Wide(pi->name);
		out+= L':';
	}
	return out;
}

SHAREDSYMBOL void WINAPI _export GetPluginInfoW(struct PluginInfo *Info)
{
//	fprintf(stderr, "NetRocks: GetPluginInfoW\n");
	static const wchar_t *s_cfg_strings[] = {G.GetMsgWide(MBackgroundTasksTitle)};
	static const wchar_t *s_menu_strings[] = {G.GetMsgWide(MTitle), G.GetMsgWide(MBackgroundTasksTitle)};
	static const wchar_t *s_disk_menu_strings[] = {L"NetRocks\0"};
	static std::wstring s_command_prefixes = CombineAllProtocolPrefixes();

	Info->StructSize = sizeof(*Info);
	Info->Flags = PF_FULLCMDLINE;
	Info->PluginConfigStrings = s_cfg_strings;
	Info->PluginConfigStringsNumber = ARRAYSIZE(s_cfg_strings);
	Info->PluginMenuStrings = s_menu_strings;
	Info->PluginMenuStringsNumber = (CountOfAllBackgroundTasks() != 0) ? ARRAYSIZE(s_menu_strings) : 1;
	Info->CommandPrefix = s_command_prefixes.c_str();
	Info->DiskMenuStrings           = s_disk_menu_strings;
	Info->DiskMenuStringsNumber     = ARRAYSIZE(s_disk_menu_strings);
}


SHAREDSYMBOL void WINAPI _export GetOpenPluginInfoW(HANDLE hPlugin,struct OpenPluginInfo *Info)
{
	((PluginImpl *)hPlugin)->GetOpenPluginInfo(Info);
}

SHAREDSYMBOL int WINAPI _export ProcessKeyW(HANDLE hPlugin,int Key,unsigned int ControlState)
{
	return ((PluginImpl *)hPlugin)->ProcessKey(Key, ControlState);
}

SHAREDSYMBOL int WINAPI _export ConfigureW(int ItemNumber)
{
	if (!G.IsStarted())
		return 0;

	BackgroundTasksList();
	return 1;
/*
	struct FarDialogItem fdi[] = {
            {DI_DOUBLEBOX,  3,  1,  70, 5,  0,{},0,0, {}},
            {DI_TEXT,      -1,  2,  0,  2,  0,{},0,0, {}},
            {DI_BUTTON,     34, 4,  0,  4,  0,{},0,0, {}}
	};

	wcsncpy(fdi[0].Data, G.GetMsgWide(MTitle), ARRAYSIZE(fdi[0].Data));
	wcsncpy(fdi[1].Data, G.GetMsgWide(MDescription), ARRAYSIZE(fdi[1].Data));
	wcsncpy(fdi[2].Data, G.GetMsgWide(MOK), ARRAYSIZE(fdi[2].Data));

	G.info.Dialog(G.info.ModuleNumber, -1, -1, 74, 7, NULL, fdi, ARRAYSIZE(fdi));
	return 1;
*/
}
