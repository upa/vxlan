hogelan
=======

hogelan is simple vxlan implementation using Linux tap interface.

	Usage:
		 -v : VXLAN Network Identifier (24bit Hex)
		 -m : Multicast Address
		 -i : Multicast Interface
		 -n : Sub-interface number (<4096)
		 -d : Daemon Mode


ex.1)

### Simple L2 extention

	# ./vxlan -i eth0 -v 0 -m MCA.ST.AD.DR -n 0 -d
	# brctl addbr br0
	# btctl addif br0 vnet0
	# brctl addif br0 vxlan0


sapmle topology

	+--------+
	|   VM   |
	|vvvvvvvv|                     
	| eth0 A |                 ******************
	|~~~~~~~~| +---------+     *                *      +---------+
	+ vnet0  + |  vxlan0 | +--------+       +--------+ |  vxlan0 |
	|^^^^^^^^| |^^^^^^^^^| |  eth0  |       |  eth0  | |^^^^^^^^^|
	|        br0         | |   L3   |       |   L3   | |   br0   |
	+====================+==========+       +====================+
	|           Linux KVM           |       |     Linux Box      |
	+-------------------------------+       +--------------------+




ex.2)

### with 802.1q tagging

	# ./vxlan -i eth0 -v 0 -m MCA.ST.AD.DR -n 0 -d
	# vconfig add vxlan0 100
	# vconfig add vxlan0 101
	# vconfig add vxlan0 102
	# btctl addif br102 vxlan.102


sample topology
	                              +------------+
	                              | 172.16.2.1 |
	+------------+ +------------+ |    br102   |
	| 172.16.0.1 | | 172.16.1.1 | |~~~~~~~~~~~~|
	| vxlan.100  | | vxlan.101  | | vxlan.102  |
	|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|
	|                  vxlan0                  |
	+==========================================+------------------+
	|                   host                   | eth0 10.0.0.1/24 |
	+==========================================+------------------+
	                                                    *
	                                                    *
	                                                    *
	                                                    *
	                              +------------+        *
	                              | 172.16.2.2 |        *
	+------------+ +------------+ |    br102   |        *
	| 172.16.0.2 | | 172.16.1.2 | |^^^^^^^^^^^^|        *
	| vxlan.100  | | vlxna.101  | | vxlan.102  |        *
	|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|        *
	|                  vxlan0                  |        *
	+==========================================+------------------+
	|                   host                   | eth0 10.0.0.2/24 |
	+==========================================+------------------+


