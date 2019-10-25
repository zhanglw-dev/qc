#include "qc_service.h"
#include "qc_protocol.h"
#include "qc_producer.h"
#include "qc_consumer.h"


struct __QcQueueSvc {
	char svc_ip[256];
	int svc_port;

	QcThread *listenThread;
	QcList *workThreadList;

	QcQSystem *qSystem;
};


struct __ListenParam {
	QcQueueSvc *queueSvc;
	QcSocket* socket;
};


struct __WorkParam {
	QcQueueSvc *queueSvc;
	QcSocket* socket;
};


typedef struct __ListenParam ListenParam;
typedef struct __WorkParam WorkParam;



void* work_thread_routine(void *param)
{
	int ret;
	QcErr err;
	int msg_len = 0;
	WorkParam *workParam = param;
	QcSocket *socket = workParam->socket;

	char* head_buff = NULL;
	char *body_buff = NULL;
	char qname[32];

	memset(qname, 0, sizeof(qname));

	int head_len = sizeof(QcPrtclHead);
	head_buff = (char*)malloc(sizeof(QcPrtclHead));

	QcPrtclReply *prtclReply;
	qc_malloc(prtclReply, sizeof(QcPrtclReply));

	while (1) {
		ret = qc_tcp_recvall(socket, head_buff, head_len);
		if (ret <= 0)
			goto failed;

		QcPrtclHead *prtclHead = (QcPrtclHead*)head_buff;
		qc_prtcl_head_ntoh(prtclHead);
		int body_len = prtclHead->body_len;

		body_buff = (char*)malloc(body_len);
		body_len = prtclHead->body_len;
		ret = qc_tcp_recvall(socket, body_buff, body_len);
		if (ret <= 0)
			goto failed;
		
		QcProducerHdl producerHdl;
		QcConsumerHdl consumerHdl;

		switch (prtclHead->type) {
		case QC_TYPE_PRODUCER:
			producerHdl.qSystem = workParam->queueSvc->qSystem;
			ret = qc_proc_producer(&producerHdl, head_buff, &err);
			if (ret < 0)
				goto failed;
			break;
		case QC_TYPE_CONSUMER:
			producerHdl.qSystem = workParam->queueSvc->qSystem;
			ret = qc_proc_consumer(&consumerHdl, head_buff, &err);
			if (ret < 0)
				goto failed;
			break;
		}
	}

	return NULL;
failed:
	if (head_buff) qc_free(head_buff);
	if (body_buff) qc_free(body_buff);
	return NULL;
}


void* listen_thread_routine(void *param)
{
	ListenParam *listenParam = param;

	while (1) {
		QcSocket* socket = qc_tcp_accept(listenParam->socket);
		qc_assert(socket);

		WorkParam *workParam = (WorkParam*)malloc(sizeof(WorkParam));
		workParam->queueSvc = listenParam->queueSvc;
		workParam->socket = socket;
		QcThread* thread = qc_thread_create(work_thread_routine, (void*)workParam);

		qc_list_inserttail(listenParam->queueSvc->workThreadList, thread);
	}

}


QcQueueSvc* qc_queuesvc_create(const char* ip, int port, QcQSystem *qSystem, QcErr *err)
{
	QcQueueSvc *queueSvc;
	qc_malloc(queueSvc, sizeof(QcQueueSvc));

	QcList *procList = qc_list_create(1);

	strcpy(queueSvc->svc_ip, ip);
	queueSvc->svc_port = port;
	queueSvc->workThreadList = procList;
	queueSvc->listenThread = NULL;
	queueSvc->qSystem = qSystem;

	return queueSvc;
}


void qc_queuesvc_destory(QcQueueSvc *queueSvc)
{
	qc_list_destroy(queueSvc->workThreadList);
	qc_free(queueSvc);
}


int qc_queuesvc_start(QcQueueSvc *queueSvc, QcErr *err)
{
	int ret;
	QcSocket* socket = qc_socket_create(AF_INET, SOCK_STREAM, 0);

	ListenParam *listenParam;
	qc_malloc(listenParam, sizeof(ListenParam));

	ret = qc_tcp_bind(socket, queueSvc->svc_ip, queueSvc->svc_port);
	if (0 != ret) {
		qc_seterr(err, QC_ERR_SOCKET, "socket bind failed.");
		return -1;
	}

	listenParam->socket = socket;
	listenParam->queueSvc = queueSvc;

	queueSvc->listenThread = qc_thread_create(listen_thread_routine, (void*)listenParam);
	return 0;
}


void qc_queuesvc_stop(QcQueueSvc *queueSvc)
{
	int excode = 0;

	while (1) {
		QcThread *thread = qc_list_pophead(queueSvc->workThreadList);
		if (!thread)
			break;
		qc_thread_cancel(thread);
		qc_thread_join(thread, &excode);
	}

	qc_thread_cancel(queueSvc->listenThread);
	qc_thread_join(queueSvc->listenThread, &excode);
}
