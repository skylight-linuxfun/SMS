#ifndef __SMS_CTRL_H__
#define __SMS_CTRL_H__

#ifdef  __cplusplus
extern "C" {
#endif
#include <sys/socket.h>
#include <sys/un.h>
#define BOOL unsigned char
typedef struct{
        int s;
        struct sockaddr_un dest;
}sms_ctrl;

sms_ctrl * sms_ctrl_open(char *path);
int sms_ctrl_get_fd(sms_ctrl *ctrl);
BOOL sms_ctrl_write(sms_ctrl *ctrl,char *msg);
BOOL sms_ctrl_read(sms_ctrl *ctrl,char *buffer,int *length);
void sms_ctrl_close(sms_ctrl *ctrl);

#ifdef  __cplusplus
}
#endif

#endif
