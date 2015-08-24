/*
 * winstore.c: Windows-specific implementation of the interface
 * defined in storage.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "putty.h"
#include "storage.h"

#include <shlobj.h>
#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA 0x001a
#endif
#ifndef CSIDL_LOCAL_APPDATA
#define CSIDL_LOCAL_APPDATA 0x001c
#endif

static const char *const reg_jumplist_key = PUTTY_REG_POS "\\Jumplist";
static const char *const reg_jumplist_value = "Recent sessions";
static const char *const puttystr = PUTTY_REG_POS "\\Sessions";

static const char hex[16] = "0123456789ABCDEF";

static int tried_shgetfolderpath = FALSE;
static HMODULE shell32_module = NULL;
DECL_WINDOWS_FUNCTION(static, HRESULT, SHGetFolderPathA, 
		      (HWND, int, HANDLE, DWORD, LPSTR));

#define REG_EITHER_STRING (0x4181)
/* any size result will be returned
 * returns of type REG_SZ or REG_EXPAND_SZ are of type TCHAR
 */
static LPBYTE strdupRegQueryValueEx(LONG *pret,HKEY hKey,LPCSTR Key,DWORD dwValType,DWORD *pcbBufLen)
{
  LPBYTE Result=NULL;
  BYTE szValue[256]; /* This will let us get the short ones in 1 try */
  DWORD dwMyValType=0,cbBufLen=sizeof(szValue);
  LONG ret=RegQueryValueEx(hKey, Key, NULL, &dwMyValType, szValue, &cbBufLen);
  if (ret == ERROR_SUCCESS) {
    if ((dwValType == dwMyValType) || ((dwValType==REG_EITHER_STRING) && ((dwMyValType==REG_SZ) || (dwMyValType==REG_EXPAND_SZ))) ) {
      Result=(LPBYTE)snewn(cbBufLen,char);
      memcpy(Result,szValue,cbBufLen);
    }
  } else if (ret == ERROR_MORE_DATA) {
    if ((dwValType == dwMyValType) || ((dwValType==REG_EITHER_STRING) && ((dwMyValType==REG_SZ) || (dwMyValType==REG_EXPAND_SZ))) ) {
      Result=(LPBYTE)snewn(cbBufLen,char);
      ret=RegQueryValueEx(hKey, Key, NULL, &dwMyValType, Result, &cbBufLen);
      if (ret != ERROR_SUCCESS) {
        sfree(Result);
        Result=NULL;
      }
    }
  }
  if (pret) *pret=ret;
  if (pcbBufLen) *pcbBufLen=Result?cbBufLen:0;
  return Result;
}

/* How to pick a special number that won't ever be a handle?
 * 0 is taken as NULL, handles are likely to be DWORD aligned so &1,&2,&3 won't ever be valid handles */
#define SESSKEY_ASFILE ((HANDLE)1) /* comment this out to disable Portable Putty INI */
#ifndef SESSKEY_ASFILE
#define setiniinfo(u) (NULL)
#else
static char g_szIniFile[MAX_PATH];
static char g_szSessionName[256];
#define SESSION_LEADCHAR ('^')
#define SSHHOSTKEY_LEADCHAR ('$')
#define WRITEBYSECTION /* Comment this out to eliminate section writes */
#ifdef WRITEBYSECTION
struct ARMString {
  char *szzSections;
  size_t cchSections; /* Characters currently used in sszSections (not counting the extra \0) */
  size_t cbSections;  /* Allocated size of sszSections */
} g_aSection={0};

/* Windows 95 is limited to 32,767 characters */
/* cchSectionData does not include the trailing \0 byte */
static LPBYTE strdupGetPrivateProfileSection(LPCSTR szApp,LPCSTR szIni,size_t *cchSectionData,size_t *cbSection)
{
  LPTSTR Result=NULL;
  char szBuf[4096];
  size_t cchBuf=lenof(szBuf),cbBuf=0;
  szBuf[0]='\0'; szBuf[1]='\0';
  DWORD cchLen=GetPrivateProfileSection(szApp, szBuf, cchBuf, szIni); /* cchLen includes the \0 after the last string but does not include the trailing \0 */
  if (cchLen < cchBuf-2) {
    cbBuf = (cchLen+1)*sizeof(szBuf[0]);
    Result=(LPBYTE)snewn(cbBuf,char);
    memset(Result,0x90,cbBuf);
    memcpy(Result,szBuf,cbBuf); /* Don't forget the trailing \0\0 */
  } else while (cchLen >= cchBuf-2) {
    cchBuf *= 2;  /* no smart way to get the buffer size on the first try so we just keep doubling until we get it all */
    if (Result) sfree(Result);
    cbBuf=cchBuf*sizeof(szBuf[0]);
    Result=snewn(cbBuf, char);
    cchLen=GetPrivateProfileSection(szApp, Result, cbBuf, szIni);
  }
  *cchSectionData=cchLen;
  *cbSection=cbBuf;
  return Result;
}

/* 0==strcmpequals("Key=","Key"); The strings are only considered equal when the first string ends in an = */
static int stricmpequals(const char *szStEquals,const char *szSt) {
  while(*szStEquals != '\0' && *szSt != '\0' && tolower(*szStEquals) == tolower(*szSt)) { szStEquals++; szSt++; }
  if (*szStEquals == '=' && *szSt=='\0') return 0;
  return -1;
}
/* Writing values one by one took 20 seconds for 25 PuTTY connections. That's too long 
 * and too abusive to flash drives which is where 99% of all portable PuTTY installations
 * are going so I wrote this. It imports all 25 sessions in less than a second. This is also better because
 * this way doesn't steadily reorder the file. The new sections are inserted where the
 * old sections were. Saving settings will also be faster and less destructive to flash drives. */
