Round-Robin多线程TCP服务器demo
=============================
-----
**简介**: 基于libevent库，采用master-worker模式实现one loop per thread IO模型。master线程监听连接请求并分发连接到各个worker线程，worker线程负责具体连接的数据读写和处理。  

* **支持平台**: x86-64 linux  
* **开发语言**: C++  
* **开发平台**: CentOS release 6.3 
* **linux内核版本**: 2.6.32-279.el6.x86_64 
* **gcc 版本**: 4.4.6
* **[libevent](http://libevent.org/)版本**: 2.0.21