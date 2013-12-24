﻿
#include "common.h"
#ifdef _WIN32
	#include "EpollFake.h"
#endif
#include "configuration.h"
#include "logger.h"
#include "network.h"
#include "basic_class.h" 
#include "pk_mgr.h"
#include "peer_mgr.h"
#include "peer.h"
//#include "librtmp/rtmp_server.h"
//#include "librtmp/rtmp.h"
//#include "librtmp/rtmp_supplement.h"
#include "bit_stream_server.h"
#include "peer_communication.h"
#include "io_connect.h"
#include "io_accept.h"
#include "logger_client.h"
#include "stunt_mgr.h"



using namespace std;

#ifdef _FIRE_BREATH_MOD_
#include "JSObject.h"
#include "variant_list.h"
#include "DOM/Document.h"
#include "global/config.h"
//#include "llp2pAPI.h"
#include "firebreathTestAPI.h"
#endif



const char version[] = "1.0.0.0";


#ifdef _FIRE_BREATH_MOD_

typedef struct
{
    volatile sig_atomic_t handle_sig_alarm;

	volatile sig_atomic_t srv_shutdown;

	int errorRestartFlag;
	//static logger_client *logger_client_ptr = NULL;
	volatile unsigned short streamingPort;
	list<int> *fd_list;
	map<string, string> *map_config2;
	pk_mgr *pk_mgr_ptr_copy;
	volatile sig_atomic_t is_Pk_mgr_ptr_copy_delete;
	volatile sig_atomic_t http_srv_ready;
	volatile sig_atomic_t threadCtl;    // 0 can open thread  ,1 cant open
	int a;
	int b;
	string c;
} GlobalVars;

map<string, string> map_config;
map<int, GlobalVars*> map_chid_globalVar;
void launchThread(void * arg);
int mainFunction(int chid);
string ssss;
int set_config_done = 0;

#else

//struct globalVar{
static volatile sig_atomic_t handle_sig_alarm = 0;
static volatile sig_atomic_t srv_shutdown = 0;
static int errorRestartFlag = 0;
//static logger_client *logger_client_ptr = NULL;
static volatile unsigned short streamingPort = 0;
list<int> fd_list;

#endif
//};


#ifdef _FIRE_BREATH_MOD_
#else

void signal_handler(int sig)
{
	fprintf(stdout, "\n\nrecv Signal %d\n\n", sig);

	//if(logger_client_ptr == NULL){
	//	logger_client_ptr = new logger_client();
	//	logger_client_ptr->log_init();
	//}

	switch (sig)
	{
		case SIGTERM: 
			srv_shutdown = 1; 
			//logger_client_ptr->log_exit();
			break;
		case SIGINT:
			srv_shutdown = 1;
			//errorRestartFlag = 1;
			//logger_client_ptr->log_exit();
			break;
#ifndef _WIN32
		case SIGALRM: 
			handle_sig_alarm = 1; 
			//logger_client_ptr->log_exit();
			break;
#endif
		case SIGSEGV:
			srv_shutdown = 1;
			//logger_client_ptr->log_exit();
#ifdef _WIN32
			MessageBox(NULL, _T("SIGSEGV"), _T("EXIT"), MB_OK | MB_ICONSTOP);
#else
			printf("SIGSEGV\n");
			PAUSE
			exit(0);		// it must exit on linux			
#endif
			break;
		case SIGABRT:
			srv_shutdown = 1;
			//logger_client_ptr->log_exit();
#ifdef _WIN32
			//MessageBox(NULL, _T("SIGABRT"), _T("EXIT"), MB_OK | MB_ICONSTOP);
#else
			printf("SIGABRT\n");
			PAUSE
			exit(0);		// it must exit on linux
#endif			
			break;	
	}

}

#endif


