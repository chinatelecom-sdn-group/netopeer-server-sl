#概述#
netopeer-server-sl 是基于libnetconf、netopeer开发的支持netconf功能的系统资源信息采集服务程序，具备采集CPU、Memory、Disk、NIC、vSwitch等资源利用率的能力。

#功能#
*	通过依赖ssh，为Monitor-restconf提供网络连接
*	定义yang的采集模型，实现数据采集功能

#环境#
libnetconf

netopeer
#安装#
	$ make
	$ sudo make install
#运行#
在/etc/sshd/sshd_config最后两行添加：

	Port 830
	Subsystem netconf /usr/local/bin/netopeer-server-sl

最后重启sshd

	systemctl restart sshd

#公司#

*	Guangzhou Research Institute of China Telecom 

#作者#

##设计##
* Hong Tang(chinatelecom.sdn.group@gmail.com)
* Liang Ou(chinatelecom.sdn.group@gmail.com)

##实现##
* Peng li (chinatelecom.sdn.group@gmail.com)