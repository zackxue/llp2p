#include "logger_client.h"

logger_client::logger_client(){
	self_pid = 0;
	previous_time_differ = 0;
	buffer_clear_flag = 0;
	start_delay = 0;
	sub_stream_number = 0;
	in_recv_len = 0;
	out_send_len = 0;
	log_bw_in_init_flag = 0;
	log_bw_out_init_flag = 0;
	log_source_delay_init_flag = 0;
	self_channel_id = 0;
	chunk_buffer =NULL;
	max_source_delay =NULL;
	delay_list=NULL;
	chunk_buffer = (struct chunk_t *)new unsigned char[(CHUNK_BUFFER_SIZE + sizeof(struct chunk_header_t))];
	quality_struct_ptr =NULL ;

	quality_struct_ptr = (struct quality_struct*)(new struct quality_struct);
	memset(quality_struct_ptr,0x00,sizeof(struct quality_struct));
	memset(chunk_buffer,0x00,(CHUNK_BUFFER_SIZE + sizeof(struct chunk_header_t)));
	memset(&non_log_recv_struct,0x00,sizeof(Nonblocking_Buff));

	memset(&start_out_bw_record,0x00,sizeof(start_out_bw_record));
	memset(&end_out_bw_record,0x00,sizeof(end_out_bw_record));
	memset(&start_in_bw_record,0x00,sizeof(struct log_in_bw_struct));
	memset(&end_in_bw_record,0x00,sizeof(struct log_in_bw_struct));
	memset(&log_period_bw_start,0x00,sizeof(log_period_bw_start));
	memset(&log_period_source_delay_start,0x00,sizeof(log_period_source_delay_start));
	log_timer_init();
}

logger_client::~logger_client(){

	if(chunk_buffer)
		delete chunk_buffer;
	if(max_source_delay)
		delete max_source_delay;
	if(delay_list)
		delete delay_list;

}

void logger_client::set_self_pid_channel(unsigned long pid,unsigned long channel_id){
	self_pid = pid;
	self_channel_id = channel_id;
	log_to_server(LOG_BEGINE,0);
	log_clear_buffer();
}

void logger_client::set_net_obj(network *net_ptr){
	_net_ptr = net_ptr;
}

void logger_client::set_pk_mgr_obj(pk_mgr *pk_mgr_ptr){
	_pk_mgr_ptr = pk_mgr_ptr;
}

void logger_client::log_init(){
	buffer_size = 0;

	if((log_server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		cout << "log_init init create socket failure" << endl;
#ifdef _WIN32
		::WSACleanup();
#endif
		exit(1);
	}

	struct sockaddr_in log_saddr;

	memset((struct sockaddr_in*)&log_saddr, 0x0, sizeof(struct sockaddr_in));

	log_saddr.sin_addr.s_addr = inet_addr("140.114.90.146");
	log_saddr.sin_port = htons(8754);
	log_saddr.sin_family = AF_INET;

//	_net_ptr->set_nonblocking(_sock);

	if(connect(log_server_sock, (struct sockaddr*)&log_saddr, sizeof(log_saddr)) < 0) {

#ifdef _WIN32
//win32
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
//linux
#endif
//		if non-blocking mode waht can i do?
		}else{
			cout << "build_connection failure" << endl;

#ifdef _WIN32
			::closesocket(log_server_sock);
			::WSACleanup();
#else
			::close(_sock);
#endif
			exit(1);

		}
	}

	_net_ptr->set_nonblocking(log_server_sock);	// set to non-blocking
	_net_ptr->epoll_control(log_server_sock, EPOLL_CTL_ADD, EPOLLOUT);
	_net_ptr->set_fd_bcptr_map(log_server_sock, dynamic_cast<basic_class *> (this));
	_pk_mgr_ptr->fd_list_ptr->push_back(log_server_sock);

}


#ifdef WIN32
void logger_client::logger_client_getTickTime(LARGE_INTEGER *tickTime)
{
	QueryPerformanceCounter(tickTime);
}
#endif


#ifdef WIN32
unsigned int logger_client::logger_client_diffTime_ms(LARGE_INTEGER startTime,LARGE_INTEGER endTime)
{
    LARGE_INTEGER CUPfreq;
    LONGLONG llLastTime;
    QueryPerformanceFrequency(&CUPfreq);
    llLastTime = (unsigned int)  (1000 * (endTime.QuadPart - startTime.QuadPart) / CUPfreq.QuadPart);

    return llLastTime;
}
#endif

void logger_client::log_timer_init(){
	logger_client_getTickTime(&log_start_time);
}

void logger_client::log_time_differ(){
	logger_client_getTickTime(&log_record_time);
	log_time_dffer = logger_client_diffTime_ms(log_start_time,log_record_time);
}

void logger_client::count_start_delay(){
	logger_client_getTickTime(&log_record_time);
	start_delay = logger_client_diffTime_ms(log_start_time,log_record_time);

	log_to_server(LOG_START_DELAY,0,start_delay);
}
	
void logger_client::source_delay_struct_init(unsigned long sub_stream_num){
	log_source_delay_init_flag = 1;
	sub_stream_number = sub_stream_num;
	max_source_delay = (struct log_source_delay_struct *)new unsigned char[(sub_stream_num*sizeof(struct log_source_delay_struct))]; 
	memset(max_source_delay,0x00,(sub_stream_num*sizeof(struct log_source_delay_struct)));
	delay_list = (unsigned long *)new unsigned char[(sub_stream_num*sizeof(unsigned long))];
	memset(delay_list,0x00,(sub_stream_num*sizeof(unsigned long)));

	logger_client_getTickTime(&log_period_source_delay_start);
}

void logger_client::set_source_delay(unsigned long sub_id,unsigned long source_delay){
	if((source_delay != 0)&&((max_source_delay + sub_id)->average_delay != 0)){
		(max_source_delay + sub_id)->delay_now = source_delay;
		(max_source_delay + sub_id)->average_delay = ((max_source_delay + sub_id)->average_delay + source_delay) / 2 ;
	}
	else if((source_delay != 0)&&((max_source_delay + sub_id)->average_delay == 0)){
		(max_source_delay + sub_id)->delay_now = source_delay;
		(max_source_delay + sub_id)->average_delay = source_delay ;
	}
	else{
		(max_source_delay + sub_id)->delay_now = 0;
		(max_source_delay + sub_id)->average_delay = 0 ;
	}
}

void logger_client::send_max_source_delay(){
	unsigned long max_delay = 0;
	
	for(int i = 0;i<sub_stream_number;i++){
		if(((max_source_delay + i)->average_delay) != 0){
			*(delay_list + i) = (max_source_delay + i)->average_delay;
		}
		else{
			*(delay_list + i) = (max_source_delay + i)->delay_now;
		}
	}

	for(int i = 0;i<sub_stream_number;i++){
		if(((max_source_delay + i)->average_delay) > max_delay){
			max_delay = ((max_source_delay + i)->average_delay);
		}
	}

	log_to_server(LOG_PERIOD_SOURCE_DELAY,0,max_delay,sub_stream_number,delay_list);

	for(int i = 0;i<sub_stream_number;i++){
		(max_source_delay + i)->average_delay = (max_source_delay + i)->delay_now;
	}
}