#ifdef _FIRE_BREATH_MOD_
///////////////////////////////////////////////////////////////////////////////
/// @fn FB::variant llp2pAPI::echo(const FB::variant& msg)
///
/// @brief  Echos whatever is passed from Javascript.
///         Go ahead and change it. See what happens!
///////////////////////////////////////////////////////////////////////////////
FB::variant firebreathTestAPI::echo(const FB::variant& msg)
{
    static int n(0);
    fire_echo("So far, you clicked this many times: ", n++);
	fire_echo("tess ", 3051);
    // return "foobar";
    return "123";
}

FB::variant firebreathTestAPI::echo2(const FB::variant& msg)
{
    static int n(0);
    fire_echo("So far, you clicked this many times: ", n++);
	fire_echo("tets ", 3051);
    // return "foobar";
    return "2";
} 

FB::variant firebreathTestAPI::echo100(const FB::variant& msg)
{
    static int n(0);
    fire_echo("So far, you clicked this many times: ", n++);
	fire_echo("test ", 3051);
    // return "foobar";
    return "10001";
}


///////////////////////////////////////////////////////////////////////////////
/// @fn llp2pPtr llp2pAPI::getPlugin()
///
/// @brief  Gets a reference to the plugin that was passed in when the object
///         was created.  If the plugin has already been released then this
///         will throw a FB::script_error that will be translated into a
///         javascript exception in the page.
///////////////////////////////////////////////////////////////////////////////
firebreathTestPtr firebreathTestAPI::getPlugin()
{
    firebreathTestPtr plugin(m_plugin.lock());
    if (!plugin) {
        throw FB::script_error("The plugin is invalid");
    }
    return plugin;
}

// Read/Write property testString
std::string firebreathTestAPI::get_testString()
{
    return m_testString;
}

void firebreathTestAPI::set_testString(const std::string& val)
{
    m_testString = val;
}

// Read-only property version
std::string firebreathTestAPI::get_version()
{
    return FBSTRING_PLUGIN_VERSION;
}

void firebreathTestAPI::testEvent()
{
    fire_test();
}

int firebreathTestAPI::start(int chid)
{
	if(map_chid_globalVar.count(chid) > 0)
	{
		map_chid_globalVar[chid]->threadCtl++;
		return 3;
	}
	else
	{
		GlobalVars *temp = (GlobalVars*) malloc (sizeof(GlobalVars));
		temp->fd_list = new list<int>;
		temp->handle_sig_alarm = 0;
		temp->srv_shutdown = 0;
		temp->errorRestartFlag = 0;
		temp->streamingPort = -1;
		temp->pk_mgr_ptr_copy = NULL;
		temp->is_Pk_mgr_ptr_copy_delete = TRUE;
		temp->http_srv_ready = 0;
		temp->threadCtl = 1;
		
		temp->map_config2 = new map<string, string>;
		map<string, string>::iterator iter;
		for (iter = map_config.begin(); iter != map_config.end(); iter++) {
			temp->map_config2->insert(pair<string, string>(iter->first, iter->second));
		}
		
		
		map_chid_globalVar[chid] = temp;
		_beginthread(launchThread, 0, (void*)chid);	
		return 2;
	}
	return TRUE;
}

void firebreathTestAPI::stop(int chid)
{
	if(chid < 1 || map_chid_globalVar.count(chid) <= 0)
	{
		return;
	}
	map_chid_globalVar[chid]->threadCtl--;
	if(map_chid_globalVar[chid]->threadCtl <= 0)
	{
		map_chid_globalVar[chid]->srv_shutdown = 1;
	}
}

int firebreathTestAPI::isReady(int chid)
{
	map<int, GlobalVars*>::iterator iter;
	iter = map_chid_globalVar.find(chid);
	if (iter != map_chid_globalVar.end()) {
		return map_chid_globalVar[chid]->http_srv_ready;
	}
	else {
		return -1;
	}
}

int firebreathTestAPI::isStreamInChannel(int streamID, int chid)
{
	if(map_chid_globalVar[chid]->is_Pk_mgr_ptr_copy_delete ==FALSE){
		map<int, struct update_stream_header *>::iterator  map_streamID_header_iter;
		map_streamID_header_iter = map_chid_globalVar[chid]->pk_mgr_ptr_copy ->map_streamID_header.find(streamID);
		//		return TRUE;
		if(map_streamID_header_iter != map_chid_globalVar[chid]->pk_mgr_ptr_copy ->map_streamID_header.end()){
			return TRUE;
		}else{
			return FALSE ;
		}
	}else{
		return FALSE;
	}

	return TRUE;
}

