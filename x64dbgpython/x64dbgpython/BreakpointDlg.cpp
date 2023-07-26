#include <map>
#include <string>
#include "resource1.h"
#include "BreakpointDlg.h"
#include "plugin.h"
#include "pybind11\embed.h"

namespace py = pybind11;

std::map<duint, std::string> g_BreakpointList;

void RefreshBpListBox(HWND hwnd);
void CleanGarbageForBpMap();

//Plugin breakpoint callback handle
void PluginHandleBreakpoint(CBTYPE cbType, PLUG_CB_BREAKPOINT* info)
{
	duint breakAddr = info->breakpoint->addr;
	std::map<duint, std::string>::const_iterator iter = g_BreakpointList.find(breakAddr);
	if (iter != g_BreakpointList.end())
	{
		try
		{
			py::exec(iter->second.c_str());
		}
		catch (py::error_already_set& e)
		{
			_plugin_logprint(e.what());
		}
	}
}

//Plugin handle save data
void PluginHandleSaveDB(CBTYPE cbType, PLUG_CB_LOADSAVEDB* info)
{
	json_t* jsonBPList = nullptr;
	if (!g_BreakpointList.empty())
	{
		jsonBPList = json_array();
		for (auto it = g_BreakpointList.cbegin(); it != g_BreakpointList.end(); it++)
		{
			json_t* jsonBP = json_object();
			json_object_set_new(jsonBP, "addr", json_integer(it->first));
			json_object_set_new(jsonBP, "command", json_string(it->second.c_str()));
			json_array_append_new(jsonBPList, jsonBP);
		}
		json_object_set_new(info->root, "bpList", jsonBPList);
	}
}

void PluginHandleLoadDB(CBTYPE cbType, PLUG_CB_LOADSAVEDB* info)
{
	if (!(info->loadSaveType == PLUG_DB_LOADSAVE_DATA || info->loadSaveType == PLUG_DB_LOADSAVE_ALL))
		return;
	
	json_t* jsonBPList = json_object_get(info->root, "bpList");
	if (!json_is_array(jsonBPList))
		return;

	json_t* jsonBP;
	size_t i;

	json_array_foreach(jsonBPList, i, jsonBP)
	{
		if (json_is_object(jsonBP))
		{
			json_t* jsonAddr = json_object_get(jsonBP, "addr");
			json_t* jsonCommand = json_object_get(jsonBP, "command");
			if (json_is_integer(jsonAddr) && json_is_string(jsonCommand))
			{
				g_BreakpointList.insert({ json_integer_value(jsonAddr), std::string(json_string_value(jsonCommand)) });
			}
		}
	}
}

duint GetBpAtIndex(int index)
{
	BPMAP bpList;
	DbgGetBpList(bp_none, &bpList);
	return bpList.bp[index].addr;
}

INT_PTR CALLBACK BreakpointDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_CLOSE:
		{
			EndDialog(hwndDlg, 0);
			break;
		}
		case WM_INITDIALOG:
		{
			CleanGarbageForBpMap();
			RefreshBpListBox(hwndDlg);
			break;
		}
		case WM_COMMAND:
		{
			if (LOWORD(wParam) == IDC_BPLIST)
			{
				if (HIWORD(wParam) == LBN_DBLCLK)
				{
					HWND bpListHwnd = GetDlgItem(hwndDlg, LOWORD(wParam));
					int pos = (int)SendMessage(bpListHwnd, LB_GETCURSEL, 0, 0);
					if (pos != -1)
					{
						duint bpAddr = GetBpAtIndex(pos);
						char dCommand[20];
						sprintf_s(dCommand, "d %p", (void*)bpAddr);
						DbgCmdExecDirect(dCommand);
					}
				}
				else if (HIWORD(wParam) == LBN_SELCHANGE)
				{
					HWND bpListHwnd = GetDlgItem(hwndDlg, LOWORD(wParam));
					int pos = (int)SendMessage(bpListHwnd, LB_GETCURSEL, 0, 0);
					if (pos != -1)
					{
						duint bpAddr = GetBpAtIndex(pos);
						std::map<duint, std::string>::const_iterator iter = g_BreakpointList.find(bpAddr);
						if (iter != g_BreakpointList.end())
						{
							SetDlgItemTextA(hwndDlg, IDC_SCRIPTCONTENT, iter->second.c_str());
						}
						else
						{
							SetDlgItemTextA(hwndDlg, IDC_SCRIPTCONTENT, "");
						}
					}
				}
			}
			else if (LOWORD(wParam) == IDC_EDITSCRIPT)
			{
				if (HIWORD(wParam) == BN_CLICKED)
				{
					HWND bpListHwnd = GetDlgItem(hwndDlg, IDC_BPLIST);
					int pos = (int)SendMessage(bpListHwnd, LB_GETCURSEL, 0, 0);
					if (pos != -1)
					{
						DialogBoxParam(g_dllInstance, MAKEINTRESOURCE(IDD_SCRIPTEDITOR), hwndDlg, ScriptEditorProc, (LPARAM)pos);
						RefreshBpListBox(hwndDlg);
						
						duint bpAddr = GetBpAtIndex(pos);
						std::map<duint, std::string>::const_iterator iter = g_BreakpointList.find(bpAddr);
						if (iter != g_BreakpointList.end())
						{
							SetDlgItemTextA(hwndDlg, IDC_SCRIPTCONTENT, iter->second.c_str());
						}
						else
						{
							SetDlgItemTextA(hwndDlg, IDC_SCRIPTCONTENT, "");
						}
					}
				}
			}
			break;
		}
		default:
			break;
	}
	return false;
}

