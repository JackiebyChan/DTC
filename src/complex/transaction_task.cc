#include "transaction_task.h"
#include "../libs/common/my/my_comm.h"

TransactionTask::TransactionTask()
{
	
}

TransactionTask::~TransactionTask() {}

//TODO: assembling the result of mysql protocol. 
int TransactionTask::save_row(CTaskRequest *request)
{
	

#if 0
	int req_field_size = m_DBConn->field_num;
	ostringstream oss;
	int cnt = 0;
	for (int i = 0; i < req_field_size; i++)
	{
		if (m_DBConn->Row[i])
		{
			stringstream ss;
			switch(m_DBConn->Fields[i].type)
			{
			case MYSQL_TYPE_DECIMAL:
			case MYSQL_TYPE_TINY:
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_LONG:
			case MYSQL_TYPE_FLOAT:
			case MYSQL_TYPE_DOUBLE:
			case MYSQL_TYPE_LONGLONG:
			case MYSQL_TYPE_INT24:
				ss << m_DBConn->Row[i];
				ss >> row[m_DBConn->Fields[i].name];
				cnt++;
				break;	
		
			case MYSQL_TYPE_NULL:
			case MYSQL_TYPE_TIMESTAMP:
			case MYSQL_TYPE_DATE:
			case MYSQL_TYPE_TIME:
			case MYSQL_TYPE_DATETIME:
			case MYSQL_TYPE_YEAR:
			case MYSQL_TYPE_NEWDATE:
			case MYSQL_TYPE_VARCHAR:
			case MYSQL_TYPE_TIMESTAMP2:
			case MYSQL_TYPE_DATETIME2:
			case MYSQL_TYPE_TIME2:
            case MYSQL_TYPE_NEWDECIMAL:
			case MYSQL_TYPE_VAR_STRING:
			case MYSQL_TYPE_STRING:
				row[m_DBConn->Fields[i].name] = m_DBConn->Row[i];
				cnt++;
				break;

			default:
				oss << "enum_field_types is " << m_DBConn->Fields[i].type;
				SetErrorMessage(oss.str());
				return -4;
			}
		}
	}
#endif
	return 0;
}

#pragma pack(1)
struct my_result_set_eof {
	unsigned char eof;
	uint16_t warning;
	uint16_t server_status;
	uint16_t reverse;
};
#pragma pack()

void encode_mysql_header(CBufferChain *r, int len, uint8_t pkt_num)
{
	//Packet Lenght + Packet Number
	unsigned char t[3];
	int_conv_3(t, len);
	memcpy(r->data, t, 3);
	*(r->data + 3) = pkt_num;
}

CBufferChain *encode_eof(CBufferChain *bc, uint8_t &pkt_nr)
{
	CBufferChain *nbc = bc;
	my_result_set_eof eof;
	eof.eof = 0xfe;
	eof.warning = 0;
	eof.server_status = 0x0022;
	eof.reverse = 0;

	int packet_len =
		sizeof(CBufferChain) + sizeof(eof) + sizeof(MYSQL_HEADER_SIZE);
	CBufferChain *r = (CBufferChain *)MALLOC(packet_len);
	if (r == NULL) {
		return NULL;
	}
	r->totalBytes = packet_len - sizeof(CBufferChain);

	memcpy(r->data + sizeof(MYSQL_HEADER_SIZE), &eof, sizeof(eof));
	r->usedBytes = sizeof(MYSQL_HEADER_SIZE) + sizeof(eof);
	r->nextBuffer = NULL;
	encode_mysql_header(r, sizeof(eof), pkt_nr++);

	nbc->nextBuffer = r;
	nbc = nbc->nextBuffer;

	return nbc;
}