unsigned short firebreathTestAPI::streamingPortIs(int chid)
{
	return map_chid_globalVar[chid]->streamingPort;
	/*
	if (map_chid_globalVar[chid]->is_Pk_mgr_ptr_copy_delete) {
		return 0;
	}
	else {
		return map_chid_globalVar[chid]->streamingPort;
	}
	*/
}

// Set up configuration and store in map_config
void firebreathTestAPI::set_config(const std::string& msg)
{
	string ss(msg.begin()+1, msg.end()-1);
	std::string delimiter = ",";

	size_t pos = 0;
	string token;
	map<string, string> m;
	while ((pos = ss.find(delimiter)) != std::string::npos) {
		token = ss.substr(0, pos);
		
		int n = token.find(":");
		string key(token.begin()+1, token.begin()+n-1);
		string value(token.begin()+n+2, token.end()-1);
		map_config[key] = value;
		//map_config.insert(pair<string, string>(key, value));
		ssss += key + ":" + value + "\n";
		ss.erase(0, pos + delimiter.length());
	}
	token = ss.substr(0, pos);
		
	int n = token.find(":");
	string key(token.begin()+1, token.begin()+n-1);
	string value(token.begin()+n+2, token.end()-1);
	map_config[key] = value;
	//map_config.insert(pair<string, string>(key, value));
	ssss += key + ":" + value + "\n";
	
	/*
	string str = map_config.find("channel_id")->second;
	istringstream is(str);
	int chid;
	is >> chid;
	
	ssss += str + "\n";
	map<string, string>::iterator iter;
	
	
	for (iter = map_config.begin(); iter != map_config.end(); iter++) {
		
		ssss += iter->first + ":" + iter->second + "\n";
		map_chid_globalVar[chid]->map_config2.insert(pair<string, string>(iter->first, iter->second));
		//map_chid_globalVar[chid]->map_config2[iter->first] = iter->second;
	}
	*/

	set_config_done = 1;
	return ;
}

std::string firebreathTestAPI::get_config()
{
	map<string, string>::iterator iter;
	string ret_msg("");
	for (iter = map_config.begin(); iter != map_config.end(); iter++) {
		ret_msg += iter->first + ":" + iter->second + "\n";
	}
	//ssss = ret_msg;
    return ret_msg;
}

int firebreathTestAPI::is_set_config_done()
{
    return set_config_done;
}
/*
int firebreathTestAPI::valid()
{
	return TRUE;
}
*/

void launchThread(void * arg)
{
	int a = (int)arg;
	mainFunction(a);
	//return;
}

#endif


