#ifndef _CONTROLL_H_
#define _CONTROLL_H_

#define CONTROL_MSG_BUF_LEN	2048
#define CONTROL_CMD_BUF_LEN	256
#define CONTROL_ARG_MAX	      	16


void init_vxlan_control (void);

#define COMMAND_TYPE_INVALID	       	0
#define COMMAND_TYPE_VNI_CREATE		1
#define COMMAND_TYPE_VNI_DESTROY	2
#define COMMAND_TYPE_SHOW		5


#define CONTROL_ERRMSG "invalid command arguments\n"


#endif /* _CONTROLL_H_ */