int encode_my_fileds_info(CBufferChain **bc, uint8_t& pkt_num, uint8_t fields_num)
{
	int packet_len = sizeof(CBufferChain) + sizeof(MYSQL_HEADER_SIZE) +
			 sizeof(fields_num);

	*bc = (CBufferChain *)MALLOC(packet_len);
	CBufferChain *r = *bc;
	if (r == NULL) {
		return -ENOMEM;
	}
	r->totalBytes = packet_len - sizeof(CBufferChain);
	encode_mysql_header(r, 1, pkt_num++);
	*(r->data + sizeof(MYSQL_HEADER_SIZE)) = fields_num;
	r->usedBytes = 5;
	r->nextBuffer = NULL;

	return 0;
}

struct my_result_set_field {
	std::string catalog;
	std::string database;
	std::string table;
	std::string original_table;
	std::string name;
	std::string original_name;
	uint16_t charset_number;
	uint32_t length;
	uchar type;
	uint16_t flags;
	uchar decimals;
	uint16_t reverse;
};


uint16_t build_charset(int type)
{
	switch (type) {
	case MYSQL_TYPE_VAR_STRING:
		return 0xff; //utf8mb4
		break;
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_FLOAT:
	default:
		return 0x3f; //binary COLLATE binary
	}
}

uint16_t build_length(int type)
{
	switch (type) {
	case MYSQL_TYPE_VAR_STRING:
		return 200;
		break;
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_FLOAT:
	default:
		return 11;
	}
}

int encode_set_field(char *buf, my_result_set_field *sf)
{
	int len = 0;
	unsigned char *p = (unsigned char*)buf;

	len = sf->catalog.length();
	*p++ = (uint8_t)len;
	if (len > 0) {
		memcpy(p, sf->catalog.c_str(), len);
		p += len;
	}

	len = sf->database.length();
	*p++ = (uint8_t)len;
	if (len > 0) {
		memcpy(p, sf->database.c_str(), len);
		p += len;
	}

	len = sf->table.length();
	*p++ = (uint8_t)len;
	if (len > 0) {
		memcpy(p, sf->table.c_str(), len);
		p += len;
	}

	len = sf->original_table.length();
	*p++ = (uint8_t)len;
	if (len > 0) {
		memcpy(p, sf->original_table.c_str(), len);
		p += len;
	}

	len = sf->name.length();
	*p++ = (uint8_t)len;
	if (len > 0) {
		memcpy(p, sf->name.c_str(), len);
		p += len;
	}

	len = sf->original_name.length();
	*p++ = (uint8_t)len;
	if (len > 0) {
		memcpy(p, sf->original_name.c_str(), len);
		p += len;
	}

	//charset number
	*p++ = 0x0c;
	int2store_big_endian(p, sf->charset_number);
	p += sizeof(sf->charset_number);

	//length
	int4store_big_endian(p, sf->length);
	p += sizeof(sf->length);

	//type
	*p = sf->type;
	p += sizeof(sf->type);

	//flags
	int2store_big_endian(p, sf->flags);
	p += sizeof(sf->flags);

	//decimals
	*p = sf->decimals;
	p += sizeof(sf->decimals);

	//reverse
	*p = sf->reverse;
	p += sizeof(sf->reverse);

	return p - (unsigned char*)buf;
}

int build_field_type(int type)
{
	switch (type) {
	case DField::Signed:
		return MYSQL_TYPE_LONG;
	case DField::Unsigned:
		return MYSQL_TYPE_LONG;
	case DField::Float:
		return MYSQL_TYPE_FLOAT;
	case DField::String:
		return MYSQL_TYPE_VAR_STRING;
	case DField::Binary:
		return MYSQL_TYPE_VAR_STRING;
	}
}