/* We always put \0\0 on the end so that this is ready to be WritePrivateProfileSection at any time */
/* bUpdate=0, add only, do not check for existing values */
/* bUpdate=1, add new or update existing value as necessary */
static BOOL WritePrivateProfileStringSection(const char *szKey,const char *szValue,struct ARMString *aSections,int bUpdate) {
  size_t cbMore;
  char *szzSections=aSections->szzSections;
  size_t cchSections=aSections->cchSections,cbSections=aSections->cbSections;
  char *szSectionKey="";
  if (szzSections && bUpdate) for(szSectionKey=szzSections; *szSectionKey; szSectionKey += strlen(szSectionKey)+1) {
    if (0==stricmpequals(szSectionKey,szKey)) break;
  }
  if (*szSectionKey) { /* Update */
    size_t cbOld=(strlen(szSectionKey)+1)*sizeof(*szSectionKey);
    size_t cbNew=szValue?((strlen(szKey)+1+strlen(szValue)+1)*sizeof(*szValue)):0;
    if (cbNew>cbOld) { /* Expand */
      cbMore = cbNew - cbOld;
      if (cbSections < (cchSections+1)*sizeof(*szzSections)+cbMore) {
        char *szzSectionsOld;
        if ((cbSections  & (cbSections-1))) cbSections=1; /* Force a power of two */
        if (cbSections<8192) cbSections=8192; /* Each full size PuTTY Session is about 4500 bytes so larger should eliminate almost all reallocs() */
        while(cbSections<(cchSections+1)*sizeof(*szzSections)+cbMore) cbSections *= 2;
        szzSections = srealloc(szzSectionsOld=szzSections,cbSections);
        szSectionKey += (szzSections-szzSectionsOld); /* Maybe realloc() moved our string */
      }
      memmove((char *)szSectionKey+cbMore,szSectionKey,(cchSections-(szSectionKey-szzSections)+1)*sizeof(*szSectionKey));
      cchSections+=cbMore/sizeof(*szSectionKey);
    } else if (cbNew<cbOld) { /* Contract */
#define cbLess cbMore
      cbLess = cbOld - cbNew;
      memmove(szSectionKey,(char *)szSectionKey+cbLess,(cchSections-(szSectionKey-szzSections)+1)*sizeof(*szSectionKey)-cbLess);
      cchSections -= cbLess/sizeof(*szSectionKey);
#undef cbLess
    }
    if (szValue) sprintf(szSectionKey,"%s=%s",szKey,szValue);
  } else { /* Add */
    if (szValue) {
      cbMore=(strlen(szKey)+1+strlen(szValue)+1)*sizeof(*szKey); /* Key=Value\0\0 */
      if (!szzSections) {
        cchSections=0;
        cbSections=0; 
      }
      if (!szzSections || cbSections < (cchSections+1)*sizeof(*szzSections)+cbMore) {
        if ((cbSections  & (cbSections-1))) cbSections=1; /* Force a power of two */
        if (cbSections<8192) cbSections=8192; /* Each full size PuTTY Session is about 4500 bytes so larger should eliminate almost all reallocs() */
        while(cbSections<(cchSections+1)*sizeof(*szzSections)+cbMore) cbSections *= 2;
        szzSections = srealloc(szzSections,cbSections);
      }
      cchSections+=sprintf(szzSections+cchSections,"%s=%s%c",szKey,szValue,'\0');
    }
  }
  aSections->szzSections=szzSections;
  aSections->cchSections=cchSections;
  aSections->cbSections=cbSections;
  return 1;
}

static int __cdecl fcmp(const void *vp,const void *vq) {
  const char **p=(const char **)vp;
  const char **q=(const char **)vq;
  return strcmp(*p,*q);
}

static void SortPrivateProfileSection(char *szzStrings) {
  size_t num;
  char *p;
  for(p=szzStrings,num=0; *p; p+=strlen(p)+1, num++);
  if (num>1) {
    char **szStrings=smalloc(num*sizeof(*szStrings));
    char *szzNewStrings=smalloc((p-szzStrings+1)*sizeof(*szzNewStrings));
    char *q;
    size_t num2;
    for(p=szzStrings,num=0; *p; p+=strlen(p)+1, num++) szStrings[num]=p;
    qsort(szStrings,num,sizeof(*szStrings),fcmp);
    for(q=szzNewStrings,num2=num,num=0; num<num2; num++) {
      size_t cbString=strlen(szStrings[num])+1;
      memcpy(q,szStrings[num],cbString);
      q+=cbString;
    }
    *q++='\0';
    memcpy(szzStrings,szzNewStrings,p-szzStrings+1);
    sfree(szStrings);
    sfree(szzNewStrings);
  }
}
#endif

static LPBYTE strdupGetPrivateProfileString(LPCSTR szApp,LPCSTR Key,LPCSTR szIni)
{
  LPTSTR Result=NULL;
  char szBuf[8192];
  size_t cchBuf=lenof(szBuf);
  DWORD cchLen=GetPrivateProfileString(szApp, Key, "\x01", szBuf, cchBuf, szIni); /* cchLen does not include the trailing nul byte */
  if (cchLen != 1 || szBuf[0]!='\x01') {
    if (cchLen < cchBuf-1) {
      Result=(LPBYTE)snewn((cchLen+1)*sizeof(szBuf[0]),char);
      memcpy(Result,szBuf,(cchLen+1)*sizeof(szBuf[0])); /* Don't forget the trailing \0 */
    } else while (cchLen >= cchBuf-1) {
      cchBuf *= 2; /* no smart way to get the buffer size on the first try so we just keep doubling until we get it all */
      if (Result) sfree(Result);
      Result=snewn(cchBuf, char);
      cchLen=GetPrivateProfileString(szApp, Key, "\x01", Result, cchBuf, szIni);
    }
    if (cchLen == 1 && Result[0]=='\x01') {
      sfree(Result);
      Result=NULL;
    }
  }
  return Result;
}