void logger_client::bw_in_struct_init(unsigned long timestamp,unsigned long pkt_size){
	in_recv_len = 0;
	pre_in_pkt_size = 0;

	start_in_bw_record.time_stamp = timestamp;
	logger_client_getTickTime(&(start_in_bw_record.client_time));
	logger_client_getTickTime(&log_period_bw_start);
	in_recv_len += pkt_size;
	pre_in_pkt_size = pkt_size;
}

void logger_client::bw_out_struct_init(unsigned long pkt_size){
	pre_out_pkt_size = 0;
	out_send_len = 0;

	logger_client_getTickTime(&start_out_bw_record);
	out_send_len += pkt_size;
	pre_out_pkt_size = pkt_size;
}

void logger_client::set_out_bw(unsigned long pkt_size){
	logger_client_getTickTime(&end_out_bw_record);
	out_send_len += pkt_size;
	pre_out_pkt_size = pkt_size;
}

void logger_client::set_in_bw(unsigned long timestamp,unsigned long pkt_size){
	end_in_bw_record.time_stamp = timestamp;
	logger_client_getTickTime(&(end_in_bw_record.client_time));
	in_recv_len += pkt_size;
	pre_in_pkt_size = pkt_size;
}

void logger_client::send_bw(){
	/*
	this part is used for in bw
	*/
	unsigned long should_in_bw = 0;
	unsigned long real_in_bw = 0;
	unsigned long period_msec = 0;
	unsigned long real_out_bw = 0;
	double quality_result = 0.0;

	if((start_in_bw_record.time_stamp!=0)&&(end_in_bw_record.time_stamp!=0)){
		period_msec = logger_client_diffTime_ms(start_in_bw_record.client_time,end_in_bw_record.client_time);
		if(period_msec != 0){
			real_in_bw = (1000 * in_recv_len) / period_msec;
		}
		else{
			real_in_bw = 0;
		}
		period_msec = (end_in_bw_record.time_stamp - start_in_bw_record.time_stamp);
		if(period_msec != 0){
			should_in_bw = (1000 * in_recv_len) / period_msec;
		}
		else{
			should_in_bw = 0;
		}
	}
	else{
		//send null
		real_in_bw = 0;
		should_in_bw = 0;
	}

	if(out_send_len != pre_out_pkt_size){
		period_msec = logger_client_diffTime_ms(start_out_bw_record,end_out_bw_record);
		if(period_msec == 0){
			real_out_bw = 0;
		}
		else{
			real_out_bw = (1000 * out_send_len) / period_msec;
		}
	}
	else{
		real_out_bw = 0;
	}

	quality_result = (double)(0.5)*( quality_struct_ptr->quality_count /(double)(quality_struct_ptr ->total_chunk)) +  (double)(0.5)*( (double)(quality_struct_ptr ->total_chunk/((double)quality_struct_ptr->loss_pkt +(double)(quality_struct_ptr ->total_chunk))));
	printf("%u %f %u\n",quality_struct_ptr ->total_chunk,quality_struct_ptr->quality_count,quality_struct_ptr->loss_pkt);
	quality_struct_ptr->loss_pkt = 0;
	quality_struct_ptr->quality_count = 0.0;
	quality_struct_ptr->total_chunk = 0;

	log_to_server(LOG_CLIENT_BW,0,should_in_bw,real_in_bw,real_out_bw,quality_result);

	in_recv_len = pre_in_pkt_size;
	start_in_bw_record.time_stamp = end_in_bw_record.time_stamp;
	start_in_bw_record.client_time = end_in_bw_record.client_time;
	end_in_bw_record.time_stamp = 0;

	out_send_len = pre_out_pkt_size;
	start_out_bw_record = end_out_bw_record;

}

