// HelloBolt.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#include <windows.h>
#include <shellapi.h> 
#include <XLUE.h>
#include <XLGraphic.h>
#include <XLLuaRuntime.h>
#include <string>
#include <Shlwapi.h>
#include <shlobj.h>
#include <process.h>
#include <atlbase.h>


using namespace std;
#define API_UTIL_CLASS	"API.Util.Class"
#define API_UTIL_OBJ		"API.Util"

struct LuaCallInfo{
	lua_State* lua_state;
	long fnRef;
};
LuaCallInfo g_LuaCall;

class LuaAPIUtil
{
public:
	LuaAPIUtil(void){};
	~LuaAPIUtil(void){};
	static LuaAPIUtil * __stdcall Instance(void *){
		static LuaAPIUtil s_instance;
		return &s_instance;
	};
	static void RegisterObj(XL_LRT_ENV_HANDLE hEnv){
		if (hEnv == NULL){
			return;
		}

		XLLRTObject object;
		object.ClassName = API_UTIL_CLASS;
		object.ObjName = API_UTIL_OBJ;
		object.MemberFunctions = sm_LuaMemberFunctions;
		object.userData = NULL;
		object.pfnGetObject = (fnGetObject)LuaAPIUtil::Instance;

		XLLRT_RegisterGlobalObj(hEnv, object);
	};
	static int AttachListener(lua_State* pLuaState){
		LuaAPIUtil** ppMyClass= reinterpret_cast<LuaAPIUtil**>(luaL_checkudata(pLuaState,1,API_UTIL_CLASS));   
		if(ppMyClass && (*ppMyClass)){
			if(!lua_isfunction(pLuaState,2)){
				return 0;
			}
			g_LuaCall.fnRef = luaL_ref(pLuaState,LUA_REGISTRYINDEX);
			g_LuaCall.lua_state = pLuaState;
			//注册完回调之后，拉起cefclient进程
			CHAR szExePath[MAX_PATH + 1] = {0};
			GetModuleFileNameA(NULL, szExePath, MAX_PATH);
			PathRemoveFileSpecA(szExePath);
			::PathCombineA(szExePath, szExePath, "cefclient.exe");
			ShellExecuteA(NULL, "open", szExePath, "", "", SW_SHOWNORMAL);
		}
		return 0;
	};
	static int FindWindow(lua_State* pLuaState){
		LuaAPIUtil** ppUtil = (LuaAPIUtil **)luaL_checkudata(pLuaState, 1, API_UTIL_CLASS);
		if (ppUtil && *ppUtil)
		{
			LPCSTR lpszClassName = lua_tostring(pLuaState, 2); // utf8 string
			LPCSTR lpszWindowName = lua_tostring(pLuaState, 3); // utf8 string

			CComBSTR bstrClassName;
			if (lpszClassName && *lpszClassName)
			{
				LuaStringToCComBSTR(lpszClassName,bstrClassName);
			}

			CComBSTR bstrWindowName;
			if (lpszWindowName && *lpszWindowName)
			{
				LuaStringToCComBSTR(lpszWindowName,bstrWindowName);
			}

			LPCWSTR lpwszClassName = bstrClassName.m_str;
			LPCWSTR lpwszWindowName = bstrWindowName.m_str;
			HWND hWnd = ::FindWindowW(lpwszClassName, lpwszWindowName);
			if (hWnd)
			{
				lua_pushlightuserdata(pLuaState, hWnd);
				return 1;
			}
		}

		lua_pushnil(pLuaState);
		return 1;
	};
	static int SendMessage( lua_State *pLuaState ){

		LuaAPIUtil** ppUtil = (LuaAPIUtil **)luaL_checkudata(pLuaState, 1, API_UTIL_CLASS);
		if (ppUtil && *ppUtil){
			HWND hWnd = (HWND)lua_touserdata(pLuaState, 2);
			DWORD dwMsg = (DWORD)lua_tointeger(pLuaState, 3);
			DWORD dwWParam = (DWORD)lua_tointeger(pLuaState, 4);
			DWORD dwLParam = (DWORD)lua_tointeger(pLuaState, 5);
			if (NULL != hWnd){
				LRESULT iRet = ::SendMessage(hWnd, dwMsg, (WPARAM)dwWParam, (LPARAM)dwLParam);
				lua_pushnumber(pLuaState, (lua_Number)iRet);
				DWORD dwError = ::GetLastError();
				lua_pushinteger(pLuaState,dwError);
				return 2;
			}
		}

		lua_pushnil(pLuaState);
		return 1;
	};

private:
	static bool LuaStringToCComBSTR(const char* src, CComBSTR& bstr){
		bstr = L"";
		if(!src)
			return false;
		int iLen = (int)strlen(src);
		if(iLen > 0){
			wchar_t* szm = new wchar_t[iLen * 4];
			ZeroMemory(szm, iLen * 4);
			int nLen = MultiByteToWideChar(CP_UTF8, 0, src,iLen, szm, iLen*4); 
			szm [nLen] = '\0';
			bstr = szm;
			delete [] szm;
			return true;
		}
		return false;
	};
	static XLLRTGlobalAPI sm_LuaMemberFunctions[];
};
#define DEF_LUAAPI(x) {#x, LuaAPIUtil::x}

