#ifndef MYBIN_CONFIG_H_
#define MYBIN_CONFIG_H_

#include <string>
#include <map>
#include <vector>

struct FieldDef
{
    FieldDef() {};
    FieldDef(const std::string& field_name
         , int field_type, int field_length
         , bool null_able
         , const std::string& key_status
         , const std::string& default_val
         , const std::string& extra)
     : s_field_name(field_name)
     , i_field_type(field_type) , i_field_length(field_length)
     , b_null_able(null_able) , s_key_status(key_status)
     , s_default(default_val) , s_extra(extra)
     { }
    std::string s_field_name;
    int i_field_type;           // same as binary_log_types.h definition
    int i_field_length;
    bool b_null_able;
    std::string s_key_status;
    std::string s_default;
    std::string s_extra;
};

struct TableDef
{
    std::string s_table_name;                   /* table name*/
    std::vector<FieldDef> o_field_vet;          // default : index is field id ,start from 0
    std::map<std::string , int> o_fieldname_id; // same table , field name is unique
};

struct DbTabCnt {
    std::string s_db_name;
    std::string s_table_name;

    bool operator<(const DbTabCnt& o_db_tab) const {
        if (!s_db_name.compare(o_db_tab.s_db_name)) {
            return (s_table_name.compare(o_db_tab.s_table_name) < 0);
        } else {
            return (s_db_name.compare(o_db_tab.s_db_name) < 0);
        }
    }

    bool empty() {
        return (s_db_name.empty() || s_table_name.empty());
    }
};

typedef std::map<DbTabCnt , TableDef>   TabDefMap;
typedef std::map<DbTabCnt , int>        BbTabIdMap;

#endif