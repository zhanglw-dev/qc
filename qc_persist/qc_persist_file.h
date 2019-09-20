#ifndef QC_PERSIST_FILE_H
#define QC_PERSIST_FILE_H

#include "qc_error.h"
#include "qc_message.h"


#pragma pack(push)
#pragma pack(1)

//must as same as qc_message.c::__QcMessage !!!
typedef struct {
	int priority;
	int persist_id;
	int bufflen;
	char *buff;
}Qc_MsgEqual;

#pragma pack(pop)


typedef struct __QcPersistFile QcPersistFile;



QcPersistFile* qc_persist_file_open(int msgbuff_size, int msgcount_limit, const char* persit_filename, QcErr *err);

void qc_persist_file_close(QcPersistFile *persistFile);

int qc_persist_file_append(QcPersistFile *persistFile, Qc_MsgEqual *msgEqual, QcErr *err);

int qc_persist_file_remove(QcPersistFile *persistFile, Qc_MsgEqual *msgEqual, QcErr *err);

int qc_persist_file_fetch_ready(QcPersistFile *persistFile, QcErr *err);

int qc_persist_file_do_fetch(QcPersistFile *persistFile, Qc_MsgEqual *msgEqual, QcErr *err);


#endif  //QC_PERSIST_FILE_H
