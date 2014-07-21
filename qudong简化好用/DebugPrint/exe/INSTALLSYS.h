// INSTALLSYS.h: interface for the CINSTALLSYS class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INSTALLSYS_H__96AAA60C_1236_4D6B_AEA8_2001C9E98424__INCLUDED_)
#define AFX_INSTALLSYS_H__96AAA60C_1236_4D6B_AEA8_2001C9E98424__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CINSTALLSYS : public CObject  
{
public:
	CINSTALLSYS();
	virtual ~CINSTALLSYS();

public:
	BOOL InstallDriver(CString DriverName, CString DriverFromPath);
	BOOL UnInstallDriver(CString DriverName);
	void StartDriver();
	void StopDriver();
public:
    BOOL CreateDriverService(CString DriverName);
	BOOL StartDriverService(CString DriverName);
public:
	CString Replace(const CString csOriginalWord,const CString csWordDead,const CString csWordLive);
	BOOL IsDriverStarted(CString DriverName);
	int FindInMultiSz(LPTSTR MultiSz, int MultiSzLen, LPTSTR Match);
    BOOL RegeditKey(CString DriverName);
};

#endif // !defined(AFX_INSTALLSYS_H__96AAA60C_1236_4D6B_AEA8_2001C9E98424__INCLUDED_)
