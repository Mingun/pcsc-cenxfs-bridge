/******************************************************************************
*                                                                             *
* xfsspi.h      XFS - SPI functions, types, and definitions                   *
*                                                                             *
*               Version 3.00  --  10/18/00                                    *
*                                                                             *
******************************************************************************/

#ifndef __inc_xfsspi__h
#define __inc_xfsspi__h

#ifdef __cplusplus
extern "C" {
#endif

//#define SPI_API extern WINAPI
#define SPI_API __declspec(dllexport)

#include <xfsapi.h>

typedef HANDLE HPROVIDER; 

#include <xfsconf.h>
#include <xfsadmin.h>

/*   be aware of alignment   */
#pragma pack(push,1)

/****** SPI functions ********************************************************/

HRESULT SPI_API WFPCancelAsyncRequest ( HSERVICE hService, REQUESTID RequestID);

HRESULT SPI_API WFPClose ( HSERVICE hService, HWND hWnd, REQUESTID ReqID);

HRESULT SPI_API WFPDeregister ( HSERVICE hService, DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID);

HRESULT SPI_API WFPExecute ( HSERVICE hService, DWORD dwCommand, LPVOID lpCmdData, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID);

HRESULT SPI_API WFPGetInfo ( HSERVICE hService, DWORD dwCategory, LPVOID lpQueryDetails, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID);

HRESULT SPI_API WFPLock ( HSERVICE hService, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID);

HRESULT SPI_API WFPOpen ( HSERVICE hService, LPSTR lpszLogicalName, HAPP hApp, LPSTR lpszAppID, DWORD dwTraceLevel, DWORD dwTimeOut, HWND hWnd, REQUESTID ReqID, HPROVIDER hProvider, DWORD dwSPIVersionsRequired, LPWFSVERSION lpSPIVersion, DWORD dwSrvcVersionsRequired, LPWFSVERSION lpSrvcVersion);

HRESULT SPI_API WFPRegister ( HSERVICE hService,  DWORD dwEventClass, HWND hWndReg, HWND hWnd, REQUESTID ReqID);

HRESULT SPI_API WFPSetTraceLevel ( HSERVICE hService, DWORD dwTraceLevel);

HRESULT SPI_API WFPUnloadService (  );

HRESULT SPI_API WFPUnlock ( HSERVICE hService, HWND hWnd, REQUESTID ReqID);


/*   restore alignment   */
#pragma pack(pop)

#ifdef __cplusplus
}    /*extern "C"*/
#endif

#endif  /* __inc_xfsspi__h */