XLLRTGlobalAPI LuaAPIUtil::sm_LuaMemberFunctions[] = {
	DEF_LUAAPI(AttachListener),
	DEF_LUAAPI(FindWindow),
	DEF_LUAAPI(SendMessage),
	{NULL,NULL}
};

void LuaCall_SetBrowserHwnd(HWND hwnd){
	lua_rawgeti(g_LuaCall.lua_state, LUA_REGISTRYINDEX, g_LuaCall.fnRef);

	int iRetCount = 0;
	lua_pushlightuserdata(g_LuaCall.lua_state, (HWND)hwnd);
	++iRetCount;

	XLLRT_LuaCall(g_LuaCall.lua_state, iRetCount, 0, L"LuaCall_SetBrowserHwnd");
}

HINSTANCE g_hInstance;

//开始
#define WM_USER_SETHWND				WM_USER+100

class MsgWindow{
public:
	static BOOL AllowMessageToRecv(UINT uiMsg, BOOL bAllowed)
	{
		BOOL bSuc = FALSE;
		CHAR szUser32DllFilePath[MAX_PATH] = { 0 };
		if (SUCCEEDED(::SHGetFolderPathA(NULL, CSIDL_SYSTEM, NULL, 0, szUser32DllFilePath))
			&& ::PathAppendA(szUser32DllFilePath, "user32.dll")){
				if (::PathFileExistsA(szUser32DllFilePath) && !::PathIsDirectoryA(szUser32DllFilePath)){
					HMODULE hUser32Dll = ::LoadLibraryA(szUser32DllFilePath);
					if (hUser32Dll){
						typedef BOOL(__stdcall *PFN_ChangeWindowMessageFilter)(UINT, DWORD);
						PFN_ChangeWindowMessageFilter pfnChangeWindowMessageFilter = (PFN_ChangeWindowMessageFilter)::GetProcAddress(hUser32Dll, "ChangeWindowMessageFilter");
						if (pfnChangeWindowMessageFilter){
							bSuc = (*pfnChangeWindowMessageFilter)(uiMsg, (bAllowed ? 1 : 2)); // 1 - MSGFLT_ADD; 2 - MSGFLT_REMOVE
						}
						::FreeLibrary(hUser32Dll);
					}
				}
		}
		return bSuc;
	};
	static LRESULT CALLBACK MyWinProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		switch(msg)
		{
		case WM_CREATE:
			AllowMessageToRecv(WM_USER_SETHWND, TRUE);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_USER_SETHWND:
			LuaCall_SetBrowserHwnd((HWND)wparam);
			break;
		default:
			break;
		}
		return ::DefWindowProc(hwnd,msg,wparam,lparam);

	}

	static int WINAPI Create(HINSTANCE hInstance = g_hInstance)
	{
		WNDCLASSEX wcex=
		{
			sizeof(WNDCLASSEX),CS_HREDRAW|CS_VREDRAW,
			MyWinProc,
			0,
			0,
			hInstance,
			LoadIcon(NULL,IDI_APPLICATION),
			LoadCursor(NULL,IDC_ARROW),
			(HBRUSH)GetStockObject(GRAY_BRUSH),
			NULL,
			L"xlue-cef-{37408185-9D8A-46be-968A-144FEBDB64D6}",
			LoadIcon(NULL,IDI_APPLICATION)

		};

		RegisterClassEx(&wcex);

		HWND hwnd;
		hwnd=CreateWindowEx
			(
			NULL,
			L"xlue-cef-{37408185-9D8A-46be-968A-144FEBDB64D6}",
			L"",
			WS_EX_PALETTEWINDOW,
			0,
			0,
			0,
			0,
			NULL,
			NULL,
			hInstance,
			NULL
			);
		if(!hwnd)
			return 0;
		ShowWindow(hwnd,SW_HIDE);
		UpdateWindow(hwnd);
		/*MSG msg;
		while(GetMessage(&msg,NULL,0,0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}*/
		return 0;
	}
};

