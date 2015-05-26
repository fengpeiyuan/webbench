# Fork of webbench, Additional options

* Modify requests' url parameter randomly in a range, on every request send.
* Modify different ip address(change http X-Forwarded-For header and only ipv4 support) randomly in a range, on every request send.
	webserver like nginx can receive so many requests in different ip address, if you use --with-http_realip_module option and config like this:
```nginx
set_real_ip_from  192.168.1.0/24;(for example)
real_ip_header    X-Forwarded-For;
```
* Change/limit requests' rate, united per connection per second.

## Usage

* Use -i(or --injection) option,parameter pattens should be like 1~100 or 100(means 0~100)
  Following commend is sample for this 
	
			webbench -c 5 -t 60 -i 1~200 "http://uri:port/action?param=$&callback=cbk"
* Use -a(or --address) option,parameter pattens should be like 1~254.0~254.0~254.0~254
  Following commend is sample for this 
	
			webbench -c 5 -t 60 -a 1~254.0~254.0~254.0~254 "http://url"
* Use -l(or --limitrate) option.

			webbench -c 5 -t 60 -l 10 "http://url"
	parameters like below means 3000 requests total, 600 requests/process, 10 requests/process/second

## License

Copyright (c) 2014-2015, Peiyuan Feng <fengpeiyuan@gmail.com>.

This module is licensed under the terms of the BSD license.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