int calc_field_def(my_result_set_field *sf)
{
	int len = 0;

	len++;
	len += sf->catalog.length();

	len++;
	len += sf->database.length();

	len++;
	len += sf->table.length();

	len++;
	len += sf->original_table.length();

	len++;
	len += sf->name.length();

	len++;
	len += sf->original_name.length();

	//charset number
	len++; //0x0c
	len += sizeof(sf->charset_number);

	//length
	len += sizeof(sf->length);

	//type
	len += sizeof(sf->type);

	//flag
	len += sizeof(sf->flags);

	//decimals
	len += sizeof(sf->decimals);

	//reverse
	len += sizeof(sf->reverse);

	log4cplus_debug("sf len:%d", len);

	return len;
}

CBufferChain *encode_field_def(MysqlConn* m_DBConn, CBufferChain *bc, uint8_t& pkt_num)
{
	CBufferChain *nbc = bc;
	CBufferChain *r = NULL;
	MYSQL_FIELD *Fields = m_DBConn->Fields;

	if(!Fields)
		return NULL;

	for (int i = 0; i < m_DBConn->field_num; i++) {
		my_result_set_field sf;
		sf.type = Fields[i].type;
		sf.charset_number = Fields[i].charsetnr;
		sf.database = Fields[i].db;
		sf.length = Fields[i].length;
		sf.catalog = Fields[i].catalog;
		sf.table = Fields[i].table;
		sf.original_table = Fields[i].org_table;
		sf.name = Fields[i].name;
		sf.original_name = Fields[i].org_name;
		sf.decimals = Fields[i].decimals;
		sf.flags = Fields[i].flags;
		sf.reverse = 0x0000;

		int packet_len = sizeof(CBufferChain) + calc_field_def(&sf) +
				 sizeof(MYSQL_HEADER_SIZE);
		r = (CBufferChain *)MALLOC(packet_len);
		if (r == NULL) {
			return NULL;
		}
		memset(r, 0, packet_len);
		r->totalBytes = packet_len - sizeof(CBufferChain);

		int set_len = encode_set_field(
			r->data + sizeof(MYSQL_HEADER_SIZE), &sf);
		log4cplus_debug("set_len:%d", set_len);
		r->usedBytes = sizeof(MYSQL_HEADER_SIZE) + set_len;
		r->nextBuffer = NULL;
		encode_mysql_header(r, set_len, pkt_num++);

		nbc->nextBuffer = r;
		nbc = nbc->nextBuffer;
	}
	return nbc;
}


CBufferChain *encode_row_data(MysqlConn* dbconn, CBufferChain *bc, uint8_t &pkt_nr)
{
	CBufferChain *nbc = bc;

	int nRows = dbconn->row_num; 
	for (int i = 0; i < nRows; i++) 
	{
		int ret = dbconn->FetchRow();
		if (0 != ret) {
			dbconn->FreeResult();
			log4cplus_info("%s!", "call FetchRow func error");
			return NULL;
		}

		unsigned long *lengths = 0;
		lengths = dbconn->getLengths();
		if (0 == lengths) {
			log4cplus_error("row length is 0.");
			dbconn->FreeResult();
			return NULL;
		}

		//calc current row len
		int row_len = 0;
		for (int j = 0; j < dbconn->field_num; j++) {
			char *v = dbconn->Row[j];
			row_len++; //first byte for result len
			row_len += lengths[j];
		}

		//alloc new buffer to store row data.
		int packet_len = sizeof(CBufferChain) +
				 sizeof(MYSQL_HEADER_SIZE) + row_len;
		CBufferChain *nbuff = (CBufferChain *)MALLOC(packet_len);
		if (nbuff == NULL) {
			return NULL;
		}
		nbuff->totalBytes = packet_len - sizeof(CBufferChain);
		nbuff->usedBytes = sizeof(MYSQL_HEADER_SIZE) + row_len;
		nbuff->nextBuffer = NULL;

		char *r = nbuff->data;
		encode_mysql_header(nbuff, row_len, pkt_nr++);
		int offset = 0;
		offset += sizeof(MYSQL_HEADER_SIZE);

		//copy fields content
		for (int j = 0; j < dbconn->field_num; j++) {
				*(r + offset) = lengths[j];
				offset++;
				memcpy(r + offset, dbconn->Row[j], lengths[j]);
				offset += lengths[j];
		}

		nbc->nextBuffer = nbuff;
		nbc = nbc->nextBuffer;

	}

	return nbc;
}