unsigned int __stdcall ThreadCreateMsgWindow(void* arg){
	MsgWindow::Create();
	return 0;
}

void CreateMsgWindow(){
	unsigned nThreadID;
	_beginthreadex(NULL, 0, &ThreadCreateMsgWindow, NULL, 0, &nThreadID);
}


const WCHAR* GetResDir()
{
	static WCHAR wszModulePath[MAX_PATH];
	GetModuleFileNameW(NULL,wszModulePath,MAX_PATH);
	PathAppend(wszModulePath, L"..\\XAR");
	return wszModulePath;
}

int __stdcall LuaErrorHandle(lua_State* luaState,const wchar_t* pExtInfo,const wchar_t* wcszLuaErrorString,PXL_LRT_ERROR_STACK pStackInfo)
{
    static bool s_bEnter = false;
    if (!s_bEnter)
    {
        s_bEnter = true;
        if(pExtInfo != NULL)
        {
			wstring str = wcszLuaErrorString ? wcszLuaErrorString : L"";
            luaState;
            pExtInfo;
            wcszLuaErrorString;
            str += L" @ ";
            str += pExtInfo;

            MessageBoxW(0,str.c_str(),L"为了帮助我们改进质量,请反馈此脚本错误",MB_ICONERROR | MB_OK);

        }
        else
        {
			MessageBoxW(0,wcszLuaErrorString ? wcszLuaErrorString : L"" ,L"为了帮助我们改进质量,请反馈此脚本错误",MB_ICONERROR | MB_OK);
        }
        s_bEnter = false;
    }
    return 0;
}


bool InitXLUE()
{
    //初始化图形库
    XLGraphicParam param;
    XL_PrepareGraphicParam(&param);
	param.textType = XLTEXT_TYPE_FREETYPE;
    long result = XL_InitGraphicLib(&param);
    result = XL_SetFreeTypeEnabled(TRUE);
    //初始化XLUE,这函数是一个复合初始化函数
    //完成了初始化Lua环境,标准对象,XLUELoader的工作
    result = XLUE_InitLoader(NULL);

	 //设置一个简单的脚本出错提示
    XLLRT_ErrorHandle(LuaErrorHandle);
	//注册对象
	XL_LRT_ENV_HANDLE hEnv = XLLRT_GetEnv(NULL);
	if (NULL == hEnv){
		return FALSE;
	}
	LuaAPIUtil::RegisterObj(hEnv);
	XLLRT_ReleaseEnv(hEnv);
    return true; 
}

void UninitXLUE()
{
    //退出流程
    XLUE_Uninit(NULL);
    XLUE_UninitLuaHost(NULL);
    XL_UnInitGraphicLib();
    XLUE_UninitHandleMap(NULL);
}

bool LoadMainXAR()
{
    long result = 0;
    //设置XAR的搜索路径
    result = XLUE_AddXARSearchPath(GetResDir());
    //加载主XAR,此时会执行该XAR的启动脚本onload.lua
    result = XLUE_LoadXAR("HelloBolt6");
    if(result != 0)
    {
        return false;
    }
    return true;
}



int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	g_hInstance = hInstance;
 	// TODO: 在此放置代码。
	if(!InitXLUE())
    {
        MessageBoxW(NULL,L"初始化XLUE 失败!",L"错误",MB_OK);
        return 1;
    }

    if(!LoadMainXAR())
    {
        MessageBoxW(NULL,L"Load XAR失败!",L"错误",MB_OK);
        return 1;
    }
	MsgWindow::Create(hInstance);
	MSG msg;
	
	// 主消息循环:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UninitXLUE();

	return 0;
}
