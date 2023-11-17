#pragma once
#include <string>

//ְ�𣺳��������������С��ת���ļ�
struct _EXCEPTION_POINTERS;
class CDumpUtil
{
public:
    /**
    @name ����ת���ļ�����·��
    */
    static void SetDumpFilePath(const wchar_t* szDumpFilePath);

    /**
    @name �����Ƿ�Ҫ������С��ת���ļ�
    */
    static void Enable(bool bEnable);

private:
    static long __stdcall UnhandledExceptionFilter(struct _EXCEPTION_POINTERS* pExceptionInfo);
    static void DisableSetUnhandledExceptionFilter();

private:
    static std::wstring m_strDumpFilePath;
};