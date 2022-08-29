/*
* Copyright [2021] JD.com, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "hb_log.h"
#include "global.h"
#include "task/task_pkey.h"
#include "table/hotbackup_table_def.h"

HBLog::HBLog(DTCTableDefinition *tbl)
    : tabledef_(tbl), log_writer_(0), log_reader_(0)
    , o_server()
{
    o_server.SetAddress("/tmp/binloglite");
}

HBLog::~HBLog()
{
    DELETE(log_writer_);
    DELETE(log_reader_);
}

int HBLog::init(const char *path, const char *prefix, uint64_t total,
        off_t max_size)
{
    log_writer_ = new BinlogWriter;
    log_reader_ = new BinlogReader;

    if (log_writer_->init(path, prefix, total, max_size)) {
        log4cplus_error("init log_writer failed");
        return DTC_CODE_FAILED;
    }

    if (log_reader_->init(path, prefix)) {
        log4cplus_error("init log_reader failed");
        return -2;
    }

    return DTC_CODE_SUCCESS;
}

int HBLog::write_update_log(DTCJobOperation &job)
{
    RawData *raw_data;
    NEW(RawData(&g_stSysMalloc, 1), raw_data);

    if (!raw_data) {
        log4cplus_error("raw_data is null");
        return DTC_CODE_FAILED;
    }

    HotBackTask &hotbacktask = job.get_hot_back_task();
    int type = hotbacktask.get_type();
    if (raw_data->init(0, tabledef_->key_size(), (const char *)&type, 0, -1,
               -1, 0)) {
        DELETE(raw_data);
        return DTC_CODE_FAILED;
    }
    DTCValue key;
    DTCValue value;
    if (0 == hotbacktask.get_packed_key_len()) {
        log4cplus_error("packedkey len is  zero");
        return DTC_CODE_FAILED;
    } else {
        key.Set(hotbacktask.get_packed_key(),
            hotbacktask.get_packed_key_len());
    }

    if (0 == hotbacktask.get_value_len()) {
        value.Set(0);
    } else {
        value.Set(hotbacktask.get_value(), hotbacktask.get_value_len());
    }

    RowValue row(tabledef_);
    row[0].u64 = type;
    row[1].u64 = hotbacktask.get_flag();
    row[2] = key;
    row[3] = value;
    log4cplus_debug(" tye is %d, flag %d", type, hotbacktask.get_flag());
    raw_data->insert_row(row, false, false);
    log_writer_->insert_header(type, 0, 1);
    log_writer_->append_body(raw_data->get_addr(), raw_data->data_size());

    DTCTableDefinition* p_dtc_tab_def = TableDefinitionManager::instance()->get_cur_table_def();

    DTCValue astKey[p_dtc_tab_def->key_fields()];// always single key
    TaskPackedKey::unpack_key(p_dtc_tab_def, key.str.ptr, astKey);

    const HitImage& image = job.hit_image;
    switch (type)
    {
    case DTCHotBackup::SYNC_INSERT:
        {
            log4cplus_debug("enter into insert");
            for (int i = 0; i < image.before_image.size(); i++) {
                WriteReq write(&o_server);
                write.SetDataBase("dbname");
                write.SetTable("tabname01");
                const RowValue& cur_row = image.before_image[i];
                set_binlite_request(cur_row , astKey[0] , write);
                int iRet = write.Execute();
                log4cplus_error("send to binloglite ret:%d , error info:%s" , iRet , write.Error().c_str());
            }
        }
        break;
    case DTCHotBackup::SYNC_DELETE:
        {
            log4cplus_debug("enter into delete");
            for (int i = 0; i < image.before_image.size(); i++) {
                DeleteReq del(&o_server);
                del.SetDataBase("dbname");
                del.SetTable("tabname01");
                const RowValue& cur_row = image.before_image[i];
                set_binlite_request(cur_row , astKey[0] , del);
                int iRet = del.Execute();
                log4cplus_error("send to binloglite ret:%d , error info:%s" , iRet , del.Error().c_str());
            }
        }
        break;
    case DTCHotBackup::SYNC_PURGE:
        {
            if (DTCHotBackup::HAS_VALUE == hotbacktask.get_flag()) {
                log4cplus_warning("mbl no need handle");
                break;
            }
            
            log4cplus_debug("enter into purge");
            DeleteReq del(&o_server);
            del.SetDataBase("dbname");
            del.SetTable("tabname01");

            switch (p_dtc_tab_def->key_type()) {
            case DField::Signed:
                {
                    del.Set(p_dtc_tab_def->key_name(),(int)astKey[0].s64);
                    log4cplus_debug("field name:%s , field value:%d" , p_dtc_tab_def->key_name() 
                            , (int)astKey[0].s64);
                }
                break;
            case DField::String:
            case DField::Binary:
                {
                    std::string key_str(astKey[0].str.ptr ,
                            astKey[0].str.len );
                
                    del.Set(p_dtc_tab_def->key_name(), key_str);
                    log4cplus_debug("field name:%s , field value:%s" , p_dtc_tab_def->key_name() 
                            , key_str.c_str());
                }
                break;
                default:
                    log4cplus_error("no match field type");
                break;
            }
            int iRet = del.Execute();
            log4cplus_error("send to binloglite ret:%d , error info:%s" , iRet , del.Error().c_str());
        }
        break;
    case DTCHotBackup::SYNC_UPDATE:
        {
            log4cplus_debug("enter into update");
            for (int i = 0; i < image.before_image.size(); i++) {
                UpdateReq update(&o_server);
                update.SetDataBase("dbname");
                update.SetTable("tabname01");
                const RowValue& cur_row = image.before_image[i];
                set_binlite_request(cur_row , astKey[0], update);
                 
                const RowValue& update_row = image.after_image[0];
                set_binlite_request(update_row , astKey[0] , update , false);
                int iRet = update.Execute();
                log4cplus_error("send to binloglite ret:%d , error info:%s" , iRet , update.Error().c_str());
            }
        }
        break;
    default:
        break;
    }

    DELETE(raw_data);
    log4cplus_debug(" packed key len:%d,key len:%d, key :%s", key.bin.len,
            *(unsigned char *)key.bin.ptr, key.bin.ptr + 1);
    return log_writer_->Commit();
}

void HBLog::set_binlite_request(
    const RowValue& cur_row,
    const DTCValue& key_val,
    WDBinReq& req)
{
    for (int j = 0; j <= cur_row.num_fields(); j++) {
        switch (cur_row.field_type(j)) {
        case DField::Signed:
            {
                if (!j) { // int key set
                    req.Set(cur_row.field_name(j), (int)key_val.s64);
                    log4cplus_debug("insert fieldname:%s, key:%d" , cur_row.field_name(j), (int)key_val.s64);
                    continue;
                }
                
                if (unlikely(cur_row.field_size(j) >
                    (int)sizeof(int32_t))) {
                    req.Set(cur_row.field_name(j) , (int)cur_row.field_value(j)->s64);
                } else {
                    req.Set(cur_row.field_name(j) , (int)cur_row.field_value(j)->s64);
                }
                log4cplus_debug("field name:%s , field value:%d" , cur_row.field_name(j) , (int)cur_row.field_value(j)->s64);
            }
            break;
        case DField::String:
        case DField::Binary:
            {
                if (!j) { // string key set
                    std::string key_str(key_val.str.ptr , key_val.str.len);
                    req.Set(cur_row.field_name(j), key_str);
                    log4cplus_debug("insert fieldname:%s, key:%s" , cur_row.field_name(j), key_str.c_str());
                    continue;
                }
                
                log4cplus_debug("bin:%d , binlen:%d, str:%d ,strlen:%d" ,
                    strlen(cur_row.field_value(j)->bin.ptr) ,
                    strlen(cur_row.field_value(j)->str.ptr),
                    cur_row.field_value(j)->bin.len,
                    cur_row.field_value(j)->str.len);
                std::string str(cur_row.field_value(j)->str.ptr ,
                        cur_row.field_value(j)->str.len );
                req.Set(cur_row.field_name(j) , str);
                log4cplus_debug("field name:%s , field value:%s" , cur_row.field_name(j) , str.c_str());
            }
            break;
            default:
                log4cplus_error("no match field type");
            break;
        }
    }
}

void HBLog::set_binlite_request(
    const RowValue& cur_row,
	const DTCValue& key_val,
    UpdateReq& req,
    bool is_bef)
{
    for (int j = 0; j <= cur_row.num_fields(); j++) {
        switch (cur_row.field_type(j)) {
        case DField::Signed:
            {
                if (!j) { // int key set
                    if (is_bef) {
                        req.SetBefImg(cur_row.field_name(j), (int)key_val.s64);
                    } else {
                        req.SetAftImg(cur_row.field_name(j), (int)key_val.s64);
                    }
                    continue;
                }
                
                if (unlikely(cur_row.field_size(j) >
                    (int)sizeof(int32_t))) {
                    if (is_bef) {
                        req.SetBefImg(cur_row.field_name(j), (int)cur_row.field_value(j)->s64);
                    } else {
                        req.SetAftImg(cur_row.field_name(j), (int)cur_row.field_value(j)->s64);
                    }
                } else {
                    if (is_bef) {
                        req.SetBefImg(cur_row.field_name(j), (int)cur_row.field_value(j)->s64);
                    } else {
                        req.SetAftImg(cur_row.field_name(j), (int)cur_row.field_value(j)->s64);
                    }
                }
                log4cplus_debug("field id:%d , field value:%d" , j , (int)cur_row.field_value(j)->s64);
            }
            break;
        case DField::String:
        case DField::Binary:
            {
                if (!j) { // string key set
                    std::string key_str(key_val.str.ptr , key_val.str.len);
                    if (is_bef) {
                        req.SetBefImg(cur_row.field_name(j), key_str);
                    } else {
                        req.SetAftImg(cur_row.field_name(j), key_str);
                    }
                    continue;
                }
                
                log4cplus_debug("bin:%d , binlen:%d, str:%d ,strlen:%d" ,
                    strlen(cur_row.field_value(j)->bin.ptr) ,
                    strlen(cur_row.field_value(j)->str.ptr),
                    cur_row.field_value(j)->bin.len,
                    cur_row.field_value(j)->str.len);
                
                std::string str(cur_row.field_value(j)->str.ptr ,
                        cur_row.field_value(j)->str.len );
            
                if (is_bef) {
                    req.SetBefImg(cur_row.field_name(j), str);
                } else {
                    req.SetAftImg(cur_row.field_name(j), str);
                }
                log4cplus_debug("field id:%d , field value:%s" , j , str.c_str());
            }
            break;
            default:
                log4cplus_error("no match field type");
            break;
        }
    }
}

int HBLog::write_lru_hb_log(DTCJobOperation &job)
{
    RawData *raw_data;
    NEW(RawData(&g_stSysMalloc, 1), raw_data);

    if (!raw_data) {
        log4cplus_error("raw_data is null");
        return DTC_CODE_FAILED;
    }

    HotBackTask &hotbacktask = job.get_hot_back_task();
    int type = hotbacktask.get_type();
    if (raw_data->init(0, tabledef_->key_size(), (const char *)&type, 0, -1,
               -1, 0)) {
        DELETE(raw_data);
        return DTC_CODE_FAILED;
    }
    DTCValue key;
    if (0 == hotbacktask.get_packed_key_len()) {
        log4cplus_error("packedkey len is  zero");
        return DTC_CODE_FAILED;
    } else {
        key.Set(hotbacktask.get_packed_key(),
            hotbacktask.get_packed_key_len());
    }

    RowValue row(tabledef_);
    row[0].u64 = type;
    row[1].u64 = hotbacktask.get_flag();
    row[2] = key;
    row[3].Set(0);
    log4cplus_debug(" type is %d, flag %d", type, hotbacktask.get_flag());
    raw_data->insert_row(row, false, false);
    log_writer_->insert_header(BINLOG_LRU, 0, 1);
    log_writer_->append_body(raw_data->get_addr(), raw_data->data_size());
    DELETE(raw_data);

    log4cplus_debug(
        " write lru hotback log, packed key len:%d,key len:%d, key :%s",
        key.bin.len, *(unsigned char *)key.bin.ptr, key.bin.ptr + 1);
    return log_writer_->Commit();
}

int HBLog::Seek(const JournalID &v)
{
    return log_reader_->Seek(v);
}

/* 批量拉取更新key，返回更新key的个数 */
int HBLog::task_append_all_rows(DTCJobOperation &job, int limit)
{
    int count;
    for (count = 0; count < limit; ++count) {
        /* 没有待处理日志 */
        if (log_reader_->Read())
            break;

        RawData *raw_data;

        NEW(RawData(&g_stSysMalloc, 0), raw_data);

        if (!raw_data) {
            log4cplus_error("allocate rawdata mem failed");
            return DTC_CODE_FAILED;
        }

        if (raw_data->check_size(g_stSysMalloc.get_handle(
                         log_reader_->record_pointer()),
                     0, tabledef_->key_size(),
                     log_reader_->record_length(0)) < 0) {
            log4cplus_error("raw data broken: wrong size");
            DELETE(raw_data);
            return DTC_CODE_FAILED;
        }

        /* attach raw data read from one binlog */
        if (raw_data->do_attach(g_stSysMalloc.get_handle(
                        log_reader_->record_pointer()),
                    0, tabledef_->key_size())) {
            log4cplus_error("attach rawdata mem failed");

            DELETE(raw_data);
            return DTC_CODE_FAILED;
        }

        RowValue r(tabledef_);
        r[0].u64 = *(unsigned *)raw_data->key();

        unsigned char flag = 0;
        while (raw_data->decode_row(r, flag) == 0) {
            log4cplus_debug("type: " UINT64FMT ", flag: " UINT64FMT
                    ", key:%s, value :%s",
                    r[0].u64, r[1].u64, r[2].bin.ptr,
                    r[3].bin.ptr);
            log4cplus_debug("binlog-type: %d",
                    log_reader_->binlog_type());

            job.append_row(&r);
        }

        DELETE(raw_data);
    }

    return count;
}

JournalID HBLog::get_reader_jid(void)
{
    return log_reader_->query_id();
}

JournalID HBLog::get_writer_jid(void)
{
    return log_writer_->query_id();
}