#ifdef _FIRE_BREATH_MOD_
int mainFunction(int chid){
#else
int main(int argc, char **argv){
#endif

		

	while (!srv_shutdown) {

	
	
		int svc_fd_tcp;		// listening-socket for peers 
		int	svc_fd_udp;
		unsigned long html_size;
		int optval = 1;
		struct sockaddr_in sin;

		string svc_tcp_port("");
		string svc_udp_port("");
		string stream_local_port("");
		string config_file("config.ini");

		configuration *prep = NULL;
		network *net_ptr = NULL;
		logger *log_ptr = NULL;					// record log on local file
		logger_client *logger_client_ptr = NULL;	// send log Info to server
		peer_mgr *peer_mgr_ptr = NULL;
		stunt_mgr *stunt_mgr_ptr = NULL;
		pk_mgr *pk_mgr_ptr = NULL;
		//rtmp_server *rtmp_svr = NULL;
		//rtmp *rtmp_ptr = NULL;
		//amf *amf_ptr = NULL; 
		//rtmp_supplement *rtmp_supplement_ptr = NULL;
		bit_stream_server *bit_stream_server_ptr =NULL;
		peer_communication *peer_communication_ptr = NULL;

		// Create constructors
		//prep = new configuration(config_file);
		
#ifdef _FIRE_BREATH_MOD_
		prep = new configuration(config_file);
		for (map<string, string>::iterator iter = map_config.begin(); iter != map_config.end(); iter++) {
			//prep->map_table.insert(pair<string, string>(iter->first, iter->second));
			prep->map_table[iter->first] = iter->second;
		}
		net_ptr = new network(&(map_chid_globalVar[chid]->errorRestartFlag), map_chid_globalVar[chid]->fd_list);
#else
		prep = new configuration(config_file);
		if (prep == NULL) {
			printf("[ERROR] prep new error \n");
			PAUSE
		}
		net_ptr = new network(&errorRestartFlag, &fd_list);
		if (net_ptr == NULL) {
			printf("[ERROR] net_ptr new error \n");
			PAUSE
		}
#endif
		log_ptr = new logger();
		if (log_ptr == NULL) {
			printf("[ERROR] log_ptr new error \n");
			PAUSE
		}
		log_ptr->logger_set(net_ptr);
		logger_client_ptr = new logger_client(log_ptr);
		if (logger_client_ptr == NULL) {
			printf("[ERROR] logger_client_ptr new error \n");
			PAUSE
		}
#ifdef _FIRE_BREATH_MOD_
		peer_mgr_ptr = new peer_mgr(map_chid_globalVar[chid]->fd_list);
		stunt_mgr_ptr = new stunt_mgr(map_chid_globalVar[chid]->fd_list);
#else
		
		peer_mgr_ptr = new peer_mgr(&fd_list);
		if (peer_mgr_ptr == NULL) {
			printf("[ERROR] peer_mgr_ptr new error \n");
			PAUSE
		}
		//stunt_mgr_ptr = new stunt_mgr(&fd_list);	// Release版本會有問題
		//if (stunt_mgr_ptr == NULL) {
		//	printf("[ERROR] stunt_mgr_ptr new error \n");
		//	PAUSE
		//}
		
#endif

		
#ifdef _FIRE_BREATH_MOD_
		char s[64];
		sprintf(s,"%d",chid);
		//prep->add_key("channel_id", s);
#endif
		
		{
		int svc_fd_tcp2;
		struct sockaddr_in sin2;
		svc_fd_tcp2 = socket(AF_INET, SOCK_STREAM, 0);
			
		
		memset(&sin2, 0, sizeof(struct sockaddr_in));
		sin2.sin_family = AF_INET;
		sin2.sin_addr.s_addr = INADDR_ANY;
		
		sin2.sin_port = htons(11102);
		::bind(svc_fd_tcp2, (struct sockaddr *)&sin2, sizeof(struct sockaddr_in));
		listen(svc_fd_tcp2, MAX_POLL_EVENT);
		}
		
		log_ptr->start_log_record(SYS_FREQ);
		
		prep->read_key("html_size", html_size);
		prep->read_key("svc_tcp_port", svc_tcp_port);
		prep->read_key("svc_udp_port", svc_udp_port);
		prep->read_key("stream_local_port", stream_local_port);

		cout << "html_size=" << html_size << endl;
		cout << "svc_tcp_port=" << svc_tcp_port << endl;
		cout << "svc_udp_port=" << svc_udp_port << endl;
		cout << "stream_local_port=" << stream_local_port << endl;
		
#ifdef _FIRE_BREATH_MOD_
		pk_mgr_ptr = new pk_mgr(html_size, map_chid_globalVar[chid]->fd_list, net_ptr , log_ptr , prep , logger_client_ptr, stunt_mgr_ptr);
#else
		pk_mgr_ptr = new pk_mgr(html_size, &fd_list, net_ptr , log_ptr , prep , logger_client_ptr, stunt_mgr_ptr);
#endif
		if (!pk_mgr_ptr) {
			printf("pk_mgr_ptr error !!!!!!!!!!!!!!!\n");
		}

		net_ptr->epoll_creater();
		//log_ptr->start_log_record(SYS_FREQ);

		peer_mgr_ptr->peer_mgr_set(net_ptr, log_ptr, prep, pk_mgr_ptr, logger_client_ptr);
		//peer_mgr_ptr->pk_mgr_set(pk_mgr_ptr);
		pk_mgr_ptr->peer_mgr_set(peer_mgr_ptr);

		peer_communication_ptr = new peer_communication(net_ptr,log_ptr,prep,peer_mgr_ptr,peer_mgr_ptr->get_peer_object(),pk_mgr_ptr,logger_client_ptr);
		if (!peer_communication_ptr) {
			printf("peer_commuication_ptr error!!!!!!!!!!\n");
		}
		peer_mgr_ptr->peer_communication_set(peer_communication_ptr);

		logger_client_ptr->set_net_obj(net_ptr);
		logger_client_ptr->set_pk_mgr_obj(pk_mgr_ptr);
		logger_client_ptr->set_prep_obj(prep);

#ifndef _WIN32
		signal(SIGALRM, signal_handler);
		signal(SIGPIPE, SIG_IGN);		// avoid crash on socket pipe 
		signal(SIGUSR1, SIG_IGN);	
#endif

#ifdef _WIN32
		WSADATA wsaData;										// Winsock initial data
		if(WSAStartup(MAKEWORD(2, 2),&wsaData)) {
			printf("WSAStartup ERROR\n");
			WSACleanup();
			exit(0);
		}
#endif

#ifdef _FIRE_BREATH_MOD_
#else
		signal(SIGTERM, signal_handler);
		signal(SIGINT,  signal_handler);
		signal(SIGSEGV, signal_handler);
		signal(SIGABRT, signal_handler);
#endif

		if (MODE == MODE_RTMP) {
			/*
			printf("MODE_RTMP\n");
			amf_ptr = new amf(log_ptr);
			if (!amf_ptr) {
				printf("Can't new amf class\n");
				return -1;
			}

			rtmp_ptr = new rtmp(net_ptr, amf_ptr, log_ptr);
			if (!rtmp_ptr) {
				printf("Can't new rtmp class\n");
				return -1;
			}

			rtmp_supplement_ptr = new rtmp_supplement(log_ptr, rtmp_ptr, amf_ptr);
			if (!rtmp_supplement_ptr) {
				printf("Can't new rtmp_supplement class\n");
				return -1;
			}

			rtmp_svr = new rtmp_server(net_ptr, log_ptr, amf_ptr, rtmp_ptr, rtmp_supplement_ptr, pk_mgr_ptr, &fd_list);
			if (!rtmp_svr) {
				printf("Can't new rtsp_server class\n");
				return -1;
			}
			rtmp_svr->init(1,stream_local_port);
			printf("new rtmp_svr successfully\n");
			*/

		//MODE BitStream
		}
		else if (MODE == MODE_BitStream || MODE == MODE_HTTP) {
			debug_printf("MODE_BitStream \n");

#ifdef _FIRE_BREATH_MOD_
			bit_stream_server_ptr = new bit_stream_server(net_ptr,log_ptr, logger_client_ptr, pk_mgr_ptr, map_chid_globalVar[chid]->fd_list);
#else
			bit_stream_server_ptr = new bit_stream_server(net_ptr,log_ptr, logger_client_ptr, pk_mgr_ptr, &fd_list);
#endif
			if (!bit_stream_server_ptr) {
				debug_printf("[ERROR] bit_stream_server_ptr error \n");
			}
			
			stringstream ss_tmp;
			unsigned short port_tcp;
			ss_tmp << stream_local_port;
			ss_tmp >> port_tcp;
#ifdef _FIRE_BREATH_MOD_
			map_chid_globalVar[chid]->streamingPort = bit_stream_server_ptr->init(0, port_tcp);
			log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "Create bit_stream_server success. port =", map_chid_globalVar[chid]->streamingPort);
			debug_printf("Create bit_stream_server success. port = %u", map_chid_globalVar[chid]->streamingPort);
#else
			streamingPort = bit_stream_server_ptr->init(0, port_tcp);
			log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Create bit_stream_server success. port =", streamingPort);
			debug_printf("Create bit_stream_server success, port = %u \n", streamingPort);
#endif
		}
		else if (MODE == MODE_RTSP) {
			debug_printf("MODE_RTSP \n");
		}

		// Create TCP socket and UDP socket
		if ((svc_fd_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			log_ptr->write_log_format("s(u) s s d d \n", __FUNCTION__, __LINE__,"[ERROR] Create TCP socket failed", ". Socket error", WSAGetLastError(), svc_fd_tcp);
			debug_printf("[ERROR] Create TCP socket failed. socket error %d %d \n", WSAGetLastError(), svc_fd_tcp);
			PAUSE
		}
		if ((svc_fd_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			log_ptr->write_log_format("s(u) s s d d \n", __FUNCTION__, __LINE__,"[ERROR] Create UDP socket failed", ". Socket error", WSAGetLastError(), svc_fd_udp);
			debug_printf("[ERROR] Create UDP socket failed. socket error %d %d \n", WSAGetLastError(), svc_fd_udp);
			PAUSE
		}
		
		// Set socket SO_REUSEADDR
		/*
		if (setsockopt(svc_fd_tcp, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval , sizeof(optval)) != 0) {
			log_ptr->write_log_format("s(u) s s d \n", __FUNCTION__, __LINE__,"[ERROR] Set SO_REUSEADDR failed", ". Socket error", WSAGetLastError());
			debug_printf("[ERROR] Set SO_REUSEADDR failed. socket error %d \n", WSAGetLastError());
			PAUSE
		}
		*/
		
		memset(&sin, 0, sizeof(struct sockaddr_in));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		unsigned short ptop_port = (unsigned short)atoi(svc_tcp_port.c_str());		// listen-port for peer connection

		// Find an available port
		for ( ; ; ptop_port++) {
			sin.sin_port = htons(ptop_port);
			if (::bind(svc_fd_tcp, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
				log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Socket bind failed at port", ptop_port, ". Socket error", WSAGetLastError());
				debug_printf("Socket bind failed at port %d. Socket error %d \n", ptop_port, WSAGetLastError());
				continue;
			}
			if (listen(svc_fd_tcp, MAX_POLL_EVENT) < 0) {
				log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "Socket listen failed at port", ptop_port, ". Socket error", WSAGetLastError());
				debug_printf("Socket listen failed at port %d. Socket error %d \n", ptop_port, WSAGetLastError());
				continue;
			}
			break;
		}
		debug_printf("Create p2p listen-socket success, port = %u \n", ptop_port);
		log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Create p2p listen-socket success, port =", ptop_port);
		
		net_ptr->set_nonblocking(svc_fd_tcp);

		cout << "tst_speed_svr " << version << " (Compiled Time: "__DATE__ << " "__TIME__")" << endl;
		log_ptr->write_log_format("s(u) s (s)\n", __FUNCTION__, __LINE__, "PF", "ready now");

		if (!log_ptr->check_arch_compatible()) {
			cout << "Hardware Architecture is not support." << endl;
			PAUSE
			log_ptr->exit(0, "Hardware Architecture is not support.");
		}

	

#ifndef _WIN32
		struct itimerval interval;
		interval.it_interval.tv_sec = SIG_FREQ;
		interval.it_interval.tv_usec = 0;
		interval.it_value.tv_sec = SIG_FREQ;
		interval.it_value.tv_usec = 0;

		// setup periodic timer (3 second) 
		if (setitimer(ITIMER_REAL, &interval, NULL)) {
			log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"setitimer ERROR ");		
			return -1;
		}

		getitimer(ITIMER_REAL, &interval);
#endif

		net_ptr->set_nonblocking(svc_fd_tcp);

		net_ptr->epoll_control(svc_fd_tcp, EPOLL_CTL_ADD, EPOLLIN);

		net_ptr->set_fd_bcptr_map(svc_fd_tcp, dynamic_cast<basic_class *>(peer_communication_ptr->get_io_accept_handler()));

#ifdef _FIRE_BREATH_MOD_
		(map_chid_globalVar[chid]->fd_list)->push_back(svc_fd_tcp);
#else
		fd_list.push_back(svc_fd_tcp);
#endif
		pk_mgr_ptr->init(ptop_port);
		logger_client_ptr->log_init();


		/*int testServerfd =0;
		int errorcount =0;
		while(1){
			testServerfd = pk_mgr_ptr ->build_connection("127.0.0.1",stream_local_port);
			if(testServerfd){

				printf("connect ok \n");
				shutdown(testServerfd, SHUT_RDWR);
				closesocket(testServerfd);
				log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"connect ok");
				break ;
			}else{
				Sleep(100);
				errorcount++;
				if(errorcount++ >=50)
					break;
				printf("connectfail  errorcount=%d\n",errorcount);
				log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"connectfail  errorcount",errorcount);
			}
		}*/

#ifdef _FIRE_BREATH_MOD_
		map_chid_globalVar[chid]->pk_mgr_ptr_copy = pk_mgr_ptr ;
		map_chid_globalVar[chid]->http_srv_ready = 1;
		map_chid_globalVar[chid]->is_Pk_mgr_ptr_copy_delete = FALSE;
		
		map<string, string>::iterator iter;
		for (iter = map_config.begin(); iter != map_config.end(); iter++) {
			log_ptr->write_log_format("s(u) s s \n", __FUNCTION__, __LINE__, (iter->first).c_str(), (iter->second).c_str());
		}
		for (iter = prep->map_table.begin(); iter != prep->map_table.end(); iter++) {
			log_ptr->write_log_format("s(u) -- s s \n", __FUNCTION__, __LINE__, (iter->first).c_str(), (iter->second).c_str());
		}
	
		
		while((!(map_chid_globalVar[chid]->srv_shutdown))  &&  (!(map_chid_globalVar[chid]->errorRestartFlag))) {
#else
		while (!srv_shutdown && !errorRestartFlag) {
#endif

#ifdef _WIN32

	#ifdef _FIRE_BREATH_MOD_
			net_ptr->epoll_waiter(1000, map_chid_globalVar[chid]->fd_list);
	#else
			net_ptr->epoll_waiter(1000, &fd_list);
	#endif

#else
			net_ptr->epoll_waiter(1000);
#endif
			net_ptr->epoll_dispatcher();

			pk_mgr_ptr->time_handle();
			
			if (*(net_ptr->_errorRestartFlag) == RESTART) {
				log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);
				log_ptr->write_log_format("s(u) \n", __FUNCTION__, __LINE__);
				log_ptr->write_log_format("s(u) s \n\n\n", __FUNCTION__, __LINE__, "Program Restart");
				//Sleep(2000);
				break;
			}
		}

		debug_printf("Client Restart \n");
		PAUSE
		
#ifdef _FIRE_BREATH_MOD_
		log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__,"srv_shutdown", map_chid_globalVar[chid]->srv_shutdown, "errorRestartFlag", map_chid_globalVar[chid]->errorRestartFlag);
#else
		log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "srv_shutdown", srv_shutdown, "errorRestartFlag", errorRestartFlag);
#endif

		net_ptr->garbage_collection();
		log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "PF", "graceful exit!!");
		log_ptr->stop_log_record();

		if (prep) { 
			delete prep;
		}
		prep = NULL;

		if (bit_stream_server_ptr) {
			delete bit_stream_server_ptr;
		}
		bit_stream_server_ptr = NULL;

		if (peer_communication_ptr) {
			delete peer_communication_ptr;
		}
		peer_communication_ptr = NULL;

		if (net_ptr) {
			delete net_ptr;
		}
		net_ptr = NULL;	