void logger_client::log_to_server(int log_mode, ...){
	va_list ap;
	int d;
	unsigned int u;
	char *s;
	unsigned long long int llu;

	log_time_differ();
	va_start(ap,log_mode);

	if(log_mode == LOG_REGISTER){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_register_struct *log_register_struct_ptr = NULL;
		log_register_struct_ptr = new struct log_register_struct;
		memset(log_register_struct_ptr,0x00,sizeof(struct log_register_struct));

		log_register_struct_ptr->log_header.cmd = LOG_REGISTER;
		log_register_struct_ptr->log_header.log_time = log_time_dffer;
		log_register_struct_ptr->log_header.pid = self_pid;
		log_register_struct_ptr->log_header.manifest = manifest;
		log_register_struct_ptr->log_header.channel_id = self_channel_id;
		log_register_struct_ptr->log_header.length = sizeof(struct log_register_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_register_struct_ptr);
		buffer_size += sizeof(struct log_register_struct);
	}
	else if(log_mode == LOG_REG_LIST){
		unsigned long manifest,list_num,connect_num;
		unsigned long *list_ptr, *connect_list_ptr;
		unsigned int pkt_size = 0;
		unsigned int offset = 0, array_size;
		struct log_pkt_format_struct *log_pkt = NULL;
		struct log_list_struct *log_list_struct_ptr = NULL;

		manifest = va_arg(ap, unsigned long);
		list_num = va_arg(ap, unsigned long);
		connect_num = va_arg(ap, unsigned long);
		list_ptr = va_arg(ap, unsigned long *);
		connect_list_ptr = va_arg(ap, unsigned long *);

		pkt_size = sizeof(struct log_header_t) + (2 * sizeof(unsigned long)) + (list_num * sizeof(unsigned long)) + (connect_num * sizeof(unsigned long));
		log_pkt = (struct log_pkt_format_struct *)new unsigned char[pkt_size];
		log_list_struct_ptr = (struct log_list_struct*)log_pkt;

		memset(log_pkt,0x00,pkt_size);

		log_list_struct_ptr->log_header.cmd = LOG_REG_LIST;
		log_list_struct_ptr->log_header.length = pkt_size - sizeof(struct log_header_t);
		log_list_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_struct_ptr->log_header.pid = self_pid;
		log_list_struct_ptr->log_header.manifest = manifest; 
		log_list_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_struct_ptr->list_num = list_num;
		log_list_struct_ptr->connect_num = connect_num;

		offset += sizeof(struct log_header_t) + (2 * sizeof(unsigned long));

		if(list_num != 0){
			array_size = list_num * sizeof(unsigned long);

			memcpy((char *)log_pkt + offset,list_ptr,array_size);
			offset += array_size;
		}

		if(connect_num != 0){
			array_size = connect_num * sizeof(unsigned long);

			memcpy((char *)log_pkt + offset,connect_list_ptr,array_size);
			offset += array_size;
		}
		log_buffer.push((struct log_pkt_format_struct *)log_pkt);
		buffer_size += pkt_size;

	}
	else if(log_mode == LOG_REG_LIST_TESTING){
		unsigned long manifest,select_pid;
		manifest = va_arg(ap, unsigned long);
		select_pid = va_arg(ap, unsigned long);

		struct log_list_testing_struct *log_list_testing_struct_ptr = NULL;
		log_list_testing_struct_ptr = new struct log_list_testing_struct;
		memset(log_list_testing_struct_ptr,0x00,sizeof(struct log_list_testing_struct));

		log_list_testing_struct_ptr->log_header.cmd = LOG_REG_LIST_TESTING;
		log_list_testing_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_testing_struct_ptr->log_header.pid = self_pid;
		log_list_testing_struct_ptr->log_header.manifest = manifest;
		log_list_testing_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_testing_struct_ptr->log_header.length = sizeof(struct log_list_testing_struct) - sizeof(struct log_header_t);
		log_list_testing_struct_ptr->select_pid = select_pid;

		log_buffer.push((struct log_pkt_format_struct *)log_list_testing_struct_ptr);
		buffer_size += sizeof(struct log_list_testing_struct);
	}
	else if(log_mode == LOG_REG_LIST_DETECTION_TESTING_SUCCESS){
		unsigned long manifest,testing_result;
		manifest = va_arg(ap, unsigned long);
		testing_result = va_arg(ap, unsigned long);

		struct log_list_detection_testing_struct *log_list_detection_testing_struct_ptr = NULL;
		log_list_detection_testing_struct_ptr = new struct log_list_detection_testing_struct;
		memset(log_list_detection_testing_struct_ptr,0x00,sizeof(struct log_list_detection_testing_struct));

		log_list_detection_testing_struct_ptr->log_header.cmd = LOG_REG_LIST_DETECTION_TESTING_SUCCESS;
		log_list_detection_testing_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_detection_testing_struct_ptr->log_header.pid = self_pid;
		log_list_detection_testing_struct_ptr->log_header.manifest = manifest;
		log_list_detection_testing_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_detection_testing_struct_ptr->log_header.length = sizeof(struct log_list_detection_testing_struct) - sizeof(struct log_header_t);
		log_list_detection_testing_struct_ptr->testing_result = testing_result;

		log_buffer.push((struct log_pkt_format_struct *)log_list_detection_testing_struct_ptr);
		buffer_size += sizeof(struct log_list_detection_testing_struct);
	}
	else if(log_mode == LOG_REG_LIST_TESTING_FAIL){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_list_testing_fail_struct *log_list_testing_fail_struct_ptr = NULL;
		log_list_testing_fail_struct_ptr = new struct log_list_testing_fail_struct;
		memset(log_list_testing_fail_struct_ptr,0x00,sizeof(struct log_list_testing_fail_struct));

		log_list_testing_fail_struct_ptr->log_header.cmd = LOG_REG_LIST_TESTING_FAIL;
		log_list_testing_fail_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_testing_fail_struct_ptr->log_header.pid = self_pid;
		log_list_testing_fail_struct_ptr->log_header.manifest = manifest;
		log_list_testing_fail_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_testing_fail_struct_ptr->log_header.length = sizeof(struct log_list_testing_fail_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_list_testing_fail_struct_ptr);
		buffer_size += sizeof(struct log_list_testing_fail_struct);
	}
	else if(log_mode == LOG_REG_CUT_PK){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_cut_pk_struct *log_cut_pk_struct_ptr = NULL;
		log_cut_pk_struct_ptr = new struct log_cut_pk_struct;
		memset(log_cut_pk_struct_ptr,0x00,sizeof(struct log_cut_pk_struct));

		log_cut_pk_struct_ptr->log_header.cmd = LOG_REG_CUT_PK;
		log_cut_pk_struct_ptr->log_header.log_time = log_time_dffer;
		log_cut_pk_struct_ptr->log_header.pid = self_pid;
		log_cut_pk_struct_ptr->log_header.manifest = manifest;
		log_cut_pk_struct_ptr->log_header.channel_id = self_channel_id;
		log_cut_pk_struct_ptr->log_header.length = sizeof(struct log_cut_pk_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_cut_pk_struct_ptr);
		buffer_size += sizeof(struct log_cut_pk_struct);
	}
	else if(log_mode == LOG_REG_DATA_COME){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_data_come_struct *log_data_come_struct_ptr = NULL;
		log_data_come_struct_ptr = new struct log_data_come_struct;
		memset(log_data_come_struct_ptr,0x00,sizeof(struct log_data_come_struct));

		log_data_come_struct_ptr->log_header.cmd = LOG_REG_DATA_COME;
		log_data_come_struct_ptr->log_header.log_time = log_time_dffer;
		log_data_come_struct_ptr->log_header.pid = self_pid;
		log_data_come_struct_ptr->log_header.manifest = manifest;
		log_data_come_struct_ptr->log_header.channel_id = self_channel_id;
		log_data_come_struct_ptr->log_header.length = sizeof(struct log_data_come_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_data_come_struct_ptr);
		buffer_size += sizeof(struct log_data_come_struct);
	}
	else if(log_mode == LOG_RESCUE_TRIGGER){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_rescue_trigger_struct *log_rescue_trigger_struct_ptr = NULL;
		log_rescue_trigger_struct_ptr = new struct log_rescue_trigger_struct;
		memset(log_rescue_trigger_struct_ptr,0x00,sizeof(struct log_rescue_trigger_struct));

		log_rescue_trigger_struct_ptr->log_header.cmd = LOG_RESCUE_TRIGGER;
		log_rescue_trigger_struct_ptr->log_header.log_time = log_time_dffer;
		log_rescue_trigger_struct_ptr->log_header.pid = self_pid;
		log_rescue_trigger_struct_ptr->log_header.manifest = manifest;
		log_rescue_trigger_struct_ptr->log_header.channel_id = self_channel_id;
		log_rescue_trigger_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_rescue_trigger_struct_ptr);
		buffer_size += sizeof(struct log_rescue_trigger_struct);
	}
	else if(log_mode == LOG_DELAY_RESCUE_TRIGGER){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_rescue_trigger_struct *log_rescue_trigger_struct_ptr = NULL;
		log_rescue_trigger_struct_ptr = new struct log_rescue_trigger_struct;
		memset(log_rescue_trigger_struct_ptr,0x00,sizeof(struct log_rescue_trigger_struct));

		log_rescue_trigger_struct_ptr->log_header.cmd = LOG_DELAY_RESCUE_TRIGGER;
		log_rescue_trigger_struct_ptr->log_header.log_time = log_time_dffer;
		log_rescue_trigger_struct_ptr->log_header.pid = self_pid;
		log_rescue_trigger_struct_ptr->log_header.manifest = manifest;
		log_rescue_trigger_struct_ptr->log_header.channel_id = self_channel_id;
		log_rescue_trigger_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_rescue_trigger_struct_ptr);
		buffer_size += sizeof(struct log_rescue_trigger_struct);
	}
	else if(log_mode == LOG_MERGE_TRIGGER){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_rescue_trigger_struct *log_rescue_trigger_struct_ptr = NULL;
		log_rescue_trigger_struct_ptr = new struct log_rescue_trigger_struct;
		memset(log_rescue_trigger_struct_ptr,0x00,sizeof(struct log_rescue_trigger_struct));

		log_rescue_trigger_struct_ptr->log_header.cmd = LOG_MERGE_TRIGGER;
		log_rescue_trigger_struct_ptr->log_header.log_time = log_time_dffer;
		log_rescue_trigger_struct_ptr->log_header.pid = self_pid;
		log_rescue_trigger_struct_ptr->log_header.manifest = manifest;
		log_rescue_trigger_struct_ptr->log_header.channel_id = self_channel_id;
		log_rescue_trigger_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_rescue_trigger_struct_ptr);
		buffer_size += sizeof(struct log_rescue_trigger_struct);
	}
	else if(log_mode == LOG_RESCUE_LIST){
		unsigned long manifest,list_num,connect_num;
		unsigned long *list_ptr, *connect_list_ptr;
		unsigned int pkt_size = 0;
		unsigned int offset = 0, array_size;
		struct log_pkt_format_struct *log_pkt = NULL;
		struct log_list_struct *log_list_struct_ptr = NULL;

		manifest = va_arg(ap, unsigned long);
		list_num = va_arg(ap, unsigned long);
		connect_num = va_arg(ap, unsigned long);
		list_ptr = va_arg(ap, unsigned long *);
		connect_list_ptr = va_arg(ap, unsigned long *);

		pkt_size = sizeof(struct log_header_t) + (2 * sizeof(unsigned long)) + (list_num * sizeof(unsigned long)) + (connect_num * sizeof(unsigned long));
		log_pkt = (struct log_pkt_format_struct *)new unsigned char[pkt_size];
		log_list_struct_ptr = (struct log_list_struct*)log_pkt;

		memset(log_pkt,0x00,pkt_size);

		log_list_struct_ptr->log_header.cmd = LOG_RESCUE_LIST;
		log_list_struct_ptr->log_header.length = pkt_size - sizeof(struct log_header_t);
		log_list_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_struct_ptr->log_header.pid = self_pid;
		log_list_struct_ptr->log_header.manifest = manifest; 
		log_list_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_struct_ptr->list_num = list_num;
		log_list_struct_ptr->connect_num = connect_num;

		offset += sizeof(struct log_header_t) + (2 * sizeof(unsigned long));

		if(list_num != 0){
			array_size = list_num * sizeof(unsigned long);

			memcpy((char *)log_pkt + offset,list_ptr,array_size);
			offset += array_size;
		}

		if(connect_num != 0){
			array_size = connect_num * sizeof(unsigned long);

			memcpy((char *)log_pkt + offset,connect_list_ptr,array_size);
			offset += array_size;
		}
		log_buffer.push((struct log_pkt_format_struct *)log_pkt);
		buffer_size += pkt_size;

	}
	else if(log_mode == LOG_RESCUE_TESTING){
		unsigned long manifest,select_pid;
		manifest = va_arg(ap, unsigned long);
		select_pid = va_arg(ap, unsigned long);

		struct log_list_testing_struct *log_list_testing_struct_ptr = NULL;
		log_list_testing_struct_ptr = new struct log_list_testing_struct;
		memset(log_list_testing_struct_ptr,0x00,sizeof(struct log_list_testing_struct));

		log_list_testing_struct_ptr->log_header.cmd = LOG_RESCUE_TESTING;
		log_list_testing_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_testing_struct_ptr->log_header.pid = self_pid;
		log_list_testing_struct_ptr->log_header.manifest = manifest;
		log_list_testing_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_testing_struct_ptr->log_header.length = sizeof(struct log_list_testing_struct) - sizeof(struct log_header_t);
		log_list_testing_struct_ptr->select_pid = select_pid;

		log_buffer.push((struct log_pkt_format_struct *)log_list_testing_struct_ptr);
		buffer_size += sizeof(struct log_list_testing_struct);
	}
	else if(log_mode == LOG_RESCUE_DETECTION_TESTING_SUCCESS){
		unsigned long manifest,testing_result;
		manifest = va_arg(ap, unsigned long);
		testing_result = va_arg(ap, unsigned long);

		struct log_list_detection_testing_struct *log_list_detection_testing_struct_ptr = NULL;
		log_list_detection_testing_struct_ptr = new struct log_list_detection_testing_struct;
		memset(log_list_detection_testing_struct_ptr,0x00,sizeof(struct log_list_detection_testing_struct));

		log_list_detection_testing_struct_ptr->log_header.cmd = LOG_RESCUE_DETECTION_TESTING_SUCCESS;
		log_list_detection_testing_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_detection_testing_struct_ptr->log_header.pid = self_pid;
		log_list_detection_testing_struct_ptr->log_header.manifest = manifest;
		log_list_detection_testing_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_detection_testing_struct_ptr->log_header.length = sizeof(struct log_list_detection_testing_struct) - sizeof(struct log_header_t);
		log_list_detection_testing_struct_ptr->testing_result = testing_result;

		log_buffer.push((struct log_pkt_format_struct *)log_list_detection_testing_struct_ptr);
		buffer_size += sizeof(struct log_list_detection_testing_struct);
	}
	else if(log_mode == LOG_RESCUE_LIST_TESTING_FAIL){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_list_testing_fail_struct *log_list_testing_fail_struct_ptr = NULL;
		log_list_testing_fail_struct_ptr = new struct log_list_testing_fail_struct;
		memset(log_list_testing_fail_struct_ptr,0x00,sizeof(struct log_list_testing_fail_struct));

		log_list_testing_fail_struct_ptr->log_header.cmd = LOG_RESCUE_LIST_TESTING_FAIL;
		log_list_testing_fail_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_testing_fail_struct_ptr->log_header.pid = self_pid;
		log_list_testing_fail_struct_ptr->log_header.manifest = manifest;
		log_list_testing_fail_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_testing_fail_struct_ptr->log_header.length = sizeof(struct log_list_testing_fail_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_list_testing_fail_struct_ptr);
		buffer_size += sizeof(struct log_list_testing_fail_struct);
	}
	else if(log_mode == LOG_RESCUE_CUT_PK){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_cut_pk_struct *log_cut_pk_struct_ptr = NULL;
		log_cut_pk_struct_ptr = new struct log_cut_pk_struct;
		memset(log_cut_pk_struct_ptr,0x00,sizeof(struct log_cut_pk_struct));

		log_cut_pk_struct_ptr->log_header.cmd = LOG_RESCUE_CUT_PK;
		log_cut_pk_struct_ptr->log_header.log_time = log_time_dffer;
		log_cut_pk_struct_ptr->log_header.pid = self_pid;
		log_cut_pk_struct_ptr->log_header.manifest = manifest;
		log_cut_pk_struct_ptr->log_header.channel_id = self_channel_id;
		log_cut_pk_struct_ptr->log_header.length = sizeof(struct log_cut_pk_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_cut_pk_struct_ptr);
		buffer_size += sizeof(struct log_cut_pk_struct);
	}
	else if(log_mode == LOG_RESCUE_DATA_COME){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_data_come_struct *log_data_come_struct_ptr = NULL;
		log_data_come_struct_ptr = new struct log_data_come_struct;
		memset(log_data_come_struct_ptr,0x00,sizeof(struct log_data_come_struct));

		log_data_come_struct_ptr->log_header.cmd = LOG_RESCUE_DATA_COME;
		log_data_come_struct_ptr->log_header.log_time = log_time_dffer;
		log_data_come_struct_ptr->log_header.pid = self_pid;
		log_data_come_struct_ptr->log_header.manifest = manifest;
		log_data_come_struct_ptr->log_header.channel_id = self_channel_id;
		log_data_come_struct_ptr->log_header.length = sizeof(struct log_data_come_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_data_come_struct_ptr);
		buffer_size += sizeof(struct log_data_come_struct);
	}
	else if(log_mode == LOG_START_DELAY){
		unsigned long manifest,start_delay;
		manifest = va_arg(ap, unsigned long);
		start_delay = va_arg(ap, unsigned long);

		struct log_start_delay_struct *log_start_delay_struct_ptr = NULL;
		log_start_delay_struct_ptr = new struct log_start_delay_struct;
		memset(log_start_delay_struct_ptr,0x00,sizeof(struct log_start_delay_struct));

		log_start_delay_struct_ptr->log_header.cmd = LOG_START_DELAY;
		log_start_delay_struct_ptr->log_header.log_time = log_time_dffer;
		log_start_delay_struct_ptr->log_header.pid = self_pid;
		log_start_delay_struct_ptr->log_header.manifest = manifest;
		log_start_delay_struct_ptr->log_header.channel_id = self_channel_id;
		log_start_delay_struct_ptr->log_header.length = sizeof(struct log_start_delay_struct) - sizeof(struct log_header_t);
		log_start_delay_struct_ptr->start_delay = start_delay;

		log_buffer.push((struct log_pkt_format_struct *)log_start_delay_struct_ptr);
		buffer_size += sizeof(struct log_start_delay_struct);
	}
	else if(log_mode == LOG_PERIOD_SOURCE_DELAY){
		unsigned long manifest,max_delay,sub_number;
		unsigned long *delay_list = NULL;
		unsigned int pkt_size = 0;
		unsigned int offset = 0;

		manifest = va_arg(ap, unsigned long);
		max_delay = va_arg(ap, unsigned long);
		sub_number = va_arg(ap, unsigned long);
		delay_list = va_arg(ap, unsigned long*);

		pkt_size = sizeof(struct log_header_t) + (2 * sizeof(unsigned long)) + (sub_number * sizeof(unsigned long));
		struct log_pkt_format_struct *log_pkt_format_struct_ptr = NULL;
		struct log_period_source_delay_struct *log_period_source_delay_struct_ptr = NULL;
		log_pkt_format_struct_ptr = (struct log_pkt_format_struct*)new unsigned char[pkt_size];
		memset(log_pkt_format_struct_ptr,0x00,pkt_size);
		log_period_source_delay_struct_ptr = (struct log_period_source_delay_struct *)log_pkt_format_struct_ptr;


		log_period_source_delay_struct_ptr->log_header.cmd = LOG_PERIOD_SOURCE_DELAY;
		log_period_source_delay_struct_ptr->log_header.log_time = log_time_dffer;
		log_period_source_delay_struct_ptr->log_header.pid = self_pid;
		log_period_source_delay_struct_ptr->log_header.manifest = manifest;
		log_period_source_delay_struct_ptr->log_header.channel_id = self_channel_id;
		log_period_source_delay_struct_ptr->log_header.length = pkt_size - sizeof(struct log_header_t);
		log_period_source_delay_struct_ptr->max_delay = max_delay;
		log_period_source_delay_struct_ptr->sub_num = sub_number;

		offset = sizeof(struct log_header_t) + (2 * sizeof(unsigned long));

		memcpy((char *)log_pkt_format_struct_ptr + offset,delay_list,(sub_number * sizeof(unsigned long)));

		log_buffer.push((struct log_pkt_format_struct *)log_pkt_format_struct_ptr);
		buffer_size += pkt_size;
	}
	else if(log_mode == LOG_RESCUE_SUB_STREAM){
		unsigned long manifest,rescue_num;
		manifest = va_arg(ap, unsigned long);
		rescue_num = va_arg(ap, unsigned long);

		struct log_rescue_sub_stream_struct *log_rescue_sub_stream_struct_ptr = NULL;
		log_rescue_sub_stream_struct_ptr = new struct log_rescue_sub_stream_struct;
		memset(log_rescue_sub_stream_struct_ptr,0x00,sizeof(struct log_rescue_sub_stream_struct));

		log_rescue_sub_stream_struct_ptr->log_header.cmd = LOG_RESCUE_SUB_STREAM;
		log_rescue_sub_stream_struct_ptr->log_header.log_time = log_time_dffer;
		log_rescue_sub_stream_struct_ptr->log_header.pid = self_pid;
		log_rescue_sub_stream_struct_ptr->log_header.manifest = manifest;
		log_rescue_sub_stream_struct_ptr->log_header.channel_id = self_channel_id;
		log_rescue_sub_stream_struct_ptr->log_header.length = sizeof(struct log_rescue_sub_stream_struct) - sizeof(struct log_header_t);
		log_rescue_sub_stream_struct_ptr->rescue_num = rescue_num;

		log_buffer.push((struct log_pkt_format_struct *)log_rescue_sub_stream_struct_ptr);
		buffer_size += sizeof(struct log_rescue_sub_stream_struct);
	}
	else if(log_mode == LOG_PEER_LEAVE){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_peer_leave_struct *log_peer_leave_struct_ptr = NULL;
		log_peer_leave_struct_ptr = new struct log_peer_leave_struct;
		memset(log_peer_leave_struct_ptr,0x00,sizeof(struct log_peer_leave_struct));

		log_peer_leave_struct_ptr->log_header.cmd = LOG_PEER_LEAVE;
		log_peer_leave_struct_ptr->log_header.log_time = log_time_dffer;
		log_peer_leave_struct_ptr->log_header.pid = self_pid;
		log_peer_leave_struct_ptr->log_header.manifest = manifest;
		log_peer_leave_struct_ptr->log_header.channel_id = self_channel_id;
		log_peer_leave_struct_ptr->log_header.length = sizeof(struct log_peer_leave_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_peer_leave_struct_ptr);
		buffer_size += sizeof(struct log_peer_leave_struct);
	}
	else if(log_mode == LOG_WRITE_STRING){
		unsigned long manifest;
		int str_buffer_size = 300;
		int int_array_size = 12;
		unsigned char *str_buffer = new unsigned char[str_buffer_size];
		char *inttostr = new char[int_array_size];	//base 4 btes, but it will be increase if not enough (in sprintf)
		const char *fmt = NULL;
		unsigned int str_buffer_offset = 0;
		int d;
		unsigned int u;
		char *s;
		int pkt_size = 0;
		int int_size = 0;

		manifest = va_arg(ap, unsigned long);
		memset(str_buffer,0x00,str_buffer_size);

		for(fmt = va_arg(ap, const char*);*fmt;fmt++){
			switch(*fmt) {
				case 's':           /* string */
					s = va_arg(ap, char *);
					if((str_buffer_offset + strlen(s))>=str_buffer_size){
						break;
					}
					memcpy((char*)str_buffer + str_buffer_offset,s,strlen(s));
					str_buffer_offset += strlen(s);
					break;
				case 'd':           /* int */
					int_size = 0;
					d = va_arg(ap, int);
					memset(inttostr,0x00,int_array_size);
					int_size = _snprintf(inttostr,int_array_size,"%d",d);
					if((str_buffer_offset + int_size)>=str_buffer_size){
						break;
					}
					memcpy((char*)str_buffer + str_buffer_offset,inttostr,int_size);
					str_buffer_offset += int_size;
					break;
				case 'u':           /* unsigned int */
					int_size = 0;
					u = va_arg(ap, unsigned int);
					memset(inttostr,0x00,int_array_size);
					int_size = _snprintf(inttostr,int_array_size,"%u",u);
					if((str_buffer_offset + int_size)>=str_buffer_size){
						break;
					}
					memcpy((char*)str_buffer + str_buffer_offset,inttostr,int_size);
					str_buffer_offset += int_size;
					break;
				default:
					if((str_buffer_offset+1)>=str_buffer_size){
						
					}
					else{
						memcpy((char*)str_buffer + str_buffer_offset,fmt,1);
						str_buffer_offset += 1;
					}
			}
		}

		pkt_size += (sizeof(struct log_header_t) + str_buffer_offset);
		struct log_write_string_struct *log_write_string_struct_ptr = (struct log_write_string_struct *)new unsigned char[pkt_size];
		
		memset(log_write_string_struct_ptr,0x00,pkt_size);

		log_write_string_struct_ptr->log_header.cmd = LOG_WRITE_STRING;
		log_write_string_struct_ptr->log_header.log_time = log_time_dffer;
		log_write_string_struct_ptr->log_header.pid = self_pid;
		log_write_string_struct_ptr->log_header.manifest = manifest;
		log_write_string_struct_ptr->log_header.channel_id = self_channel_id;
		log_write_string_struct_ptr->log_header.length = str_buffer_offset;
		
		memcpy(log_write_string_struct_ptr->buf,str_buffer,str_buffer_offset);

		log_buffer.push((struct log_pkt_format_struct *)log_write_string_struct_ptr);
		buffer_size += pkt_size;
	}
	else if(log_mode == LOG_BEGINE){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_begine_struct *log_begine_struct_ptr = NULL;
		log_begine_struct_ptr = new struct log_begine_struct;
		memset(log_begine_struct_ptr,0x00,sizeof(struct log_begine_struct));

		log_begine_struct_ptr->log_header.cmd = LOG_BEGINE;
		log_begine_struct_ptr->log_header.log_time = log_time_dffer;
		log_begine_struct_ptr->log_header.pid = self_pid;
		log_begine_struct_ptr->log_header.manifest = manifest;
		log_begine_struct_ptr->log_header.channel_id = self_channel_id;
		log_begine_struct_ptr->log_header.length = sizeof(struct log_begine_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_begine_struct_ptr);
		buffer_size += sizeof(struct log_begine_struct);
	}
	else if(log_mode == LOG_RESCUE_TRIGGER_BACK){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_rescue_trigger_back_struct *log_rescue_trigger_back_struct_ptr = NULL;
		log_rescue_trigger_back_struct_ptr = new struct log_rescue_trigger_back_struct;
		memset(log_rescue_trigger_back_struct_ptr,0x00,sizeof(struct log_rescue_trigger_back_struct));

		log_rescue_trigger_back_struct_ptr->log_header.cmd = LOG_RESCUE_TRIGGER_BACK;
		log_rescue_trigger_back_struct_ptr->log_header.log_time = log_time_dffer;
		log_rescue_trigger_back_struct_ptr->log_header.pid = self_pid;
		log_rescue_trigger_back_struct_ptr->log_header.manifest = manifest;
		log_rescue_trigger_back_struct_ptr->log_header.channel_id = self_channel_id;
		log_rescue_trigger_back_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_back_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_rescue_trigger_back_struct_ptr);
		buffer_size += sizeof(struct log_rescue_trigger_back_struct);
	}
	else if(log_mode == LOG_LIST_EMPTY){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_list_empty_struct *log_list_empty_struct_ptr = NULL;
		log_list_empty_struct_ptr = new struct log_list_empty_struct;
		memset(log_list_empty_struct_ptr,0x00,sizeof(struct log_list_empty_struct));

		log_list_empty_struct_ptr->log_header.cmd = LOG_LIST_EMPTY;
		log_list_empty_struct_ptr->log_header.log_time = log_time_dffer;
		log_list_empty_struct_ptr->log_header.pid = self_pid;
		log_list_empty_struct_ptr->log_header.manifest = manifest;
		log_list_empty_struct_ptr->log_header.channel_id = self_channel_id;
		log_list_empty_struct_ptr->log_header.length = sizeof(struct log_list_empty_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_list_empty_struct_ptr);
		buffer_size += sizeof(struct log_list_empty_struct);
	}
	else if(log_mode == LOG_TEST_DELAY_FAIL){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_test_delay_fail_struct *log_test_delay_fail_struct_ptr = NULL;
		log_test_delay_fail_struct_ptr = new struct log_test_delay_fail_struct;
		memset(log_test_delay_fail_struct_ptr,0x00,sizeof(struct log_test_delay_fail_struct));

		log_test_delay_fail_struct_ptr->log_header.cmd = LOG_TEST_DELAY_FAIL;
		log_test_delay_fail_struct_ptr->log_header.log_time = log_time_dffer;
		log_test_delay_fail_struct_ptr->log_header.pid = self_pid;
		log_test_delay_fail_struct_ptr->log_header.manifest = manifest;
		log_test_delay_fail_struct_ptr->log_header.channel_id = self_channel_id;
		log_test_delay_fail_struct_ptr->log_header.length = sizeof(struct log_test_delay_fail_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_test_delay_fail_struct_ptr);
		buffer_size += sizeof(struct log_test_delay_fail_struct);
	}
	else if(log_mode == LOG_TEST_DETECTION_FAIL){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_test_detection_fail_struct *log_test_detection_fail_struct_ptr = NULL;
		log_test_detection_fail_struct_ptr = new struct log_test_detection_fail_struct;
		memset(log_test_detection_fail_struct_ptr,0x00,sizeof(struct log_test_detection_fail_struct));

		log_test_detection_fail_struct_ptr->log_header.cmd = LOG_TEST_DETECTION_FAIL;
		log_test_detection_fail_struct_ptr->log_header.log_time = log_time_dffer;
		log_test_detection_fail_struct_ptr->log_header.pid = self_pid;
		log_test_detection_fail_struct_ptr->log_header.manifest = manifest;
		log_test_detection_fail_struct_ptr->log_header.channel_id = self_channel_id;
		log_test_detection_fail_struct_ptr->log_header.length = sizeof(struct log_test_detection_fail_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_test_detection_fail_struct_ptr);
		buffer_size += sizeof(struct log_test_detection_fail_struct);
	}
	else if(log_mode == LOG_DATA_COME_PK){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_data_come_pk_struct *log_data_come_pk_struct_ptr = NULL;
		log_data_come_pk_struct_ptr = new struct log_data_come_pk_struct;
		memset(log_data_come_pk_struct_ptr,0x00,sizeof(struct log_data_come_pk_struct));

		log_data_come_pk_struct_ptr->log_header.cmd = LOG_DATA_COME_PK;
		log_data_come_pk_struct_ptr->log_header.log_time = log_time_dffer;
		log_data_come_pk_struct_ptr->log_header.pid = self_pid;
		log_data_come_pk_struct_ptr->log_header.manifest = manifest;
		log_data_come_pk_struct_ptr->log_header.channel_id = self_channel_id;
		log_data_come_pk_struct_ptr->log_header.length = sizeof(struct log_data_come_pk_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_data_come_pk_struct_ptr);
		buffer_size += sizeof(struct log_data_come_pk_struct);
	}
	else if(log_mode == LOG_CLIENT_BW){
		unsigned long manifest;
		unsigned long temp_should_in_bw;
		unsigned long temp_real_in_bw;
		unsigned long temp_real_out_bw;
		double temp_quality;

		manifest = va_arg(ap, unsigned long);
		temp_should_in_bw = va_arg(ap, unsigned long);
		temp_real_in_bw = va_arg(ap, unsigned long);
		temp_real_out_bw = va_arg(ap, unsigned long);
		temp_quality = va_arg(ap, double);

		struct log_client_bw_struct *log_client_bw_struct_ptr = NULL;
		log_client_bw_struct_ptr = new struct log_client_bw_struct;
		memset(log_client_bw_struct_ptr,0x00,sizeof(struct log_client_bw_struct));

		log_client_bw_struct_ptr->log_header.cmd = LOG_CLIENT_BW;
		log_client_bw_struct_ptr->log_header.log_time = log_time_dffer;
		log_client_bw_struct_ptr->log_header.pid = self_pid;
		log_client_bw_struct_ptr->log_header.manifest = manifest;
		log_client_bw_struct_ptr->log_header.channel_id = self_channel_id;
		log_client_bw_struct_ptr->log_header.length = sizeof(struct log_client_bw_struct) - sizeof(struct log_header_t);
		log_client_bw_struct_ptr->should_in_bw = temp_should_in_bw;
		log_client_bw_struct_ptr->real_in_bw = temp_real_in_bw;
		log_client_bw_struct_ptr->real_out_bw = temp_real_out_bw;
		log_client_bw_struct_ptr->quality = temp_quality;

		log_buffer.push((struct log_pkt_format_struct *)log_client_bw_struct_ptr);
		buffer_size += sizeof(struct log_client_bw_struct);
	}
	else if(log_mode == LOG_TIME_OUT){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_time_out_struct *log_time_out_struct_ptr = NULL;
		log_time_out_struct_ptr = new struct log_time_out_struct;
		memset(log_time_out_struct_ptr,0x00,sizeof(struct log_time_out_struct));

		log_time_out_struct_ptr->log_header.cmd = LOG_TIME_OUT;
		log_time_out_struct_ptr->log_header.log_time = log_time_dffer;
		log_time_out_struct_ptr->log_header.pid = self_pid;
		log_time_out_struct_ptr->log_header.manifest = manifest;
		log_time_out_struct_ptr->log_header.channel_id = self_channel_id;
		log_time_out_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_time_out_struct_ptr);
		buffer_size += sizeof(struct log_time_out_struct);
	}
	else if(log_mode == LOG_PKT_LOSE){
		unsigned long manifest;
		manifest = va_arg(ap, unsigned long);

		struct log_pkt_lose_struct *log_pkt_lose_struct_ptr = NULL;
		log_pkt_lose_struct_ptr = new struct log_pkt_lose_struct;
		memset(log_pkt_lose_struct_ptr,0x00,sizeof(struct log_pkt_lose_struct));

		log_pkt_lose_struct_ptr->log_header.cmd = LOG_PKT_LOSE;
		log_pkt_lose_struct_ptr->log_header.log_time = log_time_dffer;
		log_pkt_lose_struct_ptr->log_header.pid = self_pid;
		log_pkt_lose_struct_ptr->log_header.manifest = manifest;
		log_pkt_lose_struct_ptr->log_header.channel_id = self_channel_id;
		log_pkt_lose_struct_ptr->log_header.length = sizeof(struct log_rescue_trigger_struct) - sizeof(struct log_header_t);

		log_buffer.push((struct log_pkt_format_struct *)log_pkt_lose_struct_ptr);
		buffer_size += sizeof(struct log_pkt_lose_struct);
	}
	else{
		
		log_to_server(LOG_WRITE_STRING,0,"s \n","unknown state in log\n");
		log_exit();
	}
}