static HKEY INIOpenFile(void) {
  DWORD rv=GetModuleFileName(NULL,g_szIniFile,lenof(g_szIniFile)); /* rv includes the \0 for this function */
  if (rv>5 && rv<lenof(g_szIniFile) && 0!=stricmp(g_szIniFile+rv-4,".INI") ) {
    strcpy(g_szIniFile+rv-4,".INI");
    HANDLE h=CreateFile(g_szIniFile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (h != INVALID_HANDLE_VALUE) {
      CloseHandle(h);
      return SESSKEY_ASFILE;
    }
  }
  return 0;
}

static HKEY setiniinfo(const char *sessionname) {
  HKEY key=INIOpenFile();
  if (key) {
    if (!sessionname) {
      g_szSessionName[0]='\0';
    } else {
      g_szSessionName[0]=SESSION_LEADCHAR;
      strncpy(g_szSessionName+1,sessionname,sizeof(g_szSessionName)-1); g_szSessionName[sizeof(g_szSessionName)-1]='\0';
    }
  }
  return key;
}

static void CopyRegToINIValues(HKEY hKey2, const char *szINISection,const char *szIniFile) {
  DWORD dwIndex2;
  char szValue[MAX_PATH+1],szData[256];
#ifdef WRITEBYSECTION
  struct ARMString aSections; aSections.szzSections=NULL;
#else
  WritePrivateProfileString(szINISection,NULL,NULL,szIniFile);
#endif
  for(dwIndex2=0; 1 ; dwIndex2++) {
    DWORD cbValue=lenof(szValue),cbData=lenof(szData),dwType=0;
    LONG rv=RegEnumValue(hKey2,dwIndex2,szValue,&cbValue,NULL,&dwType,szData,&cbData);
    if (rv==ERROR_NO_MORE_ITEMS) break;
    switch(dwType) {
    case REG_SZ: {
        char *p=NULL;
        if (rv==ERROR_MORE_DATA) {
          rv=RegEnumValue(hKey2,dwIndex2,szValue,&cbValue,NULL,&dwType,NULL,NULL); /* The clod Windows didn't fill in szValue even though it could have */
          if (rv==ERROR_SUCCESS) {
            p=strdupRegQueryValueEx(NULL, hKey2, szValue, dwType, NULL);
          }
        }
        if (rv==ERROR_SUCCESS) {
#ifndef WRITEBYSECTION
          WritePrivateProfileString(szINISection,szValue,p?p:szData,szIniFile);
#else
          WritePrivateProfileStringSection(szValue,p?p:szData,&aSections,0);
#endif
        }
        if (p) sfree(p);
      }
      break;
    case REG_DWORD: {
        DWORD *dwData=(DWORD *)szData;
        char st[16];
        sprintf(st,"%d",dwData[0]);
#ifndef WRITEBYSECTION
        WritePrivateProfileString(szINISection,szValue,st,szIniFile);
#else
        WritePrivateProfileStringSection(szValue,st,&aSections,0);
#endif
      } 
      break;
    }
  }
#ifdef WRITEBYSECTION
  if (aSections.szzSections) {
    SortPrivateProfileSection(aSections.szzSections);
    WritePrivateProfileSection(szINISection,aSections.szzSections,szIniFile);
    sfree(aSections.szzSections);
  } else WritePrivateProfileString(szINISection,NULL,NULL,szIniFile);
#endif
}

static void CopyRegToINI(const char *szIniFile) {
  HKEY hKey;
  if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_POS "\\SshHostKeys", &hKey) == ERROR_SUCCESS) {
    CopyRegToINIValues(hKey,"SshHostKeys",szIniFile);
    RegCloseKey(hKey);
  }
  if (RegOpenKey(HKEY_CURRENT_USER, puttystr, &hKey) == ERROR_SUCCESS) {
    DWORD dwIndex;
    char szKey[MAX_PATH+1+1];
    for(dwIndex=0; RegEnumKey(hKey,dwIndex,szKey+1,lenof(szKey)-1) == ERROR_SUCCESS; dwIndex++) {
      HKEY hKey2;
      char puttystr2[256];
      szKey[0]=SESSION_LEADCHAR;
      sprintf(puttystr2,"%s\\%s",puttystr,szKey+1);
      if (RegOpenKey(HKEY_CURRENT_USER, puttystr2, &hKey2) == ERROR_SUCCESS) {
        CopyRegToINIValues(hKey2,szKey,szIniFile);
        RegCloseKey(hKey2);
      }
    }
    RegCloseKey(hKey);
  }
}

static void CopyINIToRegValues(HKEY hKey2, const char *szSection,const char *szIniFile) {
  char *szzKeys=strdupGetPrivateProfileString(szSection,NULL,szIniFile);
  if (szzKeys) {
    char *szKey;
    for(szKey=szzKeys; *szKey; szKey+=strlen(szKey)+1) {
      char *szValue=strdupGetPrivateProfileString(szSection,szKey,szIniFile);
      if (szValue) {
        LONG n=strtol(szValue,NULL,10);
        char st[16];
        sprintf(st,"%ld",n);
        if (0==strcmp(szValue,st)) {
          DWORD dw=n;
          RegSetValueEx(hKey2, szKey, 0, REG_DWORD, (CONST BYTE *)&dw, sizeof(dw));
        } else RegSetValueEx(hKey2, szKey, 0, REG_SZ, szValue, strlen(szValue)+1);
        sfree(szValue);
      }
    }
    sfree(szzKeys);
  } 
}

static void CopyINIToReg(const char *szIniFile) {
  HKEY hKey;
  size_t cbputtystr=strlen(puttystr);
  if (RegCreateKey(HKEY_CURRENT_USER, PUTTY_REG_POS "\\SshHostKeys", &hKey) == ERROR_SUCCESS) {
    CopyINIToRegValues(hKey,"SshHostKeys",szIniFile);
    RegCloseKey(hKey);
  }
  if (RegCreateKey(HKEY_CURRENT_USER, puttystr, &hKey) == ERROR_SUCCESS) {
    char *szzSections=strdupGetPrivateProfileString(NULL,NULL,szIniFile);
    if (szzSections) {
      char *szSection;
      for(szSection=szzSections; *szSection; szSection+=strlen(szSection)+1) if (*szSection==SESSION_LEADCHAR && strlen(szSection)+cbputtystr < MAX_PATH) {
        HKEY hKey2;
        char puttystr2[MAX_PATH+1];
        sprintf(puttystr2,"%s\\%s",puttystr,szSection+1);
        RegDeleteKey(hKey,szSection+1);
        if (RegCreateKey(HKEY_CURRENT_USER, puttystr2, &hKey2) == ERROR_SUCCESS) {
          CopyINIToRegValues(hKey2,szSection,szIniFile);
          RegCloseKey(hKey2);
        }
      }
      sfree(szzSections);
    }
    RegCloseKey(hKey);
  }
}
#endif