#ifdef _FIRE_BREATH_MOD_
		map_chid_globalVar[chid]->is_Pk_mgr_ptr_copy_delete = TRUE;
		map_chid_globalVar[chid]->pk_mgr_ptr_copy =NULL;
		map_chid_globalVar[chid]->http_srv_ready = 0;
#endif
		if (pk_mgr_ptr) {
			delete pk_mgr_ptr;
		}
		pk_mgr_ptr = NULL;

//		if(rtmp_svr)
//			delete rtmp_svr;
//		rtmp_svr =NULL;

		if (peer_mgr_ptr) {
			delete peer_mgr_ptr;
		}
		peer_mgr_ptr = NULL;

		if (log_ptr) {
			delete log_ptr;
		}
		log_ptr = NULL;

		if (logger_client_ptr) {
			delete logger_client_ptr;
		}
		logger_client_ptr = NULL;

		for (int i = 0; i < 10; i++) {
			printf("Some Thing Error Wait %d Sec To Restart ............\n",10-i );
			Sleep(1000);
		}

#ifdef _FIRE_BREATH_MOD_
		map_chid_globalVar[chid]->errorRestartFlag =0;
#else
		errorRestartFlag =0;
#endif

	}

#ifdef _FIRE_BREATH_MOD_
	map_chid_globalVar[chid]->streamingPort = 0;
	map_chid_globalVar[chid]->handle_sig_alarm = 0;
	map_chid_globalVar[chid]->srv_shutdown = 0;
	map_chid_globalVar[chid]->errorRestartFlag = 0;
	delete map_chid_globalVar[chid]->fd_list;
	free(map_chid_globalVar[chid]);
	map_chid_globalVar.erase(chid);
