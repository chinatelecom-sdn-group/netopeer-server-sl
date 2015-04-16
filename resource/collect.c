/*
* Copyright (c) 2015 GuangZhou Research Institute of China Telecom . and others.  All rights reserved.
*
* This program and the accompanying materials are made available under the
* terms of the Eclipse Public License v1.0 which accompanies this distribution,
* and is available at http://www.eclipse.org/legal/epl-v10.html
*/
/**
* This model implements system config reading and init system params
* <p/>
*
* @author Peng li (chinatelecom.sdn.group@gmail.com)
* @version 0.1
*          <p/>
* @since 2015-03-23
*/

#include "collect.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <resolv.h>
#include <sys/ioctl.h>
#include "net/if.h"
#include "arpa/inet.h"
#include "linux/sockios.h"
#include <sys/statfs.h>
#include <sys/vfs.h>
#include <errno.h>
/* resource status structure */

#define NCPUSTATES 7
/* these are for calculating cpu state percentages */
static char *cpustatenames[NCPUSTATES + 1] = { "user", "nice", "system", "idle",
	"iowait", "irq", "sofrirq", NULL
};
static long cpustat[NCPUSTATES];

typedef struct {
	int pid;
	int lwp;
	int psr;
} lwpinfo;
typedef struct {
	char progressName[128];
	lwpinfo LWPInfo[50];
} qemulwpinfo;
typedef struct {
	char progressName[128];
	lwpinfo LWPInfo[20];
} vswitchdlwpinfo;
typedef struct {
	char progressName[128];
	lwpinfo LWPInfo[2];
} ovsdblwpinfo;
typedef struct {
	char progressName[128];
	lwpinfo LWPInfo[10];
} dpdklwpinfo;
typedef struct {
	char netdevType[32];
	char netdevId[32];
	int netdevvhostfd;
	char netdevIfname[32];
	char netdevdevicetype[32];
	char netdevdevicemac[32];
} vmnetdevinfo;


typedef struct {
	char VMname[64];
	char VMuuid[64];
	long VMMemInfo;
	long VMCPUCores;
	vmnetdevinfo VMNetdevInfo[4];
	char VMVNC[32];
	int VMpid;
} vminfo;

typedef struct {
	char PortName[40];
	int PortSerialNo;
	int PortArrayIndex;
	char addr[40];
	char config[15];
	char state[15];
	char current[15];
	char speed[50];
	long long Rxbytes;
	long long Rxpkts;
	long long Rxerrs;
	long long Rxdrop;
	long long Rxframe;
	long long Rxover;
	long long Rxcrc;
	long long Txbytes;
	long long Txpkts;
	long long Txerrs;
	long long Txdrop;
	long long Txcolls;
} ovsport;

typedef struct {
	char BrName[20];
	ovsport ovsportlist[20];
	int PortTotalNum;
} ovsbr;

typedef struct {
	char devName[20];
	long long Rxbytes;
	long long Rxpackets;
	long long Rxerrs;
	long long Rxdrop;
	long long Rxfifo;
	long long Rxframe;
	long long Rxcompressed;
	long long Rxmulticast;
	long long Txbytes;
	long long Txpackets;
	long long Txerrs;
	long long Txdrop;
	long long Txfifo;
	long long Txcolls;
	long long Txcarrier;
	long long Txcompressed;
	char mac[27];
	char IP[16];
	char broadIp[16];
	char netMask[16];
	long mtu;
} netdev;

typedef struct {
	long MemTotal;
	long MemFree;
	long HugePages_Total;
	long HugePages_Free;
	long HugePages_Rsvd;
	long HugePages_Surp;
	long Hugepagesize;
} meminfo;

typedef struct {
	int cpuNum;
	double cpu_idle_precent;
} cpusingleinfo;
typedef struct {
	int cpuloNum;
	int cpuphNum;
	double cpu_idle_precent;
	char model_name[500];
	double cpu_Mhz;
	cpusingleinfo cpuSingleInfo[64];
} cpuinfo;

typedef struct {
	char Path[30];
	long long TotalSize;
	long long FreeSize;
} diskinfo;

/*
*@description skip NaN
*@param p - current poniter
*/
char * skip_char ( char *p )
{
	while ( *p>'9'||*p<'0' ) {
		p++;
	}
	return ( char * ) p;
}
/*
*@description get parenthese value 
*@param p - current poniter
*/
char * get_parenthese_value ( char *p )
{
	while ( *p!=')' ) {
		p++;
	}
	return ( char * ) p;
}

/*
*@description skip token
*@param p - current poniter
*/
static inline char *skip_token ( const char *p )
{
	while ( isspace ( *p ) )
		p++;
	while ( *p && !isspace ( *p ) )
		p++;
	return ( char * ) p;
}
/*
*@description get total time
*@param input_time - input time array
*/
long get_totle_time ( long *input_time )
{
	long totle_time = 0;
	int i;
	for ( i = 0; i < 7; i++ ) {
		totle_time = totle_time + input_time[i];
	}
	return totle_time;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////              cpu info get     /////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*@description get idle time 
*@param idle_time - idle_time
*@param totle_time - totle_time
*@return £¨cpu idle time£©
*/
int get_idle_totle_time ( long idle_time[65], long totle_time[65] )
{

	char szLine[512]= {0};
	char loopend='c';
	char *p;
	int count=0;
	//open /proc/stat
	FILE *file = fopen ( "/proc/stat", "r" );
	//get each line when the first char is 'c' caculate infomation
	for ( count=0; fgets ( szLine, sizeof ( szLine ), file ) &&szLine[0]==loopend; count++ ) {
		p = skip_token ( szLine ); /* "cpu" */
		cpustat[0] = strtoul ( p, &p, 0 );
		cpustat[1] = strtoul ( p, &p, 0 );
		cpustat[2] = strtoul ( p, &p, 0 );
		cpustat[3] = strtoul ( p, &p, 0 );
		idle_time[count] = cpustat[3];
		cpustat[4] = strtoul ( p, &p, 0 );
		cpustat[5] = strtoul ( p, &p, 0 );
		cpustat[6] = strtoul ( p, &p, 0 );
		totle_time[count] = get_totle_time ( cpustat );
	}
	fclose ( file );
	file = NULL;
	return count;
}
/*
*@description get cpu info
*@param CPUInfo - CPUInfo
*/
void get_cpu_info ( cpuinfo *CPUInfo )
{

	long totle_old_time[65]= {0};
	long totle_new_time[65]= {0};
	long idle_new_time[65]= {0};
	long idle_old_time[65]= {0};
	char tmpstring[256]= {0};
	//double cpu_idle_precent[65]= {0};
	int cpuNum=0;
	int loop = 0;
	/* getthe cpu old time info */
	cpuNum = get_idle_totle_time ( &idle_old_time, &totle_old_time );
	CPUInfo->cpuloNum=cpuNum-1;

	/*cpu collector wait 1 second*/
	sleep ( 5 );
	/* get the cpu new time info */
	get_idle_totle_time ( &idle_new_time, &totle_new_time );
	CPUInfo->cpu_idle_precent= ( double ) ( idle_new_time[0] - idle_old_time[0] )
		/ ( double ) ( totle_new_time[0] - totle_old_time[0] ) * 100;
	//caculate eche core and total cpu_idle_precent
	for ( loop=1; loop<cpuNum; loop++ ) {
		CPUInfo->cpuSingleInfo[loop-1].cpuNum=loop-1;
		CPUInfo->cpuSingleInfo[loop-1].cpu_idle_precent = ( double ) ( idle_new_time[loop] - idle_old_time[loop] )
			/ ( double ) ( totle_new_time[loop] - totle_old_time[loop] ) * 100;
	}
	//get cpu hardware info from /proc/cpuinfo
	int i=0;
	int breaksign = 0;
	FILE *file=fopen ( "/proc/cpuinfo","r" );
	char *tmppoint;
	int physicalid = 0;
	while ( fgets ( tmpstring, sizeof ( tmpstring ), file ) && ( breaksign == 0 ) ) {
		tmppoint =strstr ( tmpstring,"model name	: " );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			tmppoint++;
			//sscanf(tmppoint,"%s",CPUInfo->model_name);
			//memcpy ( CPUInfo->model_name,tmppoint,strlen(tmppoint)-1);
			//trncpy (CPUInfo->model_name,CPUInfo->model_name,strlen(CPUInfo->model_name));
			strncpy ( CPUInfo->model_name,tmppoint,strlen(tmppoint));
			strcat(CPUInfo->model_name,"\000");
		}
		tmppoint =strstr ( tmpstring,"cpu MHz		: " );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			CPUInfo->cpu_Mhz=strtod ( tmppoint,&tmppoint );
		}

		tmppoint =strstr ( tmpstring,"physical id		: " );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			physicalid=strtod ( tmppoint,&tmppoint );
		}
		tmppoint =strstr ( tmpstring,"cpu cores	: " );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			if(physicalid != 0){
				CPUInfo->cpuphNum=strtoul ( tmppoint,&tmppoint,0 ) * physicalid;
			}
			//breaksign=1;
		}
	}
	fclose ( file );
	file = NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////              netdev info get     /////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*@description get netdev info
