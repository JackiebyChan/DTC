#ifndef PROTO_CVTER_H_
#define PROTO_CVTER_H_

#include <vector>
#include <string>
#include "cm_conf.h"

class ColData;

/**
 * type define Time
*/
struct TimeSec {
    long long int long_tm; // 距离1970 至今的秒数
    TimeSec() : long_tm(0) {};
    TimeSec(long long int val) : long_tm(val) {};
};

/**
 * type define single field
*/
struct FieldValueCnt {
    union OptionSt
    {
        bool bool_val;
        double double_val;
        int32_t int32_val;
        long long int long_val;
    };
    FieldValueCnt();
    FieldValueCnt(int id);
    FieldValueCnt(int id , int32_t val);
    FieldValueCnt(int id , const std::string& str);
    FieldValueCnt(int id , const TimeSec& val);

    bool has_val;   // field val is null or not
    OptionSt o_val; // field value , exclude string
    std::string str_val; // field value about string
    uint32_t ui_field_id; // field id , corresponding to tab def
    uint32_t ui_field_type; // field type, corresponding to tab def
};

/**
 * type define single row data
*/
typedef std::vector<FieldValueCnt> ColDataCnt;

enum BLCmdType {
    EN_BIN_ADD = 0,
    EN_BIN_DELETE = 1,
    EN_BIN_UPDATE = 2
};

struct RowData {
    RowData();

    /*
        image index : 0 => for write , delete and image before update
                      1 => for image after update
    */
    enum BLImageType {
        EN_BIN_BEFORE_IMG = 0,
        EN_BIN_AFTER_IMG
    };

    uint32_t ui_req_type; // request type : BLCmdType

    DbTabCnt db_tab; // database and table that row data belong to
    
    /*
        col_data_vet[EN_BIN_BEFORE_IMG] : storage write , delete and image before update
        col_data_vet[EN_BIN_AFTER_IMG]  : storage image after update
    */
    std::vector<ColDataCnt> col_data_vet;

    /** serialize to string 
     * 
     * @param buf  string buffer
    */
    bool SerializeToStr(std::string& buf);

    /** deserialize to ColDataCnt array 
     * 
     * @param data  buffer start pointer
     * @param size  buffer size
    */
    bool ParseFromArray(char* data , int size);

    /** verify row data
     * 
     * @param tab_map  table defination map
    */
    int VerifyRowData(const TabDefMap* tab_map);

private:
    void append_col_date(ColData* des_col , const ColDataCnt& src_col);
};

struct RowResponse {
    uint32_t ui_code; // error id , 0:success , others : error
    std::string s_error_info; // error info

    RowResponse();
    RowResponse(uint32_t code , const std::string& error);

    void operator()(uint32_t code , const std::string& error) {
        ui_code = code;
        s_error_info = error;
    };

    bool SerializeToStr(std::string& );
};

#endif