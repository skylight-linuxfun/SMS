#include "smsctrl.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#define TRUE 1
#define FALSE 0

sms_ctrl *sms_ctrl_open(char *path)
{
	sms_ctrl *ctrl;
	ctrl = malloc(sizeof(*ctrl));
	if(ctrl == NULL)
	{
		return NULL;
	}
	memset(ctrl,0,sizeof(*ctrl));
	ctrl->s = socket(AF_UNIX, SOCK_STREAM, 0); 
	if(ctrl->s < 0)
	{
		free(ctrl);
		return NULL;
	}
	ctrl->dest.sun_family = AF_UNIX;
	strcpy(ctrl->dest.sun_path,path);
	if(connect(ctrl->s, (struct sockaddr *)&ctrl->dest, sizeof(ctrl->dest))<0) 
	{ 
		close(ctrl->s);
		free(ctrl);
		unlink(ctrl->dest.sun_path);
		return NULL;
        }
	return ctrl;
}

int sms_ctrl_get_fd(sms_ctrl *ctrl)
{
	return ctrl->s;
}
BOOL sms_ctrl_write(sms_ctrl *ctrl,char *msg)
{
	if(write(ctrl->s,msg,strlen(msg))<0)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL sms_ctrl_read(sms_ctrl *ctrl,char *buffer,int *length)
{
        int len = 0;
        if((len = read(ctrl->s,buffer,512))<=0)
	{
		return FALSE;
	}
        *length = len;
	return TRUE;
}
void sms_ctrl_close(sms_ctrl *ctrl)
{
	close(ctrl->s);
	free(ctrl);
}