*@param netdevtmp - netdevtmp
*/
void dispNetInfo ( netdev *netdevtmp )
{
	int s = socket ( AF_INET, SOCK_DGRAM, 0 );
	if ( s < 0 ) {
		//debug ( "Create socket failed by %s!\n", strerror ( errno ) );
		return;
	}
	struct ifreq ifr;
	char mactmp[27]= {0};
	unsigned char mac[6];
	unsigned long nIP, nNetmask, nBroadIP;

	strcpy ( ifr.ifr_name, netdevtmp->devName );
	if ( ioctl ( s, SIOCGIFHWADDR, &ifr ) < 0 ) {
		return;
	}
	memcpy ( mac, ifr.ifr_hwaddr.sa_data, sizeof ( mac ) );
	sprintf ( netdevtmp->mac,"%02X:%02X:%02X:%02X:%02X:%02X\000", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

	if ( ioctl ( s, SIOCGIFADDR, &ifr ) < 0 )
		nIP = 0;
	else
		nIP = * ( unsigned long* ) &ifr.ifr_broadaddr.sa_data[2];

	strcpy ( netdevtmp->IP,  inet_ntoa ( * ( struct in_addr* ) &nIP ) );

	//fprintf(stderr, "\tIP:%s\n", inet_ntoa(*(struct in_addr*)&nIP));

	if ( ioctl ( s, SIOCGIFBRDADDR, &ifr ) < 0 )
		nBroadIP = 0;
	else
		nBroadIP = * ( unsigned long* ) &ifr.ifr_broadaddr.sa_data[2];
	strcpy ( netdevtmp->broadIp, inet_ntoa ( * ( struct in_addr* ) &nBroadIP ) );
	if ( ioctl ( s, SIOCGIFNETMASK, &ifr ) < 0 )
		nNetmask = 0;
	else
		nNetmask = * ( unsigned long* ) &ifr.ifr_netmask.sa_data[2];
	strcpy ( netdevtmp->netMask, inet_ntoa ( * ( struct in_addr* ) &nNetmask ) );
	strcpy ( ifr.ifr_name, netdevtmp->devName );
	if ( ioctl ( s, SIOCGIFMTU, &ifr ) < 0 )
		netdevtmp->mtu = 0;
	else
		netdevtmp->mtu = ifr.ifr_mtu;

	close ( s );
}
/*
*@description get netdev number
*@param netdevtmp - netdevtmp
*@return count
*/
int getNetCardCount ( netdev netdevtmp[15] )
{

	char datatitle[16][15] = {"Rbytes","Rpackets","Rerrs","Rdrop","Rfifo","Rframe","Rcompressed","Rmulticast","Tbytes","Tpackets","Terrs","Tdrop","Tfifo","Tcolls","Tcarrier","Tcompressed"};

	//netdevc.datatitle[0][0]=*datatitle[0][0]; //{"Rbytes","Rpackets","Rerrs","Rdrop","Rfifo","Rframe","Rcompressed","Rmulticast","Tbytes","Tpackets","Terrs","Tdrop","Tfifo","Tcolls","Tcarrier","Tcompressed"};
	int nCount = 0;
	FILE *file = fopen ( "/proc/net/dev", "r" );
	if ( !file ) {
		//debug ( "Open /proc/net/dev failed by %s !\n", strerror ( errno ) );
		return nCount;
	}

	char szLine[512];
	fgets ( szLine, sizeof ( szLine ), file );
	fgets ( szLine, sizeof ( szLine ), file );

	for ( nCount=0; fgets ( szLine, sizeof ( szLine ), file ); nCount++ ) {
		netdev* tmppoint = ( netdev* ) malloc ( sizeof ( netdev ) );
		int i=0;

		char szName[128] = {0};
		char* p;

		p = skip_token ( szLine );
		tmppoint->Rxbytes= strtol ( p, &p, 0 );
		tmppoint->Rxpackets= strtol ( p, &p, 0 );
		tmppoint->Rxerrs= strtol ( p, &p, 0 );
		tmppoint->Rxdrop= strtol ( p, &p, 0 );
		tmppoint->Rxfifo= strtol ( p, &p, 0 );
		tmppoint->Rxframe= strtol ( p, &p, 0 );
		tmppoint->Rxcompressed= strtol ( p, &p, 0 );
		tmppoint->Rxmulticast= strtol ( p, &p, 0 );
		tmppoint->Txbytes= strtol ( p, &p, 0 );
		tmppoint->Txpackets= strtol ( p, &p, 0 );
		tmppoint->Txerrs= strtol ( p, &p, 0 );
		tmppoint->Txdrop= strtol ( p, &p, 0 );
		tmppoint->Txfifo= strtol ( p, &p, 0 );
		tmppoint->Txcolls= strtol ( p, &p, 0 );
		tmppoint->Txcarrier= strtol ( p, &p, 0 );
		tmppoint->Txcompressed= strtol ( p, &p, 0 );

		sscanf ( szLine, "%s", szName );
		int nLen = strlen ( szName );
		if ( nLen <= 0 )
			continue;
		if ( szName[nLen - 1] == ':' )
			szName[nLen - 1] = '\0';
		strncpy ( tmppoint->devName,szName,nLen );
		memcpy ( &netdevtmp[nCount],tmppoint,sizeof ( netdev ) );
		free(tmppoint);
	}

	fclose ( file );
	file = NULL;
	int i =0;
	for ( i; i<nCount; i++ ) {
		dispNetInfo ( &netdevtmp[i] );
	}
	return nCount;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////              memory info get     /////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*@description get memory info
*@param MemInfo - MemInfo
*/
void get_meminfo ( meminfo* MemInfo )
{
	char tmpstring[128]= {0};
	FILE *file = fopen ( "/proc/meminfo","r" );

	int breaksign=0;
	char *tmppoint;;

	while ( fgets ( tmpstring, sizeof ( tmpstring ), file )  && ( breaksign == 0 ) ) {
		tmppoint =strstr ( tmpstring,"MemTotal:" );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			MemInfo->MemTotal=strtoul ( tmppoint,&tmppoint,0 );
		}
		tmppoint =strstr ( tmpstring,"MemFree:" );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			MemInfo->MemFree=strtoul ( tmppoint,&tmppoint,0 );
		}
		tmppoint =strstr ( tmpstring,"HugePages_Total:" );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			MemInfo->HugePages_Total=strtoul ( tmppoint,&tmppoint,0 );

		}
		tmppoint =strstr ( tmpstring,"HugePages_Free:" );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			MemInfo->HugePages_Free=strtoul ( tmppoint,&tmppoint,0 );

		}
		tmppoint =strstr ( tmpstring,"HugePages_Rsvd:" );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			MemInfo->HugePages_Rsvd=strtoul ( tmppoint,&tmppoint,0 );

		}
		tmppoint =strstr ( tmpstring,"HugePages_Surp:" );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			MemInfo->HugePages_Surp=strtoul ( tmppoint,&tmppoint,0 );

		}
		tmppoint =strstr ( tmpstring,"Hugepagesize:" );
		if ( tmppoint!= NULL ) {
			while ( *tmppoint!=':' ) {
				tmppoint++;
			}
			tmppoint++;
			MemInfo->Hugepagesize=strtoul ( tmppoint,&tmppoint,0 );
			breaksign=1;
		}
	}

	fclose ( file );
	file=NULL;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////              harddisk info get     ////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*@description get disk info