static void mungestr(const char *in, char *out)
{
    int candot = 0;

    while (*in) {
	if (*in == ' ' || *in == '\\' || *in == '*' || *in == '?' ||
	    *in == '%' || *in < ' ' || *in > '~' || (*in == '.'
						     && !candot)) {
	    *out++ = '%';
	    *out++ = hex[((unsigned char) *in) >> 4];
	    *out++ = hex[((unsigned char) *in) & 15];
	} else
	    *out++ = *in;
	in++;
	candot = 1;
    }
    *out = '\0';
    return;
}

static void unmungestr(const char *in, char *out, int outlen)
{
    while (*in) {
	if (*in == '%' && in[1] && in[2]) {
	    int i, j;

	    i = in[1] - '0';
	    i -= (i > 9 ? 7 : 0);
	    j = in[2] - '0';
	    j -= (j > 9 ? 7 : 0);

	    *out++ = (i << 4) + j;
	    if (!--outlen)
		return;
	    in += 3;
	} else {
	    *out++ = *in++;
	    if (!--outlen)
		return;
	}
    }
    *out = '\0';
    return;
}

void *open_settings_w(const char *sessionname, char **errmsg)
{
    char *p;
    HKEY sesskey;

    if (!sessionname || !*sessionname)
	sessionname = "Default Settings";

    p = snewn(3 * strlen(sessionname) + 1, char);
    mungestr(sessionname, p);

    *errmsg = NULL;
#ifdef SESSKEY_ASFILE
    sesskey = setiniinfo(p);
    if (sesskey) {
#ifdef WRITEBYSECTION
	g_aSection.szzSections=strdupGetPrivateProfileSection(g_szSessionName,g_szIniFile,&g_aSection.cchSections,&g_aSection.cbSections);
#endif
	sfree(p);
	return sesskey;
    } else 
#endif
    {
    HKEY subkey1;
    int ret;

    ret = RegCreateKey(HKEY_CURRENT_USER, puttystr, &subkey1);
    if (ret != ERROR_SUCCESS) {
	sfree(p);
        *errmsg = dupprintf("Unable to create registry key\n"
                            "HKEY_CURRENT_USER\\%s", puttystr);
	return NULL;
    }
    ret = RegCreateKey(subkey1, p, &sesskey);
    RegCloseKey(subkey1);
    if (ret != ERROR_SUCCESS) {
        *errmsg = dupprintf("Unable to create registry key\n"
                            "HKEY_CURRENT_USER\\%s\\%s", puttystr, p);
	sfree(p);
	return NULL;
    }
    sfree(p);
    return (void *) sesskey;
    }
}

void write_setting_s(void *handle, const char *key, const char *value)
{
#ifdef SESSKEY_ASFILE
    if (handle==SESSKEY_ASFILE) {
#ifdef WRITEBYSECTION
	g_aSection.szzSections
	?WritePrivateProfileStringSection(key,value,&g_aSection,1) 
 	:
#endif
	WritePrivateProfileString(g_szSessionName,key,value,g_szIniFile); 
    } else
#endif
    if (handle) {
	if (value)
	  RegSetValueEx((HKEY) handle, key, 0, REG_SZ, value, 1 + strlen(value));
        else
	  RegDeleteValue((HKEY) handle, key);
    }
}

void write_setting_i(void *handle, const char *key, int value)
{
#ifdef SESSKEY_ASFILE
    if (handle==SESSKEY_ASFILE) { 
        char value1[32];
	sprintf(value1,"%d",value);
#ifdef WRITEBYSECTION
	g_aSection.szzSections
  	  ?WritePrivateProfileStringSection(key,value1,&g_aSection,1) 
	  :
#endif
	  WritePrivateProfileString(g_szSessionName,key,value1,g_szIniFile); 
    } else
#endif
    if (handle)
	RegSetValueEx((HKEY) handle, key, 0, REG_DWORD,
		      (CONST BYTE *) &value, sizeof(value));
}

void close_settings_w(void *handle)
{
#ifdef SESSKEY_ASFILE
    if (handle==SESSKEY_ASFILE) {
#ifdef WRITEBYSECTION
      if (g_aSection.szzSections != NULL) {
	SortPrivateProfileSection(g_aSection.szzSections);
        WritePrivateProfileSection(g_szSessionName,g_aSection.szzSections,g_szIniFile);
        sfree(g_aSection.szzSections);
        g_aSection.szzSections=NULL;
      }
#endif
    } else
#endif
    RegCloseKey((HKEY) handle);
}

void *open_settings_r(const char *sessionname)
{
    HKEY sesskey;
    char *p;

    if (!sessionname || !*sessionname)
	sessionname = "Default Settings";

    p = snewn(3 * strlen(sessionname) + 1, char);
    mungestr(sessionname, p);

#ifdef SESSKEY_ASFILE
    sesskey = setiniinfo(p);
    if (sesskey) {
	sfree(p);
        p=strdupGetPrivateProfileString("Settings","LocalReg",g_szIniFile);
        if (!p) WritePrivateProfileString("Settings","LocalReg","none",g_szIniFile);
	else {
	    if (0==stricmp(p,"import")) CopyRegToINI(g_szIniFile);
	    else if (0==stricmp(p,"export")) CopyINIToReg(g_szIniFile);
	    if (0!=stricmp(p,"none")) WritePrivateProfileString("Settings","LocalReg","none",g_szIniFile);
	    sfree(p);
	}
	if (NULL==(p=strdupGetPrivateProfileString("Settings","LocalRegHelp",g_szIniFile))) {
	    WritePrivateProfileString("Settings","LocalRegHelp","LocalReg can be 'import' (from registry) 'export' (to registry), or 'none' (do nothing)",g_szIniFile);
	    sfree(p);
	}
	return sesskey;
    } else 
#endif
    {
    HKEY subkey1;

    if (RegOpenKey(HKEY_CURRENT_USER, puttystr, &subkey1) != ERROR_SUCCESS) {
	sesskey = NULL;
    } else {
	if (RegOpenKey(subkey1, p, &sesskey) != ERROR_SUCCESS) {
	    sesskey = NULL;
	}
	RegCloseKey(subkey1);
    }

    sfree(p);

    return (void *) sesskey;
    }
}

char *read_setting_s(void *handle, const char *key)
{
     return
#ifdef SESSKEY_ASFILE
      (handle == SESSKEY_ASFILE)
      ?strdupGetPrivateProfileString(g_szSessionName,key,g_szIniFile)
      :
#endif
       strdupRegQueryValueEx(NULL, handle, key, REG_SZ, NULL);
}