int g_CurrentBpSelection;

INT_PTR CALLBACK ScriptEditorProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			g_CurrentBpSelection = (int)lParam;
			duint bpAddr = GetBpAtIndex(g_CurrentBpSelection);;
			std::map<duint, std::string>::const_iterator iter = g_BreakpointList.find(bpAddr);
			if (iter != g_BreakpointList.end())
			{
				SetDlgItemTextA(hwndDlg, IDC_SCRIPT, iter->second.c_str());
			}
			break;
		}
		case WM_CLOSE:
		{
			EndDialog(hwndDlg, 0);
			break;
		}
		case WM_COMMAND:
		{
			if (LOWORD(wParam) == IDC_SAVESCRIPT)
			{
				if (HIWORD(wParam) == BN_CLICKED)
				{
					duint bpAddr = GetBpAtIndex(g_CurrentBpSelection);

					HWND scriptTextHwnd = GetDlgItem(hwndDlg, IDC_SCRIPT);
					unsigned int scriptLength = GetWindowTextLengthA(scriptTextHwnd);
					char* scriptContent = new char[(unsigned long long)scriptLength + 2];
					GetDlgItemTextA(hwndDlg, IDC_SCRIPT, scriptContent, scriptLength + 1);

					std::map<duint, std::string>::const_iterator iter = g_BreakpointList.find(bpAddr);
					if (iter != g_BreakpointList.end())
					{
						g_BreakpointList.erase(iter);
					}

					if (scriptLength)
						g_BreakpointList.insert({ bpAddr, std::string(scriptContent) });

					delete[] scriptContent;

					EndDialog(hwndDlg, 0);
				}
			}
		}
		default:
			break;
	}

	return false;
}

void RefreshBpListBox(HWND hwnd)
{
	HWND bpListHwnd = GetDlgItem(hwnd, IDC_BPLIST);
	SendMessage(bpListHwnd, LB_RESETCONTENT, NULL, NULL);
	BPMAP bpList;
	DbgGetBpList(bp_none, &bpList);

	for (int i = 0; i < bpList.count; i++)
	{
		wchar_t bpAddrText[20];

		if (g_BreakpointList.find(bpList.bp[i].addr) != g_BreakpointList.end())
			swprintf(bpAddrText, sizeof(bpAddrText) / 2, L"%p*", (void*)bpList.bp[i].addr);
		else
			swprintf(bpAddrText, sizeof(bpAddrText) / 2, L"%p", (void*)bpList.bp[i].addr);
		SendMessage(bpListHwnd, LB_ADDSTRING, NULL, (LPARAM)bpAddrText);
	}
}

void CleanGarbageForBpMap()
{
	BPMAP bpList;
	DbgGetBpList(bp_none, &bpList);

	for (auto it = g_BreakpointList.cbegin(); it != g_BreakpointList.end();)
	{
		bool isHaveInBpList = false;
		for (int i = 0; i < bpList.count; i++)
		{
			if (bpList.bp[i].addr == it->first)
			{
				isHaveInBpList = true;
				break;
			}
		}
		if (!isHaveInBpList)
			it = g_BreakpointList.erase(it);
		else
			it++;
	}
}