void logger_client::log_clear_buffer(){
	buffer_clear_flag = 1;
}

void logger_client::log_exit(){
	Nonblocking_Ctl * Nonblocking_Send_Ctrl_ptr = NULL;
	struct log_pkt_format_struct *log_buffer_element_ptr = NULL;
	Nonblocking_Send_Ctrl_ptr = &(non_log_recv_struct.nonBlockingSendCtrl);
	int chunk_buffer_offset = 0;
	int log_struct_size = 0;
	int _send_byte;

	//blocking send
	while(Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING ){
		_send_byte = _net_ptr->nonblock_send(log_server_sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
			
		if(_send_byte < 0) {
				printf("(RUNNING) send info to log server error : %d in log_exit\n",WSAGetLastError());
				exit(1);
		}
	}

	log_to_server(LOG_PEER_LEAVE,0);

	if((buffer_size!=0)){
		while(buffer_size!=0){
			log_buffer_element_ptr = log_buffer.front();

			log_struct_size = log_buffer_element_ptr->log_header.length + sizeof(struct log_header_t);
			buffer_size -= log_struct_size;

			if((buffer_size<0)||((chunk_buffer_offset+log_struct_size) > CHUNK_BUFFER_SIZE)){
				printf("error : buffer size : %d and %d (overflow)  in log_exit\n",buffer_size,(chunk_buffer_offset+log_struct_size));
				exit(1);
			}

			memcpy((char *)(chunk_buffer->buf) + chunk_buffer_offset,log_buffer_element_ptr,log_struct_size);
			chunk_buffer_offset += log_struct_size;

			log_buffer.pop();
			if(log_buffer_element_ptr)
				delete log_buffer_element_ptr;
		}
		
		chunk_buffer->header.cmd = CHNK_CMD_LOG;
		chunk_buffer->header.rsv_1 = REPLY;
		chunk_buffer->header.length = chunk_buffer_offset;
		chunk_buffer->header.sequence_number = 0;

		//non-bolcking send maybe have problem?
		_send_byte = send(log_server_sock, (char *)chunk_buffer, (chunk_buffer->header.length + sizeof(chunk_header_t)), 0);


		if(_send_byte < 0) {
			printf("(READY) send info to log server error : %d  in log_exit\n",WSAGetLastError());
			exit(1);

		}
	}
}

int logger_client::handle_pkt_in(int sock){
	
	log_to_server(LOG_WRITE_STRING,0,"s \n","cannot in this sope in logger_client::handle_pkt_in\n");
	log_exit();
	return RET_OK;
}

int logger_client::handle_pkt_out(int sock){
	Nonblocking_Ctl * Nonblocking_Send_Ctrl_ptr = NULL;
	struct log_pkt_format_struct *log_buffer_element_ptr = NULL;
	Nonblocking_Send_Ctrl_ptr = &(non_log_recv_struct.nonBlockingSendCtrl);
	int chunk_buffer_offset = 0;
	int log_struct_size = 0;
	int _send_byte;
	log_time_differ();

	if(Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY ){

		if((log_time_dffer - previous_time_differ) > TIME_PERIOD){

			previous_time_differ = log_time_dffer;

			if((buffer_size <= BUFFER_CONTENT_THRESHOLD)&&(buffer_clear_flag == 0)){

				if((buffer_size!=0)){
					printf("logger_client::handle_pkt_out buffer != 0\n");
					while((buffer_size!=0)&&(chunk_buffer_offset < TIME_BW)){
						log_buffer_element_ptr = log_buffer.front();

						log_struct_size = log_buffer_element_ptr->log_header.length + sizeof(struct log_header_t);
						buffer_size -= log_struct_size;

						if((buffer_size<0)||((chunk_buffer_offset+log_struct_size) > CHUNK_BUFFER_SIZE)){
							
							log_to_server(LOG_WRITE_STRING,0,"s d d \n","error : buffer size : %d and %d (overflow)\n",buffer_size,(chunk_buffer_offset+log_struct_size));
							log_exit();
						}

						memcpy((char *)(chunk_buffer->buf) + chunk_buffer_offset,log_buffer_element_ptr,log_struct_size);
						chunk_buffer_offset += log_struct_size;

						log_buffer.pop();
						if(log_buffer_element_ptr)
							delete log_buffer_element_ptr;
					}
		
					chunk_buffer->header.cmd = CHNK_CMD_LOG;
					chunk_buffer->header.rsv_1 = REPLY;
					chunk_buffer->header.length = chunk_buffer_offset;
					chunk_buffer->header.sequence_number = 0;

					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.offset =0 ;
					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.total_len = chunk_buffer->header.length + sizeof(chunk_header_t) ;
					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.expect_len = chunk_buffer->header.length + sizeof(chunk_header_t) ;
					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.buffer = (char *)chunk_buffer ;
					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_buffer;
					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.serial_num =  chunk_buffer->header.sequence_number;

					//printf("cmd= %d total_len = %d\n",chunk_ptr->header.cmd,chunk_ptr->header.length );

					_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));


					if(_send_byte < 0) {
						
						log_to_server(LOG_WRITE_STRING,0,"s d \n","(READY) send info to log server error :",WSAGetLastError());
						log_exit();
					}
				}
				
			}
			else{
				buffer_clear_flag = 0;

				if((buffer_size!=0)){
					printf("logger_client::handle_pkt_out buffer != 0 clear\n");
					while(buffer_size!=0){
						log_buffer_element_ptr = log_buffer.front();

						log_struct_size = log_buffer_element_ptr->log_header.length + sizeof(struct log_header_t);
						buffer_size -= log_struct_size;

						if((buffer_size<0)||((chunk_buffer_offset+log_struct_size) > CHUNK_BUFFER_SIZE)){
							
							log_to_server(LOG_WRITE_STRING,0,"s d d \n","error : buffer size : %d and %d (overflow) in send all\n",buffer_size,(chunk_buffer_offset+log_struct_size));
							log_exit();
						}

						memcpy((char *)(chunk_buffer->buf) + chunk_buffer_offset,log_buffer_element_ptr,log_struct_size);
						chunk_buffer_offset += log_struct_size;

						log_buffer.pop();
						if(log_buffer_element_ptr)
							delete log_buffer_element_ptr;
					}
		
					chunk_buffer->header.cmd = CHNK_CMD_LOG;
					chunk_buffer->header.rsv_1 = REPLY;
					chunk_buffer->header.length = chunk_buffer_offset;
					chunk_buffer->header.sequence_number = 0;

					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.offset =0 ;
					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.total_len = chunk_buffer->header.length + sizeof(chunk_header_t) ;
					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.expect_len = chunk_buffer->header.length + sizeof(chunk_header_t) ;
					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.buffer = (char *)chunk_buffer ;
					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_buffer;
					Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.serial_num =  chunk_buffer->header.sequence_number;

					//printf("cmd= %d total_len = %d\n",chunk_ptr->header.cmd,chunk_ptr->header.length );

					_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));


					if(_send_byte < 0) {
					
						log_to_server(LOG_WRITE_STRING,0,"s d \n","(READY) send info to log server error : %d in send all\n",WSAGetLastError());
						log_exit();
					}
				}
			}
		}
	}
	else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING ){
		_send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));
			
		if(_send_byte < 0) {
				
				log_to_server(LOG_WRITE_STRING,0,"s d \n","(RUNNING) send info to log server error : ",WSAGetLastError());
				log_exit();
		}/*else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY){
			
		}*/
	}
	return RET_OK;
}

void logger_client::handle_pkt_error(int sock){
	
	log_to_server(LOG_WRITE_STRING,0,"s \n","cannot in this sope in logger_client::handle_pkt_error");
	log_exit();
}

void logger_client::handle_sock_error(int sock, basic_class *bcptr){
	
	log_to_server(LOG_WRITE_STRING,0,"s \n","cannot in this sope in logger_client::handle_sock_error");
	log_exit();
}

void logger_client::handle_job_realtime(){
	
	log_to_server(LOG_WRITE_STRING,0,"s \n","cannot in this sope in logger_client::handle_job_realtime");
	log_exit();
}

void logger_client::handle_job_timer(){

	log_to_server(LOG_WRITE_STRING,0,"s \n","cannot in this sope in logger_client::handle_job_timer");
	log_exit();
}