int read_setting_i(void *handle, const char *key, int defvalue)
{
    DWORD type, val, size;
    size = sizeof(val);

#ifdef SESSKEY_ASFILE
    if (handle==SESSKEY_ASFILE) {
        char *value1=read_setting_s(handle,key);
	int rv=(value1)?atoi(value1):defvalue;
	sfree(value1);
	return rv;
    } else 
#endif
    {
    if (!handle ||
	RegQueryValueEx((HKEY) handle, key, 0, &type,
			(BYTE *) &val, &size) != ERROR_SUCCESS ||
	size != sizeof(val) || type != REG_DWORD)
	return defvalue;
    else
	return val;
    }
}

FontSpec *read_setting_fontspec(void *handle, const char *name)
{
    char *settingname;
    char *fontname;
    FontSpec *ret;
    int isbold, height, charset;

    fontname = read_setting_s(handle, name);
    if (!fontname)
	return NULL;

    settingname = dupcat(name, "IsBold", NULL);
    isbold = read_setting_i(handle, settingname, -1);
    sfree(settingname);
    if (isbold == -1) {
        sfree(fontname);
        return NULL;
    }

    settingname = dupcat(name, "CharSet", NULL);
    charset = read_setting_i(handle, settingname, -1);
    sfree(settingname);
    if (charset == -1) {
        sfree(fontname);
        return NULL;
    }

    settingname = dupcat(name, "Height", NULL);
    height = read_setting_i(handle, settingname, INT_MIN);
    sfree(settingname);
    if (height == INT_MIN) {
        sfree(fontname);
        return NULL;
    }

    ret = fontspec_new(fontname, isbold, height, charset);
    sfree(fontname);
    return ret;
}

void write_setting_fontspec(void *handle, const char *name, FontSpec *font)
{
    char *settingname;

    write_setting_s(handle, name, font->name);
    settingname = dupcat(name, "IsBold", NULL);
    write_setting_i(handle, settingname, font->isbold);
    sfree(settingname);
    settingname = dupcat(name, "CharSet", NULL);
    write_setting_i(handle, settingname, font->charset);
    sfree(settingname);
    settingname = dupcat(name, "Height", NULL);
    write_setting_i(handle, settingname, font->height);
    sfree(settingname);
}

Filename *read_setting_filename(void *handle, const char *name)
{
    char *tmp = read_setting_s(handle, name);
    if (tmp) {
        Filename *ret = filename_from_str(tmp);
	sfree(tmp);
	return ret;
    } else
	return NULL;
}

void write_setting_filename(void *handle, const char *name, Filename *result)
{
    write_setting_s(handle, name, result->path);
}

void close_settings_r(void *handle)
{
#ifdef SESSKEY_ASFILE
    if (handle!=SESSKEY_ASFILE)
#endif
    RegCloseKey((HKEY) handle);
}

void del_settings(const char *sessionname)
{
    char *p;
#ifdef SESSKEY_ASFILE
    if (setiniinfo(NULL)) {
        p = snewn(3 * strlen(sessionname) + 2, char);
	*p=SESSION_LEADCHAR;
        mungestr(sessionname, p+1);
	WritePrivateProfileString(p,NULL,NULL,g_szIniFile);
    } else 
#endif
    {
    HKEY subkey1;

    if (RegOpenKey(HKEY_CURRENT_USER, puttystr, &subkey1) != ERROR_SUCCESS)
	return;

    p = snewn(3 * strlen(sessionname) + 1, char);
    mungestr(sessionname, p);
    RegDeleteKey(subkey1, p);

    RegCloseKey(subkey1);
    }

    sfree(p);
    remove_session_from_jumplist(sessionname);
}

struct enumsettings {
    HKEY key;
    int i;
#ifdef SESSKEY_ASFILE
    char *enumsettingsini,*p;
#endif
};

void *enum_settings_start(void)
{
    HKEY key;
    struct enumsettings *ret;
#ifdef SESSKEY_ASFILE
    key=setiniinfo(NULL);
    if (key) {
        ret = snew(struct enumsettings);
	ret->key=key;
	ret->i = 0;
	ret->p=ret->enumsettingsini=strdupGetPrivateProfileString(NULL,NULL,g_szIniFile);
    } else 
#endif
    {

    if (RegOpenKey(HKEY_CURRENT_USER, puttystr, &key) != ERROR_SUCCESS)
	return NULL;

    ret = snew(struct enumsettings);
    if (ret) {
	ret->key = key;
	ret->i = 0;
    }
    }
    return ret;
}

char *enum_settings_next(void *handle, char *buffer, int buflen)
{
    struct enumsettings *e = (struct enumsettings *) handle;
#ifdef SESSKEY_ASFILE
    if (e->key==SESSKEY_ASFILE) {
	while (1) {
	    if (!*e->p) return NULL;
	    if (*e->p == SESSION_LEADCHAR) {
		unmungestr(e->p+1,buffer,buflen);
		e->p+=strlen(e->p)+1;
		return buffer;
	    } else e->p+=strlen(e->p)+1;
	}
    } else 
#endif
    {
    char *otherbuf;
    otherbuf = snewn(3 * buflen, char);
    if (RegEnumKey(e->key, e->i++, otherbuf, 3 * buflen) == ERROR_SUCCESS) {
	unmungestr(otherbuf, buffer, buflen);
	sfree(otherbuf);
	return buffer;
    } else {
	sfree(otherbuf);
	return NULL;
    }
    }
}

void enum_settings_finish(void *handle)
{
    struct enumsettings *e = (struct enumsettings *) handle;
#ifdef SESSKEY_ASFILE
    if (e->key==SESSKEY_ASFILE) {
      sfree(e->enumsettingsini);
    } else 
#endif
    {
    RegCloseKey(e->key);
    }
    sfree(e);
}

/* strdup() like functions eliminate lots of code, are very secure, and 
 * make it obvious that the caller must free the buffer. All other functions
 * in the PuTTY sources that return a value to be free()'d should have
 * strdup_ added to the front. 
 */