std::string build_dtc_table_type(std::string real_tbname, std::string real_dbname)
{
	std::string str = real_dbname;
	str += ".";
	str += real_tbname;
	log4cplus_debug("query str: %s, map size: %d", str.c_str(), g_config.table_type_info.size());

	map<string,string>::iterator it;
	for(it=g_config.table_type_info.begin();it!=g_config.table_type_info.end();it++)
	{
		log4cplus_debug("first: %s, second: %s", (*it).first.c_str(), (*it).second.c_str());
	}
	std::string res = g_config.table_type_info[str];
	if(res.length() > 0)
		return res;
	else
		return "SINGLE TABLE";
}

bool ignore_system_db_name(std::string dbname)
{
	if(dbname == std::string("mysql") || 
		dbname == std::string("information_schema") || 
		dbname == std::string("performance_schema") || 
		dbname == std::string("sys"))
		return true;

	YAML::Node dtc = g_config.get_conf();
	if(dbname == dtc["primary"]["full"]["logic"]["db"].as<std::string>())
		return true;

	return false;
}

CBufferChain *encode_show_db_row_data(MysqlConn* dbconn, CBufferChain *bc, uint8_t &pkt_nr)
{
	CBufferChain *nbc = bc;

	int nRows = dbconn->row_num; 
	for (int i = 0; i < nRows; i++) 
	{
		int ret = dbconn->FetchRow();
		if (0 != ret) {
			dbconn->FreeResult();
			log4cplus_info("%s!", "call FetchRow func error");
			return NULL;
		}

		unsigned long *lengths = 0;
		lengths = dbconn->getLengths();
		if (0 == lengths) {
			log4cplus_error("row length is 0.");
			dbconn->FreeResult();
			return NULL;
		}

		if(dbconn->field_num != 1)
		{
			log4cplus_error("field num is %d.", dbconn->field_num);
			dbconn->FreeResult();
			return NULL;
		}

		//calc current row len
		int row_len = 0;

		if(ignore_system_db_name(dbconn->Row[0]))
		{
			log4cplus_debug("ignore db: %s", dbconn->Row[0]);
			continue;
		}	

		for (int j = 0; j < dbconn->field_num; j++) {
			row_len++; //first byte for result len
			row_len += lengths[j];
		}

		//alloc new buffer to store row data.
		int packet_len = sizeof(CBufferChain) +
				 sizeof(MYSQL_HEADER_SIZE) + row_len;
		CBufferChain *nbuff = (CBufferChain *)MALLOC(packet_len);
		if (nbuff == NULL) {
			return NULL;
		}
		nbuff->totalBytes = packet_len - sizeof(CBufferChain);
		nbuff->usedBytes = sizeof(MYSQL_HEADER_SIZE) + row_len;
		nbuff->nextBuffer = NULL;

		char *r = nbuff->data;
		encode_mysql_header(nbuff, row_len, pkt_nr++);
		int offset = 0;
		offset += sizeof(MYSQL_HEADER_SIZE);

		//copy fields content
		for (int j = 0; j < dbconn->field_num; j++) {
			*(r + offset) = lengths[j];
			offset++;
			memcpy(r + offset, dbconn->Row[j], lengths[j]);
			offset += lengths[j];
		}

		nbc->nextBuffer = nbuff;
		nbc = nbc->nextBuffer;
	}

	return nbc;
}

