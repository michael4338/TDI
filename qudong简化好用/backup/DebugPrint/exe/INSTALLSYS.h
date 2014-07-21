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
	void InstallDriver( CString DriverName, CString DriverFromPath);
	BOOL CreateDriver( CString DriverName, CString FullDriver);
	BOOL StartDriver(CString DriverName);
	int FindInMultiSz(LPTSTR MultiSz, int MultiSzLen, LPTSTR Match);

};

#endif // !defined(AFX_INSTALLSYS_H__96AAA60C_1236_4D6B_AEA8_2001C9E98424__INCLUDED_)
