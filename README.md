hogelan
=======

hogelan is simple vxlan implementation using Linux tap interface.

ID : https://tools.ietf.org/html/draft-mahalingam-dutt-dcops-vxlan-00

hogelan includes "vxland" and "vxlanctl" commands.
vxland, is vxlan daemon, forwards packet to VXLAN 
Overlay Network. vxlanctl is command for controlling vxlan. 
You can create/destroy vxlan tunnel interface using 
vxlanctl. And configure access list about ARP/ND/MAC.

INSTALL
-------

	% git clone git://github.com/upa/hogelan.git
	% cd hogelan
	% make
	% make install


How to Use
----------

### vxlan.conf ###

Multicast Address and Interface are configured in 
/usr/local/etc/vxlan.conf . vxland can work with 
IPv4 and IPv6.

	# vxlan.conf
	multicast_address 239.0.0.1
	multicast_interface eth0

### vxland ###

vxland is daemon for forwarding packet.

+ start : _/etc/init.d/vxlan start_
+ stop : _/etc/init.d/vxlan stop_

Other configurations is installed by vxlanctl.


### vxlanctl ###

vxlanctl is command tool for configuring vxlan. you can 
create/destroy vxlan tunnel interface, and install Access 
list. All Access List affect as Out Bound Filter per VNI. 
if you want to use vlan, create vlan interface using vconfig.

	### Usage ####
	 % vxlanctl --help
	    Usage :
	 	  vxlanctl [commands]
	   
	    commands:    (VNI is hex)
	  
	     create <VNI>                             add vxlan interface
	     destroy <VNI>                            delete vxlan interface
	     acl <VNI> mac [deny|permit] <Mac Addr>   Source Mac Address Filter
	     acl <VNI> arp [deny|permit] <v4Addr>     ARP Target Address Filter
	     acl <VNI> ns  [deny|permit] <v6Addr>     NS Target Address Filter
	     acl <VNI> ra  [deny|permit]              RA Filter
	     acl <VNI> rs  [deny|permit]              RS FIlter
	  
	  	 
	### Create vxlan interface  ####
	 % vxlanctl create 0
	 created
	 %
	 % ifconfig vxlan0
	 vxlan0    Link encap:Ethernet  HWaddr 06:b3:1a:45:86:9a  
	           inet6 addr: fe80::4b3:1aff:fe45:869a/64 Scope:Link
	           UP BROADCAST RUNNING  MTU:1500  Metric:1
	           RX packets:0 errors:0 dropped:0 overruns:0 frame:0
	           TX packets:4 errors:0 dropped:0 overruns:0 carrier:0
	           collisions:0 txqueuelen:500 
	           RX bytes:0 (0.0 B)  TX bytes:328 (328.0 B)
	 
	 
	### using with 802.1q vlan ####
	 % vconfig add vxlan0 100	// using vlan tag
	 Added VLAN with VID == 100 to IF -:vxlan0:-
	 % ifconfig vxlan0.100 up
	 % ifconfig vxlan0.100
	 vxlan0    Link encap:Ethernet  HWaddr 06:b3:1a:45:86:9a  
	           inet6 addr: fe80::4b3:1aff:fe45:869a/64 Scope:Link
	           UP BROADCAST RUNNING  MTU:1500  Metric:1
	           RX packets:0 errors:0 dropped:0 overruns:0 frame:0
	           TX packets:6 errors:0 dropped:0 overruns:0 carrier:0
	           collisions:0 txqueuelen:500 
	           RX bytes:0 (0.0 B)  TX bytes:468 (468.0 B)


ToDo
----
+ Per VLAN Access List
+ Per VLAN Forwarding Database (it is Per VNI FDB now)
+ Support *BSD 


License
-------
Under BSD LICENSE.


Contact
-------
upa@haeena.net


