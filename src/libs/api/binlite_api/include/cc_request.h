#ifndef CC_REQUEST_H_
#define CC_REQUEST_H_

#include "proto_cvter.h"

class ColData;

enum ErrorCode {
    EN_MBL_SUCCESSS                     = 0,
    EN_MBL_REQ_NONE_DBNAME              = -10001,
    EN_MBL_REQ_NONE_TABNAME             = -10002,
    EN_MBL_REQ_SERIAL_FAILL             = -10003,
    EN_MBL_REPONSE_DESERIAL_FAILL       = -10004,
    EN_MBL_HAS_ERROR_IN_MBL             = -10005,
    EN_MBL_RECV_REPONSE_FAILL           = -10006,
    EN_MBL_CONNECT_FAILL                = -10007,
    EN_MBL_TABLE_DEF_NULL               = -10008,
    EN_MBL_REQ_FIELD_MISMATCH           = -10009
};

class Server {
public:
    Server();
    ~Server() {
        Close();
    };

public:
    void SetAddress(const char* s_unix) {
        s_unix_path_.clear();
        s_unix_path_.append(s_unix);
    };
    void SetTimeout(int i_time) {
        i_time_out_ = i_time;
    };
    int Connect();
    char* SendAndRecv(const char* p_Buf, uint32_t i_Len , int& iRecvLen);

private:
    void Close();
    int ReceivePacket(char* data, int dataLen);
    int SendPacket(const char *data, int dataLen);
    

private:
    std::string s_unix_path_;
    int client_socket_;
    int i_time_out_;
    bool b_connected;
};

class BinReq {
public:
    BinReq(Server* p_svr);

public:
    bool SetDataBase(const std::string& db_name);

    bool SetTable(const std::string& tab_name);

    const std::string& Error() { return s_error_info;};

    int Execute();

protected:
    template<typename T>
    bool SetImg(
        const std::string& field_name,
        ColDataCnt& cur_col, T val)
    {
        int field_id = -1;
        if (!get_field_id(field_name , field_id)) {
            return false;
        }
        
        cur_col.push_back(FieldValueCnt(field_id , val));
        return true;
    };

protected:
    bool get_field_id(const std::string& field_name, int& field_id);
    bool null_set(const std::string& field_name , ColDataCnt& cur_col);

protected:
    RowData row_data;
    Server* p_server;
    std::string s_error_info;
    TableDef* p_cur_table;
};

class WDBinReq : public BinReq {
public:
    WDBinReq(Server* p_svr) : BinReq(p_svr) {};

public:
    template<typename T>
    bool Set(const std::string& field_name , T val) {
        return SetImg(field_name , row_data.col_data_vet[RowData::EN_BIN_BEFORE_IMG] , val);
    };

    bool SetNull(const std::string& field_name) { 
        return null_set(field_name , row_data.col_data_vet[RowData::EN_BIN_BEFORE_IMG]);
    };
};

class WriteReq : public WDBinReq {
public:
    WriteReq(Server* p_svr) : WDBinReq(p_svr) { row_data.ui_req_type = EN_BIN_ADD; };
};

class DeleteReq : public WDBinReq {
public:
    DeleteReq(Server* p_svr) : WDBinReq(p_svr) { row_data.ui_req_type = EN_BIN_DELETE; };
};

class UpdateReq : public BinReq {
public:
    UpdateReq(Server* p_svr) : BinReq(p_svr) { row_data.ui_req_type = EN_BIN_UPDATE; };

public:
    template<typename T>
    bool SetBefImg(const std::string& field_name , T val) {
        return SetImg(field_name , row_data.col_data_vet[RowData::EN_BIN_BEFORE_IMG] , val);
    };

    bool SetBefImgNull(const std::string& field_name) {
        return null_set(field_name , row_data.col_data_vet[RowData::EN_BIN_BEFORE_IMG]);
    };

    template<typename T>
    bool SetAftImg(const std::string& field_name , T val) {
        return SetImg(field_name , row_data.col_data_vet[RowData::EN_BIN_AFTER_IMG] , val);
    };

    bool SetAftImgNull(const std::string& field_name) {
        return null_set(field_name , row_data.col_data_vet[RowData::EN_BIN_AFTER_IMG]);
    };
};

#endif