*@param harddiskInfo - harddiskInfo
*/
void get_hardDiskInfo ( diskinfo* harddiskInfo )
{
	struct statfs diskInfo;
	statfs ( harddiskInfo->Path, &diskInfo );
	harddiskInfo->TotalSize= ( diskInfo.f_bsize* diskInfo.f_blocks ) >>20;
	harddiskInfo->FreeSize= ( diskInfo.f_bsize* diskInfo.f_bfree ) >>20;
	//printf ("%s  disk_total=%dMB, disk_free=%dMB\n",harddiskInfo->Path, harddiskInfo->TotalSize,harddiskInfo->FreeSize);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////              ovs info get     ////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*@description get vSwitch info
*@param OVSBr - OVSBr
*@return br number
*/
int get_OVSInfo ( ovsbr OVSBr[5] )
{
	char buffer[64]= {0};
	FILE *file;
	file=popen ( "ovs-vsctl list-br","r" );
	int ovsbrloop=0;
	char * p;
	for ( ovsbrloop=0; fgets ( buffer,sizeof ( buffer ),file ); ovsbrloop++ ) {
		p=strstr ( buffer,"command not found" );
		if ( p==0 ) {
			strncpy ( OVSBr[ovsbrloop].BrName,buffer,strlen ( buffer )-1 );
		} else {
			return -1;
		}
	}
	pclose ( file );
	return ovsbrloop;
}
/*
*@description get vSwitch port info
*@param OVSBr - OVSBr
*@return port number
*/
int get_OVSPortInfo ( ovsbr * OVSBr )
{
	char buffer[256]= {0};
	char cmd[256]= {0};
	char tmpbrnam[64]= {0};
	char * tmppoint=NULL;
	int PortTotalNum=0;
	//////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////             ovs-ofctl dump-ports                   ///////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////
	int portloop=0;
	strcpy ( cmd,"ovs-ofctl dump-ports " );
	strcat ( cmd,OVSBr->BrName );
	FILE *file;
	file=popen ( cmd,"r" );
	fgets ( buffer,sizeof ( buffer ),file );
	if(buffer[0]!='\000'){
	tmppoint = strstr ( buffer,": " );
	tmppoint++;
	tmppoint++;
	PortTotalNum=strtod ( tmppoint,&tmppoint );
	OVSBr->PortTotalNum=PortTotalNum+1;
	for ( portloop=0; portloop<OVSBr->PortTotalNum; portloop++ ) {
		fgets ( buffer,sizeof ( buffer ),file );
		tmppoint = strstr ( buffer,"LOCAL" );
		if ( tmppoint != NULL ) {
			strcpy ( OVSBr->ovsportlist[portloop].PortName,"LOCAL\000" );
			OVSBr->ovsportlist[portloop].PortSerialNo=OVSBr->PortTotalNum=PortTotalNum;
			OVSBr->ovsportlist[portloop].PortArrayIndex = OVSBr->ovsportlist[portloop].PortSerialNo;
			tmppoint=skip_char ( buffer );
			OVSBr->ovsportlist[portloop].Rxpkts=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxbytes=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxdrop=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxerrs=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxframe=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxover=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxcrc=strtol ( tmppoint, &tmppoint, 0 );
			fgets ( buffer,sizeof ( buffer ),file );
			tmppoint = skip_char ( buffer );
			OVSBr->ovsportlist[portloop].Txpkts=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Txbytes=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Txdrop=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Txerrs=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Txcolls=strtol ( tmppoint, &tmppoint, 0 );
		} else {
			tmppoint=skip_token ( buffer );
			OVSBr->ovsportlist[portloop].PortSerialNo=strtol ( tmppoint, &tmppoint, 0 );
			OVSBr->ovsportlist[portloop].PortArrayIndex = OVSBr->ovsportlist[portloop].PortSerialNo;
			sprintf ( OVSBr->ovsportlist[portloop].PortName,"%d\000",OVSBr->ovsportlist[portloop].PortSerialNo );
			tmppoint=skip_char ( buffer );
			OVSBr->ovsportlist[portloop].Rxpkts=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxbytes=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxdrop=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxerrs=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxframe=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxover=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Rxcrc=strtol ( tmppoint, &tmppoint, 0 );
			fgets ( buffer,sizeof ( buffer ),file );
			tmppoint = skip_char ( buffer );
			OVSBr->ovsportlist[portloop].Txpkts=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Txbytes=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Txdrop=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Txerrs=strtol ( tmppoint, &tmppoint, 0 );
			tmppoint=skip_char ( tmppoint );
			OVSBr->ovsportlist[portloop].Txcolls=strtol ( tmppoint, &tmppoint, 0 );

		}
	}
	pclose ( file );
	//////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////             ovs-ofctl dump-ports-desc                   ///////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////
	file=NULL;
	portloop=0;
	strcpy ( cmd,"ovs-ofctl dump-ports-desc " );
	strcat ( cmd,OVSBr->BrName );
	file=popen ( cmd,"r" );
	fgets ( buffer,sizeof ( buffer ),file );
	for ( portloop=0; portloop<OVSBr->PortTotalNum; portloop++ ) {
		int tmpportserialnum;
		fgets ( buffer,sizeof ( buffer ),file );
		tmpportserialnum=strtol ( buffer,&tmppoint,0 );
		//printf("the tmpportserialnum is %d\n",tmpportserialnum);
		int tmpnumofport;
		for ( tmpnumofport=0; tmpnumofport<OVSBr->PortTotalNum; tmpnumofport++ ) {
			//printf("loop is %d\n",tmpnumofport);
			//printf("the match num is %d\n",OVSBr->ovsportlist[tmpnumofport].PortSerialNo);
			if ( OVSBr->ovsportlist[tmpnumofport].PortArrayIndex==tmpportserialnum ) {
				//printf("the PortSerialNo is %d\n",OVSBr->ovsportlist[tmpnumofport].PortSerialNo);
				//printf("the array index num is %d\n",tmpnumofport);
				break;
			}
		}
		tmppoint++;
		//printf("PortName before set is %s\n",OVSBr->ovsportlist[tmpnumofport].PortName);
		memset(OVSBr->ovsportlist[tmpnumofport].PortName,0,40);
		//printf("PortName after set is %s\n",OVSBr->ovsportlist[tmpnumofport].PortName);
		int temp = get_parenthese_value ( tmppoint )-tmppoint;
		//printf("the length of tmp %d \n",temp);
		strncpy ( OVSBr->ovsportlist[tmpnumofport].PortName,tmppoint,get_parenthese_value ( tmppoint )-tmppoint );
		//printf("the PortName is %s\n",OVSBr->ovsportlist[tmpnumofport].PortName);
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////          ovs-pfactl -c f -n 4 --proc-type=secondary -- get-portid dpdk@dp PortName                  ///////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		char *pftmppoint = NULL;
		pftmppoint = strstr(OVSBr->ovsportlist[tmpnumofport].PortName,"LOCAL");
		if(pftmppoint == NULL){
			char pfbuffer[512]= {0};
			char ovspfcmd[512] ={0};
			char *pffiletmppoint = NULL;
		        strcpy ( ovspfcmd,"ovs-pfactl -c f -n 4 --proc-type=secondary -- get-portid dpdk@dp " );
	                strcat ( ovspfcmd,OVSBr->ovsportlist[tmpnumofport].PortName );
		        FILE *pffile;
			pffile=popen ( ovspfcmd,"r" );
                        while(fgets(pfbuffer,512,pffile) != NULL){
                        char *pffiletmppoint = NULL;
			pffiletmppoint = strstr(pfbuffer,"Port id is ");
			if(pffiletmppoint != NULL){
					pffiletmppoint = skip_char(pffiletmppoint);
					OVSBr->ovsportlist[tmpnumofport].PortSerialNo=strtol (pffiletmppoint, &pffiletmppoint, 0 );
				}
			}
			pclose(pffile);
		}
		tmppoint=strstr ( tmppoint,"addr:" );
		sscanf ( tmppoint+5,"%s",OVSBr->ovsportlist[tmpnumofport].addr );
		fgets ( buffer,sizeof ( buffer ),file );
		tmppoint=strstr ( buffer,"config:" );
		sscanf ( ( tmppoint+8 ),"%s",OVSBr->ovsportlist[tmpnumofport].config );

		fgets ( buffer,sizeof ( buffer ),file );
		tmppoint=strstr ( buffer,"state:" );
		sscanf ( ( tmppoint+8 ),"%s",OVSBr->ovsportlist[tmpnumofport].state );
		fgets ( buffer,sizeof ( buffer ),file );
		tmppoint=strstr ( buffer,"current:" );
		if ( tmppoint !=NULL ) {
			sscanf ( ( tmppoint+9 ),"%s",OVSBr->ovsportlist[tmpnumofport].current );
			fgets ( buffer,sizeof ( buffer ),file );
			tmppoint=strstr ( buffer,"speed:" );
			sscanf ( ( tmppoint+6 ),"%s",OVSBr->ovsportlist[tmpnumofport].speed );
		} else {
			tmppoint=strstr ( buffer,"speed:" );
			sscanf ( ( tmppoint+6 ),"%s",OVSBr->ovsportlist[tmpnumofport].speed );
		}
	}}
	pclose ( file );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////              qemu info get     ////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*@description get qemu info
*@param VMInfo - VMInfo
*@return vm number
*/
int get_QemuInfo ( vminfo VMInfo[] )
{
	char buffer[4096]= {0};
	char cmd[256]= {0};
	char * tmppoint=NULL;
	strcpy ( cmd,"ps -eo pid,args | grep qemu | grep -v grep | grep -v SCREEN " );
	FILE *file= popen ( cmd,"r" );
	int vminfoloop=-1;
	for ( vminfoloop=0; fgets ( buffer,sizeof ( buffer ),file ); vminfoloop++ ) {
		//printf("[%s]\n",buffer);
		VMInfo[vminfoloop].VMpid=strtol ( buffer,&tmppoint,0 );
		tmppoint=strstr ( tmppoint," -m " );
		if ( tmppoint ) {
			VMInfo[vminfoloop].VMMemInfo=strtol ( tmppoint+4,&tmppoint,0 );
		}
		tmppoint=strstr ( buffer,"-smp" );
		if ( tmppoint ) {
			VMInfo[vminfoloop].VMCPUCores=strtol ( tmppoint+4,&tmppoint,0 );
		}
		tmppoint=strstr ( buffer,"-name " );
		if ( tmppoint ) {
			sscanf ( tmppoint+6,"%[^ ]",VMInfo[vminfoloop].VMname );
		}
		tmppoint=strstr ( buffer,"-uuid " );
		if ( tmppoint ) {
			sscanf ( tmppoint+6,"%[^ ]",VMInfo[vminfoloop].VMuuid );
		}
		tmppoint=strstr ( buffer,"-vnc " );
		if ( tmppoint ) {
			sscanf ( tmppoint+5,"%[^ ]",VMInfo[vminfoloop].VMVNC );
		}

		//get the vm netdev info from ps command
		int vmnetdevloop=0;
		char * vmnetdevpoint=NULL;
	//get normal ifcae infomation
		vmnetdevpoint=strstr (buffer,"macaddr=");
		if(vmnetdevpoint){
			char * tmppoint=NULL;
			char * conservepointer=NULL;
			tmppoint=vmnetdevpoint;
			sscanf ( tmppoint+8,"%[^ ]",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevdevicemac );
			conservepointer=strstr(tmppoint,"ifname=");
			if(conservepointer){
				tmppoint=conservepointer;
				sscanf ( tmppoint+7,"%[^,]",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevIfname );
				}
			strcpy (VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevType,"normal\000");
			strcpy (VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevId,VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevIfname);
			strcpy (VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevdevicetype,"tap\000");
			vmnetdevloop++;
			vmnetdevpoint=NULL;
		}
		vmnetdevpoint=strstr ( buffer,"-netdev" );
		while ( vmnetdevpoint ) {
			char * conservepointer=NULL;
			tmppoint=strstr ( vmnetdevpoint,"type=" );
			if ( tmppoint==NULL ) {
				sscanf ( vmnetdevpoint+8,"%[^,]",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevType );
				tmppoint=vmnetdevpoint;
			} else {
				sscanf ( tmppoint+5,"%[^,]",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevType );
			}
			conservepointer=strstr ( tmppoint,"fd=" );
			if ( conservepointer ) {
				tmppoint=conservepointer;
				VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevvhostfd=strtol ( tmppoint+3,&tmppoint,0 );
			}
			conservepointer=strstr ( tmppoint,"id=" );
			if ( conservepointer ) {
				tmppoint=conservepointer;
				sscanf ( tmppoint+3,"%[^ ]",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevId );
				strcpy ( VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevIfname,VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevId );
			}
			conservepointer=strstr ( tmppoint,"ifname=" );
			if ( conservepointer ) {
				tmppoint=conservepointer;
				sscanf ( tmppoint+7,"%[^,]",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevIfname );
			}
			conservepointer=strstr ( tmppoint,"-device " );
			if ( conservepointer ) {
				tmppoint=conservepointer;
				sscanf ( tmppoint+8,"%[^,]",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevdevicetype );
			}
			conservepointer=strstr ( tmppoint,"mac=" );
			if ( conservepointer ) {
				tmppoint=conservepointer;
				sscanf ( tmppoint+4,"%[^ ]",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevdevicemac );
			}
			vmnetdevpoint=strstr ( tmppoint,"-netdev" );
			vmnetdevloop++;
		}
//		//if netdevIfname=null&&netdevmac=null , get the iface from virsh domiflist vmname
//		if ( VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevIfname ) {
//			char virshcmd[128]="virsh domiflist ";
//			char virshresultcache[128]= {0};
//			int virshloop=0;
//			//printf("%s\n",VMInfo[vminfoloop].VMname);
//			strcat ( virshcmd,VMInfo[vminfoloop].VMname );
//			//printf("%s\n",virshcmd);
//			FILE *virshfile= popen ( virshcmd,"r" );
//			fgets ( virshresultcache,sizeof ( virshresultcache ),virshfile );
//			fgets ( virshresultcache,sizeof ( virshresultcache ),virshfile );
//			while ( fgets ( virshresultcache,sizeof ( virshresultcache ),virshfile ) ) {
//				//printf("%s\n",virshresultcache);
//				char *virshtmppointer=NULL;
//				char virshifname[20]= {0};
//				char virshmac[20]= {0};
//				sscanf ( virshresultcache,"%[^ ]",virshifname );
//				//printf("%s\n",virshifname);
//				//virshtmppointer= strstr( virshresultcache,":");
//				sscanf ( virshresultcache+strlen ( virshresultcache )-18,"%s",virshmac );
//				for ( virshloop=0; virshloop<=vmnetdevloop; virshloop++ ) {
//					if ( strcmp ( VMInfo[vminfoloop].VMNetdevInfo[virshloop].netdevdevicemac,virshmac ) ==0 ) {
//						strcpy ( VMInfo[vminfoloop].VMNetdevInfo[virshloop].netdevIfname,virshifname );
//					}
//				}
//			}
//			pclose ( virshfile );
//		}
	}
	pclose ( file );
	return vminfoloop;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////              pid info get     ////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////            qemu pid info get     ////////////////////////////////////////////
int get_QemuPidInfo ( qemulwpinfo QemuLwpInfo[] )
{
	char* qemutmppointer=NULL;
	char buffer[256]= {0};
	FILE *file=popen ( "ps -eLo pid,lwp,psr,args | grep qemu- | grep -v SCREEN | grep -v grep","r" );
	int qemupidloop=0;
	int lwploop=0;
	while ( fgets ( buffer,sizeof ( buffer ),file ) ) {
		char progressNametmp[32]= {0};
		//printf ( "%s",buffer );
		qemutmppointer = strstr ( buffer,"-name " );
		if ( qemutmppointer ) {
			sscanf ( qemutmppointer+6,"%s",progressNametmp );
		} else {
			qemutmppointer=NULL;
			continue;
		}
		if ( strcmp ( QemuLwpInfo[qemupidloop].progressName,progressNametmp ) !=0 ) {
			//the pid is different
			qemupidloop++;
			//printf ( "%d\n",qemupidloop );
			strcpy ( QemuLwpInfo[qemupidloop].progressName,progressNametmp );
			//printf ( "next %s\n",QemuLwpInfo[qemupidloop].progressName );
			lwploop=0;
			sscanf ( buffer,"%d%d%d",&QemuLwpInfo[qemupidloop].LWPInfo[lwploop].pid,&QemuLwpInfo[qemupidloop].LWPInfo[lwploop].lwp,&QemuLwpInfo[qemupidloop].LWPInfo[lwploop].psr );
			lwploop++;

		} else {
			//the pid is same
			//printf ( "progressName = loopname %s\n",QemuLwpInfo[qemupidloop].progressName );
			sscanf ( buffer,"%d%d%d",&QemuLwpInfo[qemupidloop].LWPInfo[lwploop].pid,&QemuLwpInfo[qemupidloop].LWPInfo[lwploop].lwp,&QemuLwpInfo[qemupidloop].LWPInfo[lwploop].psr );
			lwploop++;
		}
		qemutmppointer=NULL;
	}
	pclose ( file );
	return qemupidloop;
}

//////////////////////            ovs-vswitchd pid info get     ////////////////////////////////////////////
int get_VswitchdInfo ( vswitchdlwpinfo *VswitchdLwpInfo )
{
	char buffer[256]= {0};
	FILE *file=popen ( "ps -eLo pid,lwp,psr,args | grep ovs-vswitchd | grep -v grep","r" );
	strcpy ( VswitchdLwpInfo->progressName,"ovs-vswitchd" );
	int vswitchlwploop=0;
	while ( fgets ( buffer,sizeof ( buffer ),file ) ) {
		//the pid is same
		sscanf ( buffer,"%d%d%d",&VswitchdLwpInfo->LWPInfo[vswitchlwploop].pid,&VswitchdLwpInfo->LWPInfo[vswitchlwploop].lwp,&VswitchdLwpInfo->LWPInfo[vswitchlwploop].psr );
		vswitchlwploop++;
	}
	pclose ( file );
	return vswitchlwploop;
}


//////////////////////            ovsdb-server pid info get     ////////////////////////////////////////////
int get_OvsdbInfo ( ovsdblwpinfo *OvsdbLwpInfo )
{
	char buffer[256]= {0};
	FILE *file=popen ( "ps -eLo pid,lwp,psr,args | grep ovsdb-server | grep -v grep","r" );
	strcpy ( OvsdbLwpInfo->progressName,"ovs-ovsdb" );
	int ovsdblwploop=0;
	while ( fgets ( buffer,sizeof ( buffer ),file ) ) {
		//the pid is same
		sscanf ( buffer,"%d%d%d",&OvsdbLwpInfo->LWPInfo[ovsdblwploop].pid,&OvsdbLwpInfo->LWPInfo[ovsdblwploop].lwp,&OvsdbLwpInfo->LWPInfo[ovsdblwploop].psr );
		ovsdblwploop++;
	}
	pclose ( file );
	return ovsdblwploop;
}

//////////////////////            ovsdb-server pid info get     ////////////////////////////////////////////
int get_DpdkInfo ( dpdklwpinfo *DpdkLwpInfo )
{
	char buffer[256]= {0};
	FILE *file=popen ( "ps -eLo pid,lwp,psr,args | grep ovs-dpdk | grep -v grep","r" );
	strcpy ( DpdkLwpInfo->progressName,"ovs-dpdk" );
	int dpdklwploop=0;
	while ( fgets ( buffer,sizeof ( buffer ),file ) ) {
		//the pid is same
		sscanf ( buffer,"%d%d%d",&DpdkLwpInfo->LWPInfo[dpdklwploop].pid,&DpdkLwpInfo->LWPInfo[dpdklwploop].lwp,&DpdkLwpInfo->LWPInfo[dpdklwploop].psr );
		dpdklwploop++;
	}
	pclose ( file );
	return dpdklwploop;
}

xmlDocPtr get_ResourceInfo(){
	xmlDocPtr state;
	xmlNodePtr root,CPUInfoNode,MemInfoNode,DiskInfoNode,NetdevInfoNode,logical_core_detail_node,cpulocorenode,NetdevSingle,DiskPathSingle;
    xmlNsPtr ns;
    //FILE *debugFile = fopen("./debug.txt","w+");
    xmlChar *xmlbuff;
    int buffersize;
    char stringvalue[512]= {0};
    //create xml file
    state = xmlNewDoc ( BAD_CAST "1.0" );
    //create root node
    root = xmlNewDocNode ( state, NULL, BAD_CAST "resource", NULL );
    xmlDocSetRootElement ( state, root );
    ns = xmlNewNs ( root, BAD_CAST "http://www.gsta.com/resource", NULL );
    xmlSetNs ( root, ns );

    //create 1level node CPUInfo MemInfo DiskInfo NetdevInfo
    CPUInfoNode = xmlNewChild ( root, NULL, BAD_CAST "CPUInfo", NULL );
    MemInfoNode = xmlNewChild ( root, NULL, BAD_CAST "MemInfo",NULL );
    NetdevInfoNode = xmlNewChild ( root, NULL, BAD_CAST "NetDevInfo",NULL );
    DiskInfoNode = xmlNewChild ( root, NULL, BAD_CAST "DiskInfo", NULL );
	//ÄÚ´æ·ÖÅä
	cpuinfo *CPUInfo= ( cpuinfo* ) malloc ( sizeof ( cpuinfo ) );
	meminfo *MemInfo= ( meminfo* ) malloc ( sizeof ( meminfo ) );
	ovsdblwpinfo *OVSDBLwpInfo= ( ovsdblwpinfo* ) malloc ( sizeof ( ovsdblwpinfo ) );
	vswitchdlwpinfo *VswitchdLwpInfo= ( vswitchdlwpinfo* ) malloc ( sizeof ( vswitchdlwpinfo ) );
	dpdklwpinfo *DPDKLwpInfo= ( dpdklwpinfo* ) malloc ( sizeof ( dpdklwpinfo ) );
     //printf("########################the CPU infomation##################\n");
   //get cpuinfo
    CPUInfo->cpuphNum = 0;
    get_cpu_info ( CPUInfo );
    xmlNewChild ( CPUInfoNode, NULL, BAD_CAST "ModelName", BAD_CAST CPUInfo->model_name );
    //xmlNewChild(CPUInfoNode,NULL,BAD_CAST "ModelName",BAD_CAST "Intel(R) Xeon(R) CPU E5-2609 0 @ 2.40GHz");
    sprintf ( stringvalue,"%g\000",CPUInfo->cpu_Mhz );
    xmlNewChild ( CPUInfoNode, NULL, BAD_CAST "CPUMhz", BAD_CAST stringvalue );
    if(CPUInfo->cpuphNum != 0){
        sprintf ( stringvalue,"%d\000",CPUInfo->cpuphNum );
        xmlNewChild ( CPUInfoNode, NULL, BAD_CAST "CPUPhNum",BAD_CAST stringvalue );
    }else{
        sprintf ( stringvalue,"%d\000",CPUInfo->cpuloNum );
        xmlNewChild ( CPUInfoNode, NULL, BAD_CAST "CPUPhNum",BAD_CAST stringvalue );
    }
    sprintf ( stringvalue,"%d\000",CPUInfo->cpuloNum );
    xmlNewChild ( CPUInfoNode, NULL, BAD_CAST "CPULoNum",BAD_CAST stringvalue );
    sprintf ( stringvalue,"%g\000",CPUInfo->cpu_idle_precent );
    xmlNewChild ( CPUInfoNode, NULL, BAD_CAST "CpuIdlePrecent",BAD_CAST stringvalue );
    //logical_core_detail_node=xmlNewChild(CPUInfoNode, NULL, BAD_CAST "CPUInfoNode",NULL);
//     //printf("################the each logical core idle precent##############");
    int cpuloop=0;
    for ( cpuloop=0; cpuloop<CPUInfo->cpuloNum; cpuloop++ ) {
        cpulocorenode=xmlNewChild ( CPUInfoNode,NULL,BAD_CAST "CPUInfoNode",NULL );
        sprintf ( stringvalue,"cpu%d\000",cpuloop );
        xmlNewChild ( cpulocorenode, NULL, BAD_CAST "CPUNum",stringvalue );
        sprintf ( stringvalue,"%g\000",CPUInfo->cpuSingleInfo[cpuloop].cpu_idle_precent );
        xmlNewChild ( cpulocorenode,NULL,BAD_CAST "CPUIdlePrecent",BAD_CAST stringvalue ) ;
        ////printf("	the cpu%d idle time precent is %g   \n",cpuloop,CPUInfo->cpuSingleInfo[cpuloop].cpu_idle_precent);
    }
    //xmlDocFormatDump(debugFile,state,1);
//    xmlDocDumpFormatMemory(state,&xmlbuff,&buffersize,1);
//    //printf("%s",(char*)xmlbuff);
//    xmlFree(xmlbuff);
//printf("########################the memory infomation##################\n");
    get_meminfo ( MemInfo );
    sprintf ( stringvalue,"%ld\000",MemInfo->MemTotal );
    xmlNewChild ( MemInfoNode, NULL, BAD_CAST "MemTotal", BAD_CAST stringvalue );
    sprintf ( stringvalue,"%ld\000",MemInfo->MemFree );
    xmlNewChild ( MemInfoNode, NULL, BAD_CAST "MemFree",BAD_CAST stringvalue );
    sprintf ( stringvalue,"%ld\000",MemInfo->HugePages_Total );
    xmlNewChild ( MemInfoNode, NULL, BAD_CAST "HugePagesTotal",BAD_CAST stringvalue );
    sprintf ( stringvalue,"%ld\000",MemInfo->HugePages_Free );
    xmlNewChild ( MemInfoNode, NULL, BAD_CAST "HugePagesFree",BAD_CAST stringvalue );
    sprintf ( stringvalue,"%ld\000",MemInfo->HugePages_Rsvd );
    xmlNewChild ( MemInfoNode, NULL, BAD_CAST "HugePagesRsvd",BAD_CAST stringvalue );
    sprintf ( stringvalue,"%ld\000",MemInfo->HugePages_Surp );
    xmlNewChild ( MemInfoNode, NULL, BAD_CAST "HugePagesSurp",BAD_CAST stringvalue );
    sprintf ( stringvalue,"%ld\000",MemInfo->Hugepagesize );
    xmlNewChild ( MemInfoNode, NULL, BAD_CAST "HugePageSize",BAD_CAST stringvalue );
//    xmlDocDumpFormatMemory(state,&xmlbuff,&buffersize,1);
//    //printf("%s",(char*)xmlbuff);
//    xmlFree(xmlbuff);
//printf("########################the netdev infomation##################\n");
    netdev NetdevInfo[60]= {0};
    int nicnum=getNetCardCount ( NetdevInfo );

    int netdevloop=0;
    for ( netdevloop=0; netdevloop<nicnum; netdevloop++ ) {
        NetdevSingle=xmlNewChild ( NetdevInfoNode, NULL,BAD_CAST "NetDevInfoDetail", NULL );
        ////printf("	######################this is nic%d#################\n",netdevloop);
        xmlNewChild ( NetdevSingle, NULL, BAD_CAST "DevName",BAD_CAST NetdevInfo[netdevloop].devName );
        xmlNewChild ( NetdevSingle, NULL, BAD_CAST "MAC",BAD_CAST NetdevInfo[netdevloop].mac );

//         //printf("                the nic%d name is %s  \n",netdevloop,NetdevInfo[netdevloop].devName);
//         //printf("                the nic%d mac is %s   \n",netdevloop,NetdevInfo[netdevloop].mac);
        if ( strlen ( NetdevInfo[netdevloop].IP ) !=0 ) {
            xmlNewChild ( NetdevSingle, NULL, BAD_CAST "IP",BAD_CAST NetdevInfo[netdevloop].IP );
            // //printf("                the nic%d ip addr is %s \n",netdevloop,NetdevInfo[netdevloop].IP);
        }
        if ( strlen ( NetdevInfo[netdevloop].broadIp ) !=0 ) {
            xmlNewChild ( NetdevSingle, NULL, BAD_CAST "BroadIp",BAD_CAST NetdevInfo[netdevloop].broadIp );
            ////printf("                the nic%d broadIp is %s   \n",netdevloop,NetdevInfo[netdevloop].broadIp);
        }
        if ( strlen ( NetdevInfo[netdevloop].netMask ) !=0 ) {
            xmlNewChild ( NetdevSingle, NULL, BAD_CAST "NetMask",BAD_CAST NetdevInfo[netdevloop].netMask );
            // //printf("                the nic%d netMask is %s   \n",netdevloop,NetdevInfo[netdevloop].netMask);
        }
        sprintf ( stringvalue,"%lld\000",NetdevInfo[netdevloop].Txbytes );
        xmlNewChild ( NetdevSingle, NULL, BAD_CAST "Txbytes",BAD_CAST stringvalue );
        //  //printf("                the nic%d Txbytes is %d   \n",netdevloop,NetdevInfo[netdevloop].Txbytes);

        sprintf ( stringvalue,"%lld\000",NetdevInfo[netdevloop].Txpackets );
        xmlNewChild ( NetdevSingle, NULL, BAD_CAST "Txpackets",BAD_CAST stringvalue );
        //  //printf("                the nic%d Txpackets is %d   \n",netdevloop,NetdevInfo[netdevloop].Txpackets);

        sprintf ( stringvalue,"%lld\000",NetdevInfo[netdevloop].Txdrop );
        xmlNewChild ( NetdevSingle, NULL, BAD_CAST "Txdrop",BAD_CAST stringvalue );
        //  //printf("                the nic%d Txdrop is %d   \n",netdevloop,NetdevInfo[netdevloop].Txdrop);

        sprintf ( stringvalue,"%lld\000",NetdevInfo[netdevloop].Rxbytes );
        xmlNewChild ( NetdevSingle, NULL, BAD_CAST "Rxbytes",BAD_CAST stringvalue );
        //  //printf("                the nic%d Rxbytes is %d   \n",netdevloop,NetdevInfo[netdevloop].Rxbytes);

        sprintf ( stringvalue,"%lld\000",NetdevInfo[netdevloop].Rxpackets );
        xmlNewChild ( NetdevSingle, NULL, BAD_CAST "Rxpackets",BAD_CAST stringvalue );
        //  //printf("                the nic%d Rxpackets is %d   \n",netdevloop,NetdevInfo[netdevloop].Rxpackets);

        sprintf ( stringvalue,"%lld\000",NetdevInfo[netdevloop].Rxdrop );
        xmlNewChild ( NetdevSingle, NULL, BAD_CAST "Rxdrop",BAD_CAST stringvalue );
        //  //printf("                the nic%d Rxdrop is %d   \n",netdevloop,NetdevInfo[netdevloop].Rxdrop);
    }
//    xmlDocDumpFormatMemory(state,&xmlbuff,&buffersize,1);
//    //printf("%s",(char*)xmlbuff);
//    xmlFree(xmlbuff);
//printf("########################the disk infomation##################\n");
    diskinfo DiskInfo[4]= {0};
    strcpy ( DiskInfo[0].Path,"/\000" );
    strcpy ( DiskInfo[1].Path,"/boot\000" );
    strcpy ( DiskInfo[2].Path,"/dev/shm\000" );
    strcpy ( DiskInfo[3].Path,"/home\000" );
    int diskloop=0;
    for ( diskloop=0; diskloop<4; diskloop++ ) {
        get_hardDiskInfo ( &DiskInfo[diskloop] );
        sprintf ( stringvalue,"part%d\000",diskloop );
        DiskPathSingle=xmlNewChild ( DiskInfoNode,NULL,BAD_CAST "Partition",NULL );
        xmlNewChild ( DiskPathSingle,NULL,BAD_CAST "Path",BAD_CAST DiskInfo[diskloop].Path );
        sprintf ( stringvalue,"%lld\000",DiskInfo[diskloop].TotalSize );
        xmlNewChild ( DiskPathSingle, NULL, BAD_CAST "Total", BAD_CAST stringvalue );
        sprintf ( stringvalue,"%lld\000",DiskInfo[diskloop].FreeSize );
        xmlNewChild ( DiskPathSingle,NULL,BAD_CAST "Free",BAD_CAST stringvalue );
    }
    //xmlNewChild(root, ns, BAD_CAST "Status", BAD_CAST (status->caculating ? "down" : "up"));
//    xmlDocDumpFormatMemory(state,&xmlbuff,&buffersize,1);
//    //printf("%s",(char*)xmlbuff);
//    xmlFree(xmlbuff);
//printf ( "########################the OVS infomation##################\n" );
    ovsbr OVSBr[5]= {0};
    int ovsbrnum = get_OVSInfo ( OVSBr );
    int ovsloop =0;
    if ( ovsbrnum>0 ) {
        xmlNodePtr OVSInfoNode,OVSBrNode,OVSPortNode;
        OVSInfoNode = xmlNewChild ( root, NULL, BAD_CAST "OVSInfo", NULL );

        for ( ovsloop=0; ovsloop<ovsbrnum; ovsloop++ ) {
            get_OVSPortInfo ( &OVSBr[ovsloop] );
            OVSBrNode=xmlNewChild ( OVSInfoNode, NULL, BAD_CAST "OVSBr", NULL );
            xmlNewChild ( OVSBrNode,NULL,BAD_CAST "BrName",OVSBr[ovsloop].BrName );
            sprintf ( stringvalue,"%d\000",OVSBr[ovsloop].PortTotalNum );
            xmlNewChild ( OVSBrNode,NULL,BAD_CAST "PortTotalNum",BAD_CAST stringvalue );
            //printf ( "                  #####################the %s br detail################################\n",OVSBr[ovsloop].BrName );
            //printf ( "                  #####################the %s br port total num is %d  #######################\n",OVSBr[ovsloop].BrName,OVSBr[ovsloop].PortTotalNum );
            //printf ( "                  ###### the %s br port detail##########################################\n",OVSBr[ovsloop].BrName );
            int portloop=0;
            for ( portloop=0; portloop<OVSBr[ovsloop].PortTotalNum; portloop++ ) {
                OVSPortNode=xmlNewChild ( OVSBrNode, NULL, BAD_CAST "OVSPortlist", NULL );
                xmlNewChild ( OVSPortNode, NULL, BAD_CAST "PortName",BAD_CAST OVSBr[ovsloop].ovsportlist[portloop].PortName );

                sprintf ( stringvalue,"%d\000",OVSBr[ovsloop].ovsportlist[portloop].PortSerialNo );
                xmlNewChild ( OVSPortNode,NULL,BAD_CAST "PortSerailNo",BAD_CAST stringvalue );
                if ( strlen ( OVSBr[ovsloop].ovsportlist[portloop].addr ) ==0 ) {
                    strcpy ( OVSBr[ovsloop].ovsportlist[portloop].addr,"00:00:00:00:00:00\000" );
                }
                xmlNewChild ( OVSPortNode, NULL, BAD_CAST "MAC",BAD_CAST OVSBr[ovsloop].ovsportlist[portloop].addr );

                sprintf ( stringvalue,"%lld\000",OVSBr[ovsloop].ovsportlist[portloop].Txbytes );
                xmlNewChild ( OVSPortNode, NULL, BAD_CAST "Txbytes",BAD_CAST stringvalue );
                //  //printf("                the nic%d Txbytes is %d   \n",netdevloop,OVSBr[ovsloop].ovsportlist[portloop].Txbytes);

                sprintf ( stringvalue,"%lld\000",OVSBr[ovsloop].ovsportlist[portloop].Txpkts );
                xmlNewChild ( OVSPortNode, NULL, BAD_CAST "Txpackets",BAD_CAST stringvalue );
                //  //printf("                the nic%d Txpackets is %d   \n",netdevloop,OVSBr[ovsloop].ovsportlist[portloop].Txpackets);

                sprintf ( stringvalue,"%lld\000",OVSBr[ovsloop].ovsportlist[portloop].Txdrop );
                xmlNewChild ( OVSPortNode, NULL, BAD_CAST "Txdrop",BAD_CAST stringvalue );
                //  //printf("                the nic%d Txdrop is %d   \n",netdevloop,OVSBr[ovsloop].ovsportlist[portloop].Txdrop);

                sprintf ( stringvalue,"%lld\000",OVSBr[ovsloop].ovsportlist[portloop].Rxbytes );
                xmlNewChild ( OVSPortNode, NULL, BAD_CAST "Rxbytes",BAD_CAST stringvalue );
                //  //printf("                the nic%d Rxbytes is %d   \n",netdevloop,OVSBr[ovsloop].ovsportlist[portloop].Rxbytes);

                sprintf ( stringvalue,"%lld\000",OVSBr[ovsloop].ovsportlist[portloop].Rxpkts );
                xmlNewChild ( OVSPortNode, NULL, BAD_CAST "Rxpackets",BAD_CAST stringvalue );
                //  //printf("                the nic%d Rxpackets is %d   \n",netdevloop,OVSBr[ovsloop].ovsportlist[portloop].Rxpackets);

                sprintf ( stringvalue,"%lld\000",OVSBr[ovsloop].ovsportlist[portloop].Rxdrop );
                xmlNewChild ( OVSPortNode, NULL, BAD_CAST "Rxdrop",BAD_CAST stringvalue );
                //  //printf("                the nic%d Rxdrop is %d   \n",netdevloop,OVSBr[ovsloop].ovsportlist[portloop].Rxdrop);
            }
        }
    }
//    xmlDocDumpFormatMemory(state,&xmlbuff,&buffersize,1);
//    //printf("%s",(char*)xmlbuff);
//    xmlFree(xmlbuff);
//printf ( "########################the QEMU infomation##################\n" );
    vminfo VMInfo[5]= {0};
    int vminfoloop=0;
    int vminfonum;
    vminfonum=get_QemuInfo ( VMInfo );
    if ( vminfonum>0 ) {
        xmlNodePtr VMInfoNode,VMInfoDetailNode,VMNetDevInfoNode;
        VMInfoNode = xmlNewChild ( root, NULL, BAD_CAST "VMInfo", NULL );
        for ( vminfoloop=0; vminfoloop<vminfonum; vminfoloop++ ) {
            VMInfoDetailNode=xmlNewChild ( VMInfoNode, NULL, BAD_CAST "VMInfoDetail", NULL );
            xmlNewChild ( VMInfoDetailNode, NULL, BAD_CAST "VMName",BAD_CAST VMInfo[vminfoloop].VMname );
            xmlNewChild ( VMInfoDetailNode, NULL, BAD_CAST "VMUUID",BAD_CAST VMInfo[vminfoloop].VMuuid );
            xmlNewChild ( VMInfoDetailNode, NULL, BAD_CAST "VMVNC",BAD_CAST VMInfo[vminfoloop].VMVNC );
            sprintf ( stringvalue,"%ld\000",VMInfo[vminfoloop].VMMemInfo );
            xmlNewChild ( VMInfoDetailNode, NULL, BAD_CAST "VMMemInfo",BAD_CAST stringvalue );

            sprintf ( stringvalue,"%ld\000",VMInfo[vminfoloop].VMCPUCores );
            xmlNewChild ( VMInfoDetailNode, NULL, BAD_CAST "VMCPUCores",BAD_CAST stringvalue );

            sprintf ( stringvalue,"%d\000",VMInfo[vminfoloop].VMpid );
            xmlNewChild ( VMInfoDetailNode, NULL, BAD_CAST "VMPID",BAD_CAST stringvalue );

            int vmnetdevloop=0;
            for ( vmnetdevloop=0; strlen ( VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevId ) !=0; vmnetdevloop++ ) {
                VMNetDevInfoNode=xmlNewChild ( VMInfoDetailNode, NULL, BAD_CAST "VMNetDevInfo", NULL );
                xmlNewChild ( VMNetDevInfoNode, NULL, BAD_CAST "NetDevIfname", BAD_CAST VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevIfname );
                xmlNewChild ( VMNetDevInfoNode, NULL, BAD_CAST "NetDevType", BAD_CAST VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevType );
                xmlNewChild ( VMNetDevInfoNode, NULL, BAD_CAST "NetDevID", BAD_CAST VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevId );
                xmlNewChild ( VMNetDevInfoNode, NULL, BAD_CAST "NetDevDeviceType", BAD_CAST VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevdevicetype );
                xmlNewChild ( VMNetDevInfoNode, NULL, BAD_CAST "NetDevDeviceMAC", BAD_CAST VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevdevicemac );
                
                                //printf ( "           ##############the vmnetdev id is %s ###################    \n",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevId );
                                //printf ( "                         the netdevIfname  is %s                        \n",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevIfname );
                                //printf ( "                         the netdevType  is %s                        \n",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevType );
                                //printf ( "                         the netdevdevicetype  is %s                        \n",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevdevicetype );
                                //printf ( "                         the netdevdevicemac  is %s                        \n",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevdevicemac );
                                //printf ( "                         the netdevvhostfd  is %d                        \n",VMInfo[vminfoloop].VMNetdevInfo[vmnetdevloop].netdevvhostfd );
            }
        }
    }
//    xmlDocDumpFormatMemory(state,&xmlbuff,&buffersize,1);
//    //printf("%s",(char*)xmlbuff);
//    xmlFree(xmlbuff);
     //////////////////////////////////////////PID Info//////////////////////////////////////////
    xmlNodePtr PIDInfoNode;
    PIDInfoNode = xmlNewChild ( root,NULL,BAD_CAST "PIDInfo",NULL );
//printf ( "########################the QEMU PID infomation##################\n" );
    qemulwpinfo QemuInfo[5]= {0};
    int qemupidloop=0;
    int qemupidnum;
    qemupidnum= get_QemuPidInfo ( QemuInfo );
    if ( qemupidnum>0 ) {
        xmlNodePtr QemuPIDInfoNode,QemuPIDInfoDetailNode,LWPInfoNode;
        QemuPIDInfoNode=xmlNewChild ( PIDInfoNode,NULL,BAD_CAST "QemuPIDInfo",NULL );
        for ( qemupidloop=1; qemupidloop<qemupidnum+1; qemupidloop++ ) {
            QemuPIDInfoDetailNode=xmlNewChild ( QemuPIDInfoNode,NULL,BAD_CAST "QemuPIDInfoDetail",NULL );
            xmlNewChild ( QemuPIDInfoDetailNode,NULL,BAD_CAST "QemuProgressName",BAD_CAST QemuInfo[qemupidloop].progressName );
            ////printf ( "    ##############the %s QEMU detail ###################    \n",QemuInfo[qemupidloop].progressName );
            int qemulwploop=0;
            for ( qemulwploop=0;  QemuInfo[qemupidloop].LWPInfo[qemulwploop].pid!=0; qemulwploop++ ) {
                LWPInfoNode=xmlNewChild ( QemuPIDInfoDetailNode,NULL,BAD_CAST "LWPInfo",NULL );
                sprintf ( stringvalue,"%d\000",QemuInfo[qemupidloop].LWPInfo[qemulwploop].pid );
                xmlNewChild ( LWPInfoNode, NULL, BAD_CAST "PID",BAD_CAST stringvalue );

                sprintf ( stringvalue,"%d\000",QemuInfo[qemupidloop].LWPInfo[qemulwploop].lwp );
                xmlNewChild ( LWPInfoNode, NULL, BAD_CAST "LWP",BAD_CAST stringvalue );

                sprintf ( stringvalue,"%d\000",QemuInfo[qemupidloop].LWPInfo[qemulwploop].psr );
                xmlNewChild ( LWPInfoNode, NULL, BAD_CAST "PSR",BAD_CAST stringvalue );
                // //printf ( "                  ###pid is %d lwp is %d prs is %d  ###              \n",QemuInfo[qemupidloop].LWPInfo[qemulwploop].pid,QemuInfo[qemupidloop].LWPInfo[qemulwploop].lwp,QemuInfo[qemupidloop].LWPInfo[qemulwploop].psr );
            }
        }
    }

	free(CPUInfo);
	free(MemInfo);
	free(OVSDBLwpInfo);
	free(VswitchdLwpInfo);
	free(DPDKLwpInfo);
	return state;
}
