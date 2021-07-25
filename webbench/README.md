压测
===============
Webbench是有名的网站压力测试工具，它是由[Lionbridge](http://www.lionbridge.com)公司开发。

> * 测试处在相同硬件上，不同服务的性能以及不同硬件上同一个服务的运行状况。
> * 展示服务器的两项内容：每秒钟响应请求数和每秒钟传输数据量。




测试规则
------------
* 测试示例

    ```C++
	webbench -c 500  -t  30   http://127.0.0.1/phpionfo.php
    ```
* 参数

> * `-c` 表示客户端数
> * `-t` 表示时间


======== test result ==========


1、log = asyn web = Proactor listen = LT conn = LT;
shallwe@ubuntu:~/XW_projection/web_server/web/webbench/webbench-1.5$ ./webbench -c 11000 -t 20 http://192.168.197.132:9999/
Webbench - Simple Web Benchmark 1.5
Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

Benchmarking: GET http://192.168.197.132:9999/
11000 clients, running 20 sec.

Speed=531909 pages/min, 5779980 bytes/sec.
Requests: 177303 susceed, 0 failed.



2、log = asyn web = Reactor listen = LT conn = ET
shallwe@ubuntu:~/XW_projection/web_server/web/webbench/webbench-1.5$ ./webbench -c 11000 -t 20 http://192.168.197.132:9999/
Webbench - Simple Web Benchmark 1.5
Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

Benchmarking: GET http://192.168.197.132:9999/
11000 clients, running 20 sec.

Speed=3330 pages/min, 38924 bytes/sec.
Requests: 1110 susceed, 0 failed.