static char *strdup_hostkey_regname(const char *hostname,
			    int port, const char *keytype)
{
    char *buffer=snewn(strlen(keytype)+1+5+1+3*strlen(hostname)+16,char);
    int len=sprintf(buffer,"%s@%u:",keytype,port);
    mungestr(hostname, buffer + len);
    return buffer;
}

int verify_host_key(const char *hostname, int port,
		    const char *keytype, const char *key)
{
    int rv=1; /* host key does not exist in registry */
    HKEY rkey=setiniinfo(NULL);
    /*
     * Now read a saved host key in from the registry and see what it
     * says.
     */
    if (rkey || RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_POS "\\SshHostKeys",
		   &rkey) == ERROR_SUCCESS) {
    char *regname = strdup_hostkey_regname(hostname, port, keytype);
    char *otherstr = 
#ifdef SESSKEY_ASFILE
      (rkey == SESSKEY_ASFILE)
      ?strdupGetPrivateProfileString("SshHostKeys",regname,g_szIniFile)
      :
#endif
       strdupRegQueryValueEx(NULL, rkey, regname, REG_SZ, NULL);

/* While I've fixed this code with the new strdup reg functions it doesn't work any more. 
 * This converts AAAABBBB/DDDDEEEEFFFF to 0xBBBBAAAA,0xFFFFEEEEDDDD. The conversion happens
 * in sets of 4 hex characters with any remainder drawing characters from outside the limits,
 * which for the first number only goes out of bounds into the second number. For the second 
 * number this would be a buffer overrun. Here's a sample SshHostKey in the new format:
 * 0x10001,0xc4f61b...
 * Note the 5 digit hex number. This routine with (ndigits/4) cannot produce a valid 5 digit hex number
 * anywhere. This code was only useful just after the format was updated. Years later it's 
 * completely useless and a security risk. -CJS 2014-04 */ 
#if 0
    if (!otherstr && rkey != SESSKEY_ASFILE && 
	!strcmp(keytype, "rsa") || 1) {
	/*
	 * Key didn't exist. If the key type is RSA, we'll try
	 * another trick, which is to look up the _old_ key format
	 * under just the hostname and translate that.
	 */
	char *oldstyle = strdupRegQueryValueEx(NULL, rkey, regname + 1 + strcspn(regname, ":"), REG_SZ, NULL, NULL);

	if (oldstyle) {
	    /*
	     * The old format is two old-style bignums separated by
	     * a slash. An old-style bignum is made of groups of
	     * four hex digits: digits are ordered in sensible
	     * (most to least significant) order within each group,
	     * but groups are ordered in silly (least to most)
	     * order within the bignum. The new format is two
	     * ordinary C-format hex numbers (0xABCDEFG...XYZ, with
	     * A nonzero except in the special case 0x0, which
	     * doesn't appear anyway in RSA keys) separated by a
	     * comma. All hex digits are lowercase in both formats.
	     */
	    char *p = otherstr = snewn(strlen(oldstyle)+16, char);
	    char *q = oldstyle;
	    int i, j;

	    for (i = 0; i < 2; i++) {
		int ndigits, nwords;
		*p++ = '0';
		*p++ = 'x';
		ndigits = strcspn(q, "/");	/* find / or end of string */
		nwords = ndigits / 4;
		/* now trim ndigits to remove leading zeros */
		while (q[(ndigits - 1) ^ 3] == '0' && ndigits > 1)
		    ndigits--;
		/* now move digits over to new string */
		for (j = 0; j < ndigits; j++)
		    p[ndigits - 1 - j] = q[j ^ 3];
		p += ndigits;
		q += nwords * 4;
		if (*q) {
		    q++;	       /* eat the slash */
		    *p++ = ',';	       /* add a comma */
		}
		*p = '\0';	       /* terminate the string */
	    }

	    /*
	     * Now _if_ this key matches, we'll enter it in the new
	     * format. If not, we'll assume something odd went
	     * wrong, and hyper-cautiously do nothing.
	     */
	    if (!strcmp(otherstr, key))
		RegSetValueEx(rkey, regname, 0, REG_SZ, otherstr,
			      strlen(otherstr) + 1);
	    sfree(oldstyle);
	}
    }
#endif

#ifdef SESSKEY_ASFILE
    if (rkey != SESSKEY_ASFILE) 
#endif
    RegCloseKey(rkey);
    sfree(regname);

    if (otherstr) {
	rv = strcmp(otherstr, key)?2:0; /* 2=key is different in registry, 0=key matched OK in registry */
	sfree(otherstr); 
    }
    }
    return rv;
}

void store_host_key(const char *hostname, int port,
		    const char *keytype, const char *key)
{

    char *regname = strdup_hostkey_regname(hostname, port, keytype);

    HKEY rkey = setiniinfo(NULL);
    if (rkey || RegCreateKey(HKEY_CURRENT_USER, PUTTY_REG_POS "\\SshHostKeys",
		     &rkey) == ERROR_SUCCESS) {
#ifdef SESSKEY_ASFILE
	if (rkey == SESSKEY_ASFILE) {
	  WritePrivateProfileString("SshHostKeys",regname,key,g_szIniFile);
        } else 
#endif
	{
	  RegSetValueEx(rkey, regname, 0, REG_SZ, key, strlen(key) + 1);
	  RegCloseKey(rkey);
        }
    } /* else key does not exist in registry */

    sfree(regname);
}

/*
 * Open (or delete) the random seed file.
 */
enum { DEL, OPEN_R, OPEN_W };
static int try_random_seed(char const *path, int action, HANDLE *ret)
{
    if (action == DEL) {
        if (!DeleteFile(path) && GetLastError() != ERROR_FILE_NOT_FOUND) {
            nonfatal("Unable to delete '%s': %s", path,
                     win_strerror(GetLastError()));
        }
	*ret = INVALID_HANDLE_VALUE;
	return FALSE;		       /* so we'll do the next ones too */
    }

    *ret = CreateFile(path,
		      action == OPEN_W ? GENERIC_WRITE : GENERIC_READ,
		      action == OPEN_W ? 0 : (FILE_SHARE_READ |
					      FILE_SHARE_WRITE),
		      NULL,
		      action == OPEN_W ? CREATE_ALWAYS : OPEN_EXISTING,
		      action == OPEN_W ? FILE_ATTRIBUTE_NORMAL : 0,
		      NULL);

    return (*ret != INVALID_HANDLE_VALUE);
}