CBufferChain *encode_show_tables_row_data(MysqlConn* dbconn, CBufferChain *bc, uint8_t &pkt_nr, std::string dbname)
{
	CBufferChain *nbc = bc;

	int nRows = dbconn->row_num; 
	for (int i = 0; i < nRows; i++) 
	{
		int ret = dbconn->FetchRow();
		if (0 != ret) {
			dbconn->FreeResult();
			log4cplus_info("%s!", "call FetchRow func error");
			return NULL;
		}

		unsigned long *lengths = 0;
		lengths = dbconn->getLengths();
		if (0 == lengths) {
			log4cplus_error("row length is 0.");
			dbconn->FreeResult();
			return NULL;
		}

		if(dbconn->field_num < 1)
		{
			log4cplus_error("field num is %d.", dbconn->field_num);
			dbconn->FreeResult();
			return NULL;
		}

		if(std::string(dbconn->Row[0]) == "data_lifecycle_table")
			continue;

		//calc current row len
		int row_len = 0;
		std::string tbtypestr = build_dtc_table_type(dbconn->Row[0], dbname);

		for (int j = 0; j < dbconn->field_num; j++) {
			if(j == 1 && tbtypestr.length() > 0) 	// table type field
			{
				row_len++; //first byte for result len
				row_len += tbtypestr.length();
			}
			else
			{
				row_len++; //first byte for result len
				row_len += lengths[j];
			}
		}

		//alloc new buffer to store row data.
		int packet_len = sizeof(CBufferChain) +
				 sizeof(MYSQL_HEADER_SIZE) + row_len;
		CBufferChain *nbuff = (CBufferChain *)MALLOC(packet_len);
		if (nbuff == NULL) {
			return NULL;
		}
		nbuff->totalBytes = packet_len - sizeof(CBufferChain);
		nbuff->usedBytes = sizeof(MYSQL_HEADER_SIZE) + row_len;
		nbuff->nextBuffer = NULL;

		char *r = nbuff->data;
		encode_mysql_header(nbuff, row_len, pkt_nr++);
		int offset = 0;
		offset += sizeof(MYSQL_HEADER_SIZE);

		//copy fields content
		for (int j = 0; j < dbconn->field_num; j++) {
			if(j == 1 && tbtypestr.length() > 0) 	// table type field
			{
				*(r + offset) = tbtypestr.length();
				offset++;
				memcpy(r + offset, tbtypestr.c_str(), tbtypestr.length());
				offset += tbtypestr.length();
			}
			else
			{
				*(r + offset) = lengths[j];
				offset++;
				memcpy(r + offset, dbconn->Row[j], lengths[j]);
				offset += lengths[j];
			}
		}

		nbc->nextBuffer = nbuff;
		nbc = nbc->nextBuffer;
	}

	return nbc;
}

CBufferChain *TransactionTask::encode_mysql_error(CTaskRequest *request, std::string errmsg, int myerrno)
{
	CBufferChain *bc = NULL;
	CBufferChain *pos = NULL;

	uint8_t buf[9] = {0xff, 0x0, 0x0, 0x30, 0x30, 0x30, 0x30, 0x30, 0x20};

	uint8_t pkt_nr = request->get_seq_num();
	pkt_nr++;

	int packet_len = sizeof(CBufferChain) + sizeof(MYSQL_HEADER_SIZE) +
		sizeof(buf) + errmsg.length();

	myerrno = abs(myerrno);
	int2store_big_endian(buf+1, myerrno);
	log4cplus_debug("myerrno: %d, buf: %x, %x, %x", myerrno, buf[0], buf[1], buf[2]);
	bc = (CBufferChain *)MALLOC(packet_len);
	CBufferChain *r = bc;
	if (r == NULL) {
		return -ENOMEM;
	}
	r->totalBytes = packet_len - sizeof(CBufferChain);
	encode_mysql_header(r, sizeof(buf) + errmsg.length(), pkt_nr);
	log4cplus_debug("len:%d, seq:%d, packet_len:%d, msg len:%d", sizeof(buf), pkt_nr, packet_len, errmsg.length());
	memcpy(r->data + sizeof(MYSQL_HEADER_SIZE), buf, sizeof(buf));
	memcpy(r->data + sizeof(MYSQL_HEADER_SIZE) + sizeof(buf), errmsg.c_str(), errmsg.length());
	r->usedBytes = sizeof(MYSQL_HEADER_SIZE) + sizeof(buf) + errmsg.length();
	r->nextBuffer = NULL;

	return bc;
}