#else
	streamingPort = 0;
	handle_sig_alarm = 0;
	srv_shutdown = 0;
	errorRestartFlag = 0;
#endif
	return EXIT_SUCCESS;
}





/**********************************************************\

  Auto-generated firebreathTestAPI.cpp

\**********************************************************/
/*
#include "JSObject.h"
#include "variant_list.h"
#include "DOM/Document.h"
#include "global/config.h"

#include "firebreathTestAPI.h"

#include <WinSock2.h>

///////////////////////////////////////////////////////////////////////////////
/// @fn FB::variant firebreathTestAPI::echo(const FB::variant& msg)
///
/// @brief  Echos whatever is passed from Javascript.
///         Go ahead and change it. See what happens!
///////////////////////////////////////////////////////////////////////////////
FB::variant firebreathTestAPI::echo(const FB::variant& msg)
{
    static int n(0);
    fire_echo("So far, you clicked this many times::: ", n++);
	fire_echo("haha", 55);

	int svc_fd_tcp;
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(3500);
	
	WSADATA wsaData;										// Winsock initial data
	WSAStartup(MAKEWORD(2, 2),&wsaData);
	svc_fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	bind(svc_fd_tcp, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
	listen(svc_fd_tcp, 5);
				
	printf("11 \n");

    // return "foobar";
    return msg;
}
FB::variant firebreathTestAPI::echo2(const FB::variant& msg)
{
	int nn = 0;
    fire_echo("echo2 test ", nn);

    // return "foobar";
    return msg;
}

///////////////////////////////////////////////////////////////////////////////
/// @fn firebreathTestPtr firebreathTestAPI::getPlugin()
///
/// @brief  Gets a reference to the plugin that was passed in when the object
///         was created.  If the plugin has already been released then this
///         will throw a FB::script_error that will be translated into a
///         javascript exception in the page.
///////////////////////////////////////////////////////////////////////////////
firebreathTestPtr firebreathTestAPI::getPlugin()
{
    firebreathTestPtr plugin(m_plugin.lock());
    if (!plugin) {
        throw FB::script_error("The plugin is invalid");
    }
    return plugin;
}

// Read/Write property testString
std::string firebreathTestAPI::get_testString()
{
    return m_testString;
}

void firebreathTestAPI::set_testString(const std::string& val)
{
    m_testString = val;
}

// Read-only property version
std::string firebreathTestAPI::get_version()
{
    return FBSTRING_PLUGIN_VERSION;
}

void firebreathTestAPI::testEvent()
{
    fire_test();
}

void firebreathTestAPI::testEvent2()
{
    fire_test2();
}
*/