static HANDLE access_random_seed(int action)
{
    HANDLE rethandle;

    char seedpath[2 * MAX_PATH + 10] = "\0";

    if (setiniinfo(NULL)) {
	strcpy(seedpath,g_szIniFile);
	strcpy(seedpath+strlen(seedpath)-4,".RND");
	if (*seedpath && try_random_seed(seedpath, action, &rethandle)) return rethandle;
    } else {
    HKEY rkey;
    DWORD type, size;
    /*
     * Iterate over a selection of possible random seed paths until
     * we find one that works.
     * 
     * We do this iteration separately for reading and writing,
     * meaning that we will automatically migrate random seed files
     * if a better location becomes available (by reading from the
     * best location in which we actually find one, and then
     * writing to the best location in which we can _create_ one).
     */

    /*
     * First, try the location specified by the user in the
     * Registry, if any.
     */
    size = sizeof(seedpath);
    if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_POS, &rkey) ==
	ERROR_SUCCESS) {
	int ret = RegQueryValueEx(rkey, "RandSeedFile",
				  0, &type, seedpath, &size);
	if (ret != ERROR_SUCCESS || type != REG_SZ)
	    seedpath[0] = '\0';
	RegCloseKey(rkey);

	if (*seedpath && try_random_seed(seedpath, action, &rethandle))
	    return rethandle;
    }

    /*
     * Next, try the user's local Application Data directory,
     * followed by their non-local one. This is found using the
     * SHGetFolderPath function, which won't be present on all
     * versions of Windows.
     */
    if (!tried_shgetfolderpath) {
	/* This is likely only to bear fruit on systems with IE5+
	 * installed, or WinMe/2K+. There is some faffing with
	 * SHFOLDER.DLL we could do to try to find an equivalent
	 * on older versions of Windows if we cared enough.
	 * However, the invocation below requires IE5+ anyway,
	 * so stuff that. */
	shell32_module = load_system32_dll("shell32.dll");
	GET_WINDOWS_FUNCTION(shell32_module, SHGetFolderPathA);
	tried_shgetfolderpath = TRUE;
    }
    if (p_SHGetFolderPathA) {
	if (SUCCEEDED(p_SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA,
					 NULL, SHGFP_TYPE_CURRENT, seedpath))) {
	    strcat(seedpath, "\\PUTTY.RND");
	    if (try_random_seed(seedpath, action, &rethandle))
		return rethandle;
	}

	if (SUCCEEDED(p_SHGetFolderPathA(NULL, CSIDL_APPDATA,
					 NULL, SHGFP_TYPE_CURRENT, seedpath))) {
	    strcat(seedpath, "\\PUTTY.RND");
	    if (try_random_seed(seedpath, action, &rethandle))
		return rethandle;
	}
    }

    /*
     * Failing that, try %HOMEDRIVE%%HOMEPATH% as a guess at the
     * user's home directory.
     */
    {
	int len, ret;

	len =
	    GetEnvironmentVariable("HOMEDRIVE", seedpath,
				   sizeof(seedpath));
	ret =
	    GetEnvironmentVariable("HOMEPATH", seedpath + len,
				   sizeof(seedpath) - len);
	if (ret != 0) {
	    strcat(seedpath, "\\PUTTY.RND");
	    if (try_random_seed(seedpath, action, &rethandle))
		return rethandle;
	}
    }

    /*
     * And finally, fall back to C:\WINDOWS.
     */
    GetWindowsDirectory(seedpath, sizeof(seedpath));
    strcat(seedpath, "\\PUTTY.RND");
    if (try_random_seed(seedpath, action, &rethandle))
	return rethandle;

    /*
     * If even that failed, give up.
     */
    }
    return INVALID_HANDLE_VALUE;
}

void read_random_seed(noise_consumer_t consumer)
{
    HANDLE seedf = access_random_seed(OPEN_R);

    if (seedf != INVALID_HANDLE_VALUE) {
	while (1) {
	    char buf[1024];
	    DWORD len;

	    if (ReadFile(seedf, buf, sizeof(buf), &len, NULL) && len)
		consumer(buf, len);
	    else
		break;
	}
	CloseHandle(seedf);
    }
}

void write_random_seed(void *data, int len)
{
    HANDLE seedf = access_random_seed(OPEN_W);

    if (seedf != INVALID_HANDLE_VALUE) {
	DWORD lenwritten;

	WriteFile(seedf, data, len, &lenwritten, NULL);
	CloseHandle(seedf);
    }
}

/*
 * Internal function supporting the jump list registry code. All the
 * functions to add, remove and read the list have substantially
 * similar content, so this is a generalisation of all of them which
 * transforms the list in the registry by prepending 'add' (if
 * non-null), removing 'rem' from what's left (if non-null), and
 * returning the resulting concatenated list of strings in 'out' (if
 * non-null).
 */
