#include <iostream>
#include "pch.h"
#include <cwchar>
#include <ShlObj.h>

using namespace std;

CLogUtil* g_dllLog = nullptr;

void RunAs(const wstring& cmdLine)
{
    int pos = cmdLine.find(L".exe");
    wstring exePath = cmdLine.substr(0, pos + wcslen(L".exe"));
    wstring param = cmdLine.substr(pos + wcslen(L".exe"));
    HINSTANCE hInstance = ShellExecuteW(NULL, L"runas", exePath.c_str(), param.c_str(), NULL, SW_HIDE);
    if (reinterpret_cast<int>(hInstance) > 32)
    {
        LOG_INFO(L"program started with administrator privileges.");
    }
    else
    {
        LOG_ERROR(L"failed to start the program with administrator privileges.");
    }
}

// 修改快捷方式，如果返回true表示需要用管理员权限修改
bool ModifyShortcutLink(const wstring& pluginPath)
{
    bool needRunAs = false;

    // Get the desktop folder
    PIDLIST_ABSOLUTE pidlDesktop;
    HRESULT hr = SHGetKnownFolderIDList(FOLDERID_Desktop, 0, NULL, &pidlDesktop);
    if (hr != S_OK)
    {
        LOG_ERROR(L"failed to get the desktop folder, error is 0x%x", hr);
        return needRunAs;
    }

    // Create an IShellItem for the desktop folder
    IShellItem* desktopFolder = nullptr;
    hr = SHCreateItemFromIDList(pidlDesktop, IID_IShellItem, (void**)(&desktopFolder));
    if (hr != S_OK)
    {
        LOG_ERROR(L"failed to create an IShellItem for the desktop folder, error is 0x%x", hr);
        CoTaskMemFree(pidlDesktop);
        return needRunAs;
    }

    // Enumerate items in the desktop folder
    IEnumShellItems* enumItems;
    hr = desktopFolder->BindToHandler(NULL, BHID_EnumItems, IID_IEnumShellItems, (void**)(&enumItems));
    if (hr != S_OK)
    {
        LOG_ERROR(L"failed to enumerate items in the desktop folder, error is 0x%x", hr);
        desktopFolder->Release();
        CoTaskMemFree(pidlDesktop);
        return needRunAs;
    }

    wstring chromeExe = L"chrome.exe";
    IShellItem* item;
    while (enumItems->Next(1, &item, NULL) == S_OK)
    {
        // Get the target path of the shortcut
        IShellLink* shellLink;
        if (item->BindToHandler(NULL, BHID_SFUIObject, IID_IShellLink, (void**)(&shellLink)) == S_OK)
        {
            wchar_t targetPath[MAX_PATH] = {0};
            wchar_t targetArg[MAX_PATH] = {0};
            if (shellLink->GetPath(targetPath, MAX_PATH-1, NULL, 0) == S_OK &&
                shellLink->GetArguments(targetArg, MAX_PATH - 1) == S_OK)
            {
                LOG_INFO(L"path is %s, argument is %s", targetPath, targetArg);
                std::wstring path(targetPath);
                if (path.find(chromeExe) == -1)
                {
                    continue;
                }

                wstring argument = targetArg;
                if (argument.find(pluginPath) != -1)
                {
                    LOG_INFO(L"the plugin path has been added ");
                }
                else
                {
                    wstring newArg = L" --load-extension=" + pluginPath;                    
                    hr = shellLink->SetArguments(newArg.c_str());
                    if (hr != S_OK)
                    {
                        LOG_ERROR(L"failed to set the argument of the Chrome shortcut link, error is 0x%x", hr);                        
                        break;
                    }

                    IPersistFile* persistFile = nullptr;
                    HRESULT hr = shellLink->QueryInterface(IID_IPersistFile, (void**)(&persistFile));
                    if (FAILED(hr))
                    {
                        LOG_ERROR(L"failed to get the persistfile interface, error is 0x%x", hr);
                        break;
                    }

                    hr = persistFile->Save(nullptr, TRUE);
                    persistFile->Release();
                    if (FAILED(hr))
                    {
                        LOG_ERROR(L"failed to save the shortcut link, error is 0x%x", hr);
                        needRunAs = true;
                        break;
                    }
                }
            }

            shellLink->Release();
        }
        
        item->Release();
    }
    
    enumItems->Release();
    desktopFolder->Release();
    CoTaskMemFree(pidlDesktop);
    return needRunAs;
}

int main()
{
    CLogUtil* commonLog = CLogUtil::GetLog(L"comm");

    LPWSTR commandLine = GetCommandLineW();
    commonLog->Log(__FILE__, __LINE__, ELogLevel::LOG_LEVEL_INFO, L"the command line : %s", commandLine);

    int numArgs;
    LPWSTR* argList = CommandLineToArgvW(commandLine, &numArgs);
    if (argList == NULL)
    {
        commonLog->Log(__FILE__, __LINE__, ELogLevel::LOG_LEVEL_ERROR, 
            L"failed to parse the command line, error is %d", GetLastError());        
        return 1;
    }    

    wstring chromePath;
    wstring pluginPath;
    bool isShortcut = false;
    for (int i = 0; i < numArgs; ++i)
    {
        if (wcscmp(argList[i], L"--chrome_path") == 0 && i + 1 < numArgs)
        {
            chromePath = argList[i + 1];
            i++;
        }
        else if (wcscmp(argList[i], L"--plugin_path") == 0 && i + 1 < numArgs)
        {
            pluginPath = argList[i + 1];
            i++;
        }
        else if (wcscmp(argList[i], L"--shortcut") == 0)
        {
            isShortcut = true;
        }
    }
    LocalFree(argList);
    argList = nullptr;

    CoInitialize(NULL);
    if (isShortcut)
    {
        g_dllLog = CLogUtil::GetLog(L"shortcut");
        if (pluginPath.empty())
        {
            LOG_ERROR(L"invalid param, plugin path is %s", pluginPath.c_str());
            return 1;
        }
        
        ModifyShortcutLink(pluginPath);
        return 0;
    }

    g_dllLog = CLogUtil::GetLog(L"main");
    if (chromePath.empty() || pluginPath.empty())
    {
        LOG_ERROR(L"invalid param, chrome path is %s, plugin path is %s", chromePath.c_str(), pluginPath.c_str());
        return 1;
    }

    // modify the shortcut link
    if (ModifyShortcutLink(pluginPath))
    {
        LOG_INFO(L"it will start the program with administrator");
        wstring cmdLine = commandLine;
        cmdLine += L" --shortcut";
        RunAs(cmdLine);
    }

    // Start the chrome program
    WCHAR chromeCmdLine[MAX_PATH*10];
    swprintf_s(chromeCmdLine, MAX_PATH*10, L"%s --load-extension=%s", chromePath.c_str(), pluginPath.c_str());
    STARTUPINFO startupInfo = { sizeof(startupInfo) };
    PROCESS_INFORMATION processInfo;
    if (CreateProcessW(NULL, chromeCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo))
    {     
        LOG_INFO(L"successful to start the Chrome");
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
    }
    else
    {
        LOG_ERROR(L"failed to start Chrome, error is %d", GetLastError());
        return 1;
    }

    return 0;
}