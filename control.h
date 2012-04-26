#ifndef _CONTROLL_H_
#define _CONTROLL_H_

/* access list related type */

#define ACL_TYPE_MAC            0
#define ACL_TYPE_IP4            1
#define ACL_TYPE_IP6            2
#define ACL_TYPE_RA             3
#define ACL_TYPE_RS             4


struct acl_entry {
        int type;
        u_int8_t vni[VXLAN_VNISIZE];

        union {
                u_int8_t mac[ETH_ALEN];
                struct in_addr addr4;
                struct in6_addr addr6;
        } term;
};
#define term_mac        term.mac
#define term_ip4        term.addr4
#define term_ip6        term.addr6

int strtoaclentry (char * str, struct acl_entry * e);
int install_acl_entry (struct vxlan_instance * vins, struct acl_entry ae);
int uninstall_acl_entry (struct vxlan_instance * vins, struct acl_entry ae);


#define CONTROL_MSG_BUF_LEN	2048
#define CONTROL_CMD_BUF_LEN	256
#define CONTROL_ARG_MAX	      	16

void init_vxlan_control (void);


#define COMMAND_TYPE_INVALID	       	0
#define COMMAND_TYPE_VNI_CREATE		1
#define COMMAND_TYPE_VNI_DESTROY	2
#define COMMAND_TYPE_ACL		3
#define COMMAND_TYPE_SHOW		5


#define CONTROL_ERRMSG "invalid command arguments\n"


#endif /* _CONTROLL_H_ */