static int transform_jumplist_registry
    (const char *add, const char *rem, char **out)
{
    int ret;
    HKEY pjumplist_key, psettings_tmp;
    DWORD type;
    int value_length;
    char *old_value, *new_value;
    char *piterator_old, *piterator_new, *piterator_tmp;

    ret = RegCreateKeyEx(HKEY_CURRENT_USER, reg_jumplist_key, 0, NULL,
                         REG_OPTION_NON_VOLATILE, (KEY_READ | KEY_WRITE), NULL,
                         &pjumplist_key, NULL);
    if (ret != ERROR_SUCCESS) {
	return JUMPLISTREG_ERROR_KEYOPENCREATE_FAILURE;
    }

    /* Get current list of saved sessions in the registry. */
    value_length = 200;
    old_value = snewn(value_length, char);
    ret = RegQueryValueEx(pjumplist_key, reg_jumplist_value, NULL, &type,
                          old_value, &value_length);
    /* When the passed buffer is too small, ERROR_MORE_DATA is
     * returned and the required size is returned in the length
     * argument. */
    if (ret == ERROR_MORE_DATA) {
        sfree(old_value);
        old_value = snewn(value_length, char);
        ret = RegQueryValueEx(pjumplist_key, reg_jumplist_value, NULL, &type,
                              old_value, &value_length);
    }

    if (ret == ERROR_FILE_NOT_FOUND) {
        /* Value doesn't exist yet. Start from an empty value. */
        *old_value = '\0';
        *(old_value + 1) = '\0';
    } else if (ret != ERROR_SUCCESS) {
        /* Some non-recoverable error occurred. */
        sfree(old_value);
        RegCloseKey(pjumplist_key);
        return JUMPLISTREG_ERROR_VALUEREAD_FAILURE;
    } else if (type != REG_MULTI_SZ) {
        /* The value present in the registry has the wrong type: we
         * try to delete it and start from an empty value. */
        ret = RegDeleteValue(pjumplist_key, reg_jumplist_value);
        if (ret != ERROR_SUCCESS) {
            sfree(old_value);
            RegCloseKey(pjumplist_key);
            return JUMPLISTREG_ERROR_VALUEREAD_FAILURE;
        }

        *old_value = '\0';
        *(old_value + 1) = '\0';
    }

    /* Check validity of registry data: REG_MULTI_SZ value must end
     * with \0\0. */
    piterator_tmp = old_value;
    while (((piterator_tmp - old_value) < (value_length - 1)) &&
           !(*piterator_tmp == '\0' && *(piterator_tmp+1) == '\0')) {
        ++piterator_tmp;
    }

    if ((piterator_tmp - old_value) >= (value_length-1)) {
        /* Invalid value. Start from an empty value. */
        *old_value = '\0';
        *(old_value + 1) = '\0';
    }

    /*
     * Modify the list, if we're modifying.
     */
    if (add || rem) {
        /* Walk through the existing list and construct the new list of
         * saved sessions. */
        new_value = snewn(value_length + (add ? strlen(add) + 1 : 0), char);
        piterator_new = new_value;
        piterator_old = old_value;

        /* First add the new item to the beginning of the list. */
        if (add) {
            strcpy(piterator_new, add);
            piterator_new += strlen(piterator_new) + 1;
        }
        /* Now add the existing list, taking care to leave out the removed
         * item, if it was already in the existing list. */
        while (*piterator_old != '\0') {
            if (!rem || strcmp(piterator_old, rem) != 0) {
                /* Check if this is a valid session, otherwise don't add. */
                psettings_tmp = open_settings_r(piterator_old);
                if (psettings_tmp != NULL) {
                    close_settings_r(psettings_tmp);
                    strcpy(piterator_new, piterator_old);
                    piterator_new += strlen(piterator_new) + 1;
                }
            }
            piterator_old += strlen(piterator_old) + 1;
        }
        *piterator_new = '\0';
        ++piterator_new;

        /* Save the new list to the registry. */
        ret = RegSetValueEx(pjumplist_key, reg_jumplist_value, 0, REG_MULTI_SZ,
                            new_value, piterator_new - new_value);

        sfree(old_value);
        old_value = new_value;
    } else
        ret = ERROR_SUCCESS;

    /*
     * Either return or free the result.
     */
    if (out && ret == ERROR_SUCCESS)
        *out = old_value;
    else
        sfree(old_value);

    /* Clean up and return. */
    RegCloseKey(pjumplist_key);

    if (ret != ERROR_SUCCESS) {
        return JUMPLISTREG_ERROR_VALUEWRITE_FAILURE;
    } else {
        return JUMPLISTREG_OK;
    }
}

/* Adds a new entry to the jumplist entries in the registry. */
int add_to_jumplist_registry(const char *item)
{
/* Crapping up the registry with this Jumplist isn't allowed if using an INI file. */
    return setiniinfo(NULL)?0:transform_jumplist_registry(item, item, NULL);
}

/* Removes an item from the jumplist entries in the registry. */
int remove_from_jumplist_registry(const char *item)
{
    return setiniinfo(NULL)?0:transform_jumplist_registry(NULL, item, NULL);
}

/* Returns the jumplist entries from the registry. Caller must free
 * the returned pointer. */
char *get_jumplist_registry_entries (void)
{
    char *list_value;

    if (setiniinfo(NULL) || transform_jumplist_registry(NULL,NULL,&list_value) != JUMPLISTREG_OK) {
	list_value = snewn(2, char);
        *list_value = '\0';
        *(list_value + 1) = '\0';
    }
    return list_value;
}

/*
 * Recursively delete a registry key and everything under it.
 */
static void registry_recursive_remove(HKEY key)
{
    DWORD i;
    char name[MAX_PATH + 1];
    HKEY subkey;

    i = 0;
    while (RegEnumKey(key, i, name, sizeof(name)) == ERROR_SUCCESS) {
	if (RegOpenKey(key, name, &subkey) == ERROR_SUCCESS) {
	    registry_recursive_remove(subkey);
	    RegCloseKey(subkey);
	}
	RegDeleteKey(key, name);
    }
}

void cleanup_all(void)
{
    HKEY key;
    int ret;
    char name[MAX_PATH + 1];

    /* ------------------------------------------------------------
     * Wipe out the random seed file, in all of its possible
     * locations.
     */
    access_random_seed(DEL);

    /* ------------------------------------------------------------
     * Ask Windows to delete any jump list information associated
     * with this installation of PuTTY.
     */
    clear_jumplist();

    /* ------------------------------------------------------------
     * Destroy all registry information associated with PuTTY.
     */

    /*
     * Open the main PuTTY registry key and remove everything in it.
     */
    if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_POS, &key) ==
	ERROR_SUCCESS) {
	registry_recursive_remove(key);
	RegCloseKey(key);
    }
    /*
     * Now open the parent key and remove the PuTTY main key. Once
     * we've done that, see if the parent key has any other
     * children.
     */
    if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_PARENT,
		   &key) == ERROR_SUCCESS) {
	RegDeleteKey(key, PUTTY_REG_PARENT_CHILD);
	ret = RegEnumKey(key, 0, name, sizeof(name));
	RegCloseKey(key);
	/*
	 * If the parent key had no other children, we must delete
	 * it in its turn. That means opening the _grandparent_
	 * key.
	 */
	if (ret != ERROR_SUCCESS) {
	    if (RegOpenKey(HKEY_CURRENT_USER, PUTTY_REG_GPARENT,
			   &key) == ERROR_SUCCESS) {
		RegDeleteKey(key, PUTTY_REG_GPARENT_CHILD);
		RegCloseKey(key);
	    }
	}
    }
    /*
     * Now we're done.
     */
}