CBufferChain *TransactionTask::encode_mysql_ok(CTaskRequest *request, int affected_rows)
{
	CBufferChain *bc = NULL;
	CBufferChain *pos = NULL;

	uint8_t buf[7] = {0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
	int2store_big_endian(buf+ 1, affected_rows);

	uint8_t pkt_nr = request->get_seq_num();
	pkt_nr++;

	int packet_len = sizeof(CBufferChain) + sizeof(MYSQL_HEADER_SIZE) +
		sizeof(buf);

	bc = (CBufferChain *)MALLOC(packet_len);
	CBufferChain *r = bc;
	if (r == NULL) {
		return -ENOMEM;
	}
	r->totalBytes = packet_len - sizeof(CBufferChain);
	encode_mysql_header(r, sizeof(buf), pkt_nr);
	log4cplus_debug("len:%d, seq:%d, affected:%d, packet_len:%d", sizeof(buf), pkt_nr, affected_rows, packet_len);
	memcpy(r->data + sizeof(MYSQL_HEADER_SIZE), buf, sizeof(buf));
	r->usedBytes = sizeof(buf) + sizeof(MYSQL_HEADER_SIZE);
	r->nextBuffer = NULL;

	return bc;
}

CBufferChain* TransactionTask::encode_mysql_protocol(CTaskRequest *request)
{
	CBufferChain *bc = NULL;
	CBufferChain *pos = NULL;
	log4cplus_debug("encode_mysql_protocol entry.");

	uint8_t pkt_nr = request->get_seq_num();
	pkt_nr++;

	if(m_DBConn->field_num <= 0)
		return NULL;

	int ret = encode_my_fileds_info(&bc, pkt_nr, m_DBConn->field_num);
	if (ret < 0)
		return NULL;

	pos = encode_field_def(m_DBConn, bc, pkt_nr);
	if (!pos)
		return NULL;

	if(request->get_eof_packet_new() == false)
	{
		//Different MYSQL Version.
		pos = encode_eof(pos, pkt_nr);
		if (!pos)
			return NULL;
	}

	CBufferChain *prow = NULL;
	if(request->cmd == QUERY_CMD_SHOW_TABLES)
		prow = encode_show_tables_row_data(m_DBConn, pos, pkt_nr, request->get_dbname());
	else if(request->cmd == QUERY_CMD_SHOW_DB)
		prow = encode_show_db_row_data(m_DBConn, pos, pkt_nr);
	else
		prow = encode_row_data(m_DBConn, pos, pkt_nr);
	if (prow) {
		pos = prow;
	}

	pos = encode_eof(pos, pkt_nr);
	if (!pos)
		return NULL;

	log4cplus_debug("encode_mysql_protocol leave.");
	return bc;
}

int TransactionTask::request_db_query(std::string request_sql, CTaskRequest *request)
{
	std::string m_Sql = request_sql;
	log4cplus_debug("request_db_query entry.");
	
	if(m_DBConn->Ping() != 0)
	{
		const char *errmsg = m_DBConn->GetErrMsg();
		int m_ErrorNo = m_DBConn->GetErrNo();
		log4cplus_error("db reconnect error, errno[%d]  errmsg[%s]", m_ErrorNo, errmsg);
		SetErrorMessage(errmsg);
		CBufferChain* rb = encode_mysql_error(request, std::string(errmsg), m_ErrorNo);
		if(rb)
			request->set_buffer_chain(rb);
		return m_ErrorNo;
	}

	std::string db = request->get_dbname();

	if(m_Sql.find("insert into") == 0 || m_Sql.find("update ") == 0 || m_Sql.find("delete from ") == 0)
	{
		request->cmd = QUERY_CMD_WRITE;
	}
	else
	{
		if(m_Sql.find("show tables") == 0 || m_Sql.find("show full tables") == 0)
		{
			log4cplus_debug("query tables info.");
			request->cmd = QUERY_CMD_SHOW_TABLES;
		}
		else if(m_Sql.find("show databases") == 0)
		{
			log4cplus_debug("query db info.");
			request->cmd = QUERY_CMD_SHOW_DB;
		}
		else
		{
			log4cplus_debug("query result.");
			request->cmd = QUERY_CMD_NORMAL;
		}
	}
  
	int ret = 0;
	if(db.length() == 0)
		ret = m_DBConn->Query(m_Sql.c_str());
	else
	{
		if(request->get_layer() == 3 && db == std::string(g_config.hot_instance.DbName))
		{
			log4cplus_debug("complex: get layer is 3");
			log4cplus_debug("complex: dbname: %s", g_config.full_instance.DbName);
			ret = m_DBConn->Query(g_config.full_instance.DbName, m_Sql.c_str());
		}
		else
		{
			ret = m_DBConn->Query(db.c_str(), m_Sql.c_str());
		}
	}
		
	if(0 != ret)
	{
		const char *errmsg = m_DBConn->GetErrMsg();
		int m_ErrorNo = m_DBConn->GetErrNo();
		log4cplus_error("db execute error. errno[%d]  errmsg[%s]", m_ErrorNo, errmsg);
		log4cplus_error("error sql [%s]", m_Sql.c_str());
		SetErrorMessage(errmsg);
		CBufferChain* rb = encode_mysql_error(request, std::string(errmsg), m_ErrorNo);
		if(rb)
			request->set_buffer_chain(rb);
		return m_ErrorNo;
	}

	if(request->cmd == QUERY_CMD_WRITE)
	{
		int affected = m_DBConn->AffectedRows();
		log4cplus_debug("affected result, %d.", affected);
		CBufferChain* rb = encode_mysql_ok(request, affected);
		if(rb)
			request->set_buffer_chain(rb);
	}
	else
	{
		ret = m_DBConn->UseResult();
		if (0 != ret) {
			log4cplus_error("can not use result,sql[%s]", m_Sql.c_str());
			SetErrorMessage(m_DBConn->GetErrMsg());
			CBufferChain* rb = encode_mysql_error(request, std::string(m_DBConn->GetErrMsg()), m_DBConn->GetErrNo());
			if(rb)
				request->set_buffer_chain(rb);			
			return -4;
		}

		ret = m_DBConn->FetchFields();
		if (0 != ret) {
			log4cplus_error("can not use fileds,[%d]%s", m_DBConn->GetErrNo(), m_DBConn->GetErrMsg());
			CBufferChain* rb = encode_mysql_error(request, std::string(m_DBConn->GetErrMsg()), m_DBConn->GetErrNo());
			if(rb)
				request->set_buffer_chain(rb);			
			return -4;
		}

		CBufferChain* rb = encode_mysql_protocol(request);
		if(rb)
			request->set_buffer_chain(rb);

		m_DBConn->FreeResult();
	}

	log4cplus_debug("request_db_query leave.");
	return 0;
}

int TransactionTask::Process(CTaskRequest *request) {
	log4cplus_debug("async-conn: pop task process begin.");

	string request_sql = request->parse_request_sql();
	log4cplus_debug("pop sql: len:%d,  %s", request_sql.length(), request_sql.c_str());
	if (request_sql.length() <= 0)
	{
		log4cplus_debug("request sql error.");

		//TODO: convert ERROR mysql protocol.
		request->setResult("");
		return 0;
	}

	int ret = request_db_query(request_sql, request);

	log4cplus_debug("async-conn: pop task process end.");
	return 0;
}
