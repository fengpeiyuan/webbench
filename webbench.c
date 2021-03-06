/*
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, fork failed
 * 
 */ 
#include "socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>

/* values */
volatile int timerexpired=0;
int speed=0;
int failed=0;
int bytes=0;
/* globals */
int http10=1; /* 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */
/* Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.5"
int method=METHOD_GET;
int clients=1;
int force=0;
int force_reload=0;
int proxyport=80;
char *proxyhost=NULL;
int benchtime=30;
/* internal */
int mypipe[2];
char host[MAXHOSTNAMELEN];
#define REQUEST_SIZE 2048
char request[REQUEST_SIZE];
/*ext:inject*/
int is_inject=0;
char inject[100];
int inject_from=0;
int inject_to=0;
const char delimeters[]="~";
char *tobe_repl="$";
/*ext:address to be set */
int is_address_tobe_set=0;
char address_tobe_set[100];
int address_part1_from=0;
int address_part1_to=0;
int address_part2_from=0;
int address_part2_to=0;
int address_part3_from=0;
int address_part3_to=0;
int address_part4_from=0;
int address_part4_to=0;
const char address_delimeters[]=".";
const char address_part_delimeters[]="~";
/*rate limit, unit:number/client/s */
int is_limitrate=0;
long limitrate;

static const struct option long_options[]=
{
 {"force",no_argument,&force,1},
 {"reload",no_argument,&force_reload,1},
 {"time",required_argument,NULL,'t'},
 {"help",no_argument,NULL,'?'},
 {"http09",no_argument,NULL,'9'},
 {"http10",no_argument,NULL,'1'},
 {"http11",no_argument,NULL,'2'},
 {"get",no_argument,&method,METHOD_GET},
 {"head",no_argument,&method,METHOD_HEAD},
 {"options",no_argument,&method,METHOD_OPTIONS},
 {"trace",no_argument,&method,METHOD_TRACE},
 {"version",no_argument,NULL,'V'},
 {"proxy",required_argument,NULL,'p'},
 {"clients",required_argument,NULL,'c'},
 {"inject",required_argument,NULL,'i'},
 {"address",required_argument,NULL,'a'},
 {"limitrate",required_argument,NULL,'l'},
 {NULL,0,NULL,0}
};

/* prototypes */
static void benchcore(const char* host,const int port,char *request);
static int bench(void);
static void build_request(const char *url);
static int get_random(int low,int high);
static char *malloc_replace(char *data, char *rep, char *to, int free_data);

static void alarm_handler(int signal)
{
   timerexpired=1;
}	

static void usage(void)
{
   fprintf(stderr,
	"webbench [option]... URL\n"
	"  -f|--force               Don't wait for reply from server.\n"
	"  -r|--reload              Send reload request - Pragma: no-cache.\n"
	"  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
	"  -p|--proxy <server:port> Use proxy server for request.\n"
	"  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
	"  -9|--http09              Use HTTP/0.9 style requests.\n"
	"  -1|--http10              Use HTTP/1.0 protocol.\n"
	"  -2|--http11              Use HTTP/1.1 protocol.\n"
	"  --get                    Use GET request method.\n"
	"  --head                   Use HEAD request method.\n"
	"  --options                Use OPTIONS request method.\n"
	"  --trace                  Use TRACE request method.\n"
	"  -?|-h|--help             This information.\n"
	"  -V|--version             Display program version.\n"
	"  -i|--inject              Inject dynamic parameter replace character '$' in url. The pattens like '10~100', 10(means 0~10) are supported.\n"
	"  -a|--address             Ip address parameter is set to http header:'X-Forwarded-For',address pattens like: 172~192.0~168.131~141.100~251, now only ipv4 is supported.\n"
    "  -l|--limitrate           Limit requests` rate by number per client per second, number/c/s.\n"
	);
};
int main(int argc, char *argv[])
{
 int opt=0;
 int options_index=0;
 char *tmp=NULL;

 if(argc==1)
 {
	  usage();
          return 2;
 } 

 while((opt=getopt_long(argc,argv,"912Vfrt:p:c:?hi:a:l:",long_options,&options_index))!=EOF )
 {
  switch(opt)
  {
   case  0 : break;
   case 'f': force=1;break;
   case 'r': force_reload=1;break; 
   case '9': http10=0;break;
   case '1': http10=1;break;
   case '2': http10=2;break;
   case 'V': printf(PROGRAM_VERSION"\n");exit(0);
   case 't': benchtime=atoi(optarg);break;	     
   case 'p': 
	     /* proxy server parsing server:port */
	     tmp=strrchr(optarg,':');
	     proxyhost=optarg;
	     if(tmp==NULL)
	     {
		     break;
	     }
	     if(tmp==optarg)
	     {
		     fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
		     return 2;
	     }
	     if(tmp==optarg+strlen(optarg)-1)
	     {
		     fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
		     return 2;
	     }
	     *tmp='\0';
	     proxyport=atoi(tmp+1);break;
   case ':':
   case 'h':
   case '?': usage();return 2;break;
   case 'c': clients=atoi(optarg);break;
   case 'i': strcpy(inject,optarg);
   	   	   char *inject_str=strdup(inject);
   	   	   char *from,*to;
   	   	   from=strsep(&inject_str,delimeters);
   	   	   if(from!=NULL)
   	   		   inject_from=atoi(from);
   	   	   to=strsep(&inject_str,delimeters);
   	   	   if(to!=NULL)
   	   		   inject_to=atoi(to);
   	   	   is_inject=1;
   	   	   break;
   case 'a': strcpy(address_tobe_set,optarg);
   	   	   char *address_tobe_set_s = strdup(address_tobe_set);
   	   	   char *address_part1_s,*address_part2_s,*address_part3_s,*address_part4_s;
   	   	   address_part1_s = strsep(&address_tobe_set_s,address_delimeters);
   	   	   /*printf("address_part1_s:%s",address_part1_s);*/
   	   	   if(address_part1_s!=NULL){
   	   		   char *from,*to;
   	   		   from=strsep(&address_part1_s,address_part_delimeters);
   	   		   if(from!=NULL) address_part1_from=atoi(from);
   	   		   to=strsep(&address_part1_s,address_part_delimeters);
   	   		   if(to!=NULL) address_part1_to=atoi(to);

   	   		   address_part2_s = strsep(&address_tobe_set_s,address_delimeters);
   	   	   }

   	   	   if(address_part2_s!=NULL){
				char *from,*to;
				from=strsep(&address_part2_s,address_part_delimeters);
				if(from!=NULL) address_part2_from=atoi(from);
				to=strsep(&address_part2_s,address_part_delimeters);
				if(to!=NULL) address_part2_to=atoi(to);

				address_part3_s = strsep(&address_tobe_set_s,address_delimeters);
   	   	   }

   	   	   if(address_part3_s!=NULL){
				char *from,*to;
				from=strsep(&address_part3_s,address_part_delimeters);
				if(from!=NULL) address_part3_from=atoi(from);
				to=strsep(&address_part3_s,address_part_delimeters);
				if(to!=NULL) address_part3_to=atoi(to);

   	   		   address_part4_s = strsep(&address_tobe_set_s,address_delimeters);
   	   	   }

   	   	   if(address_part4_s!=NULL){
				char *from,*to;
				from=strsep(&address_part4_s,address_part_delimeters);
				if(from!=NULL) address_part4_from=atoi(from);
				to=strsep(&address_part4_s,address_part_delimeters);
				if(to!=NULL) address_part4_to=atoi(to);

   	   	   }
   	   	   is_address_tobe_set=1;
   	   	   break;
   case 'l':
	   	   is_limitrate=1;
	   	   limitrate=atoi(optarg);
	   	   limitrate=limitrate*benchtime;
   	   	   break;

  }
 }
 
 if(optind==argc) {
                      fprintf(stderr,"webbench: Missing URL!\n");
		      usage();
		      return 2;
                    }

 if(clients==0) clients=1;
 if(benchtime==0) benchtime=30;
 /* Copyright */
 fprintf(stderr,"Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
	 "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
	 );
 /*ext:address tobe set*/
  if(is_address_tobe_set==1){
 	 printf("Ip address tobe set:%s.\n",address_tobe_set);
 	 printf("IP address range:%d~%d.%d~%d.%d~%d.%d~%d\n",address_part1_from,address_part1_to,address_part2_from,address_part2_to,address_part3_from,address_part3_to,address_part4_from,address_part4_to);
 	 if(address_part1_from<=0||address_part1_from>254||address_part1_to<=0||address_part1_to>254 \
 			||address_part2_from<0||address_part2_from>254||address_part2_to<0||address_part2_to>254 \
 			||address_part3_from<0||address_part3_from>254||address_part3_to<0||address_part3_to>254 \
 			||address_part4_from<0||address_part4_from>254||address_part4_to<0||address_part4_to>254){
 		fprintf(stderr,"Error! Input ip address range error. Range should be 0~255 . Modify patameters range now\n");
 		return 2;

 	 }
 	 if(address_part1_from>address_part1_to||address_part2_from>address_part2_to||address_part3_from>address_part3_to||address_part4_from>address_part4_to)
 	 {
 		 fprintf(stderr,"Error! Input ip address range error. Range should be from small to big and must be number. Modify patameters patten now\n");
 		 return 2;
 	 }
  }

 /*ext: limitrate*/
  if(is_limitrate==1&&limitrate==0){
	  fprintf(stderr,"Error! Input limitrate/-l error. Range must be number and more then zero. Modify patameters now\n");
  }

 /*build request*/
 build_request(argv[optind]);
 /*ext: Inject Dynimic Parameters*/
 if(is_inject==1){
	 printf("Inject Parameter Range:%s. From:%d,to:%d\n",inject,inject_from,inject_to);
	 if(inject_to<inject_from)
	 {
		 fprintf(stderr,"Error! Input inject parameters(use -i) range error,pattens should be like 1~100 or 100(means 0~100). Modify patameters patten now\n");
		 return 2;
	 }
 }

 /* print bench info */
 printf("\nBenchmarking: ");
 switch(method)
 {
	 case METHOD_GET:
	 default:
		 printf("GET");break;
	 case METHOD_OPTIONS:
		 printf("OPTIONS");break;
	 case METHOD_HEAD:
		 printf("HEAD");break;
	 case METHOD_TRACE:
		 printf("TRACE");break;
 }
 printf(" %s",argv[optind]);
 switch(http10)
 {
	 case 0: printf(" (using HTTP/0.9)");break;
	 case 2: printf(" (using HTTP/1.1)");break;
 }
 printf("\n");
 if(clients==1) printf("1 client");
 else
   printf("%d clients",clients);

 printf(", running %d sec", benchtime);
 if(force) printf(", early socket close");
 if(proxyhost!=NULL) printf(", via proxy server %s:%d",proxyhost,proxyport);
 if(force_reload) printf(", forcing reload");
 printf(".\n");
 return bench();
}

void build_request(const char *url)
{
  char tmp[10];
  int i;

  bzero(host,MAXHOSTNAMELEN);
  bzero(request,REQUEST_SIZE);

  if(force_reload && proxyhost!=NULL && http10<1) http10=1;
  if(method==METHOD_HEAD && http10<1) http10=1;
  if(method==METHOD_OPTIONS && http10<2) http10=2;
  if(method==METHOD_TRACE && http10<2) http10=2;

  switch(method)
  {
	  default:
	  case METHOD_GET: strcpy(request,"GET");break;
	  case METHOD_HEAD: strcpy(request,"HEAD");break;
	  case METHOD_OPTIONS: strcpy(request,"OPTIONS");break;
	  case METHOD_TRACE: strcpy(request,"TRACE");break;
  }
		  
  strcat(request," ");

  if(NULL==strstr(url,"://"))
  {
	  fprintf(stderr, "\n%s: is not a valid URL.\n",url);
	  exit(2);
  }
  if(strlen(url)>1500)
  {
         fprintf(stderr,"URL is too long.\n");
	 exit(2);
  }
  if(proxyhost==NULL)
	   if (0!=strncasecmp("http://",url,7)) 
	   { fprintf(stderr,"\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
             exit(2);
           }
  /* protocol/host delimiter */
  i=strstr(url,"://")-url+3;
  /* printf("%d\n",i); */

  if(strchr(url+i,'/')==NULL) {
                                fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
                                exit(2);
                              }
  if(proxyhost==NULL)
  {
   /* get port from hostname */
   if(index(url+i,':')!=NULL &&
      index(url+i,':')<index(url+i,'/'))
   {
	   strncpy(host,url+i,strchr(url+i,':')-url-i);
	   bzero(tmp,10);
	   strncpy(tmp,index(url+i,':')+1,strchr(url+i,'/')-index(url+i,':')-1);
	   /* printf("tmp=%s\n",tmp); */
	   proxyport=atoi(tmp);
	   if(proxyport==0) proxyport=80;
   } else
   {
     strncpy(host,url+i,strcspn(url+i,"/"));
   }
   // printf("Host=%s\n",host);
   strcat(request+strlen(request),url+i+strcspn(url+i,"/"));
  } else
  {
   // printf("ProxyHost=%s\nProxyPort=%d\n",proxyhost,proxyport);
   strcat(request,url);
  }
  if(http10==1)
	  strcat(request," HTTP/1.0");
  else if (http10==2)
	  strcat(request," HTTP/1.1");
  strcat(request,"\r\n");
  if(http10>0)
	  strcat(request,"User-Agent: WebBench "PROGRAM_VERSION"\r\n");
  if(proxyhost==NULL && http10>0)
  {
	  strcat(request,"Host: ");
	  strcat(request,host);
	  strcat(request,"\r\n");
  }
  if(force_reload && proxyhost!=NULL)
  {
	  strcat(request,"Pragma: no-cache\r\n");
  }
  if(http10>1)
	  strcat(request,"Connection: close\r\n");
  /*ext: add X-Forwarded-For header if address tobe set*/
  /*ext:address tobe set*/
   if(is_address_tobe_set==1){
	   strcat(request,"X-Forwarded-For: address_tobe_set\r\n");
   }
  /* add empty line at end */
  if(http10>0) strcat(request,"\r\n"); 
  /*printf("Req=%s\n",request);*/
}

/* vraci system rc error kod */
static int bench(void)
{
  int i,j,k;	
  pid_t pid=0;
  FILE *f;

  /* check avaibility of target server */
  i=Socket(proxyhost==NULL?host:proxyhost,proxyport);
  if(i<0) { 
	   fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
           return 1;
         }
  close(i);
  /* create pipe */
  if(pipe(mypipe))
  {
	  perror("pipe failed.");
	  return 3;
  }

  /* not needed, since we have alarm() in childrens */
  /* wait 4 next system clock tick */
  /*
  cas=time(NULL);
  while(time(NULL)==cas)
        sched_yield();
  */

  /* fork childs */
  for(i=0;i<clients;i++)
  {
	   pid=fork();
	   if(pid <= (pid_t) 0)
	   {
		   /* child process or error*/
	           sleep(1); /* make childs faster */
		   break;
	   }
  }

  if( pid< (pid_t) 0)
  {
          fprintf(stderr,"problems forking worker no. %d\n",i);
	  perror("fork failed.");
	  return 3;
  }

  if(pid== (pid_t) 0)
  {
    /* I am a child */
    if(proxyhost==NULL)
      benchcore(host,proxyport,request);
         else
      benchcore(proxyhost,proxyport,request);

         /* write results to pipe */
	 f=fdopen(mypipe[1],"w");
	 if(f==NULL)
	 {
		 perror("open pipe for writing failed.");
		 return 3;
	 }
	 /* fprintf(stderr,"Child - %d %d\n",speed,failed); */
	 fprintf(f,"%d %d %d\n",speed,failed,bytes);
	 fclose(f);
	 return 0;
  } else
  {
	  f=fdopen(mypipe[0],"r");
	  if(f==NULL) 
	  {
		  perror("open pipe for reading failed.");
		  return 3;
	  }
	  setvbuf(f,NULL,_IONBF,0);
	  speed=0;
          failed=0;
          bytes=0;

	  while(1)
	  {
		  pid=fscanf(f,"%d %d %d",&i,&j,&k);
		  if(pid<2)
                  {
                       fprintf(stderr,"Some of our childrens died.\n");
                       break;
                  }
		  speed+=i;
		  failed+=j;
		  bytes+=k;
		  /* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
		  if(--clients==0) break;
	  }
	  fclose(f);

  printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
		  (int)((speed+failed)/(benchtime/60.0f)),
		  (int)(bytes/(float)benchtime),
		  speed,
		  failed);
  }
  return i;
}

void benchcore(const char *host,const int port,char *req)
{
 int rlen;
 char buf[1500];
 int s,i;
 struct sigaction sa;

 /*ext:limitrate*/
 long limitrate_process=0;
 struct timeval tv_start;
 struct timezone tz_start;
 gettimeofday(&tv_start,&tz_start);

 /* setup alarm signal handler */
 sa.sa_handler=alarm_handler;
 sa.sa_flags=0;
 if(sigaction(SIGALRM,&sa,NULL))
    exit(3);
 alarm(benchtime);

 nexttry:while(1)
 {
	 /*ext:limitrate*/
	 if(is_limitrate==1){
		 struct timeval tv_now;
		 struct timezone tz_now;
		 gettimeofday(&tv_now,&tz_now);
		 long sec_diff = tv_now.tv_sec-tv_start.tv_sec;
		 long limitrate_process_step=(limitrate/benchtime)*(sec_diff+1);
		 //printf("sec_diff: %ld,step:%ld\n", sec_diff,limitrate_process_step);
		 if(limitrate_process>=limitrate){
			 break;
		 }
		 if(limitrate_process>=limitrate_process_step){
			 usleep(1000);//1ms
			 continue;
		 }
		 limitrate_process++;
	 }


	 char *req_repl=req;
	/*ext:inject here*/
	 if(is_inject==1){
		int ran = get_random(inject_from,inject_to);
		char repl[255];
		sprintf(repl,"%d",ran);
		req_repl=malloc_replace(req_repl,tobe_repl,repl,0);

	 }

	/*ext:address tobe set */
	if(is_address_tobe_set==1){
		   int address_part1_random = get_random(address_part1_from,address_part1_to);
		   int address_part2_random = get_random(address_part2_from,address_part2_to);
		   int address_part3_random = get_random(address_part3_from,address_part3_to);
		   int address_part4_random = get_random(address_part4_from,address_part4_to);
		   char address_random[255];
		   sprintf(address_random,"X-Forwarded-For: %d.%d.%d.%d\r\n",address_part1_random,address_part2_random,address_part3_random,address_part4_random);
		   req_repl=malloc_replace(req_repl,"address_tobe_set",address_random,0);
	   }


	rlen=strlen(req_repl);
    if(timerexpired)
    {
       if(failed>0)
       {
          /* fprintf(stderr,"Correcting failed by signal\n"); */
          failed--;
       }
       return;
    }
    s=Socket(host,port);                          
    if(s<0) { failed++;continue;} 
    if(rlen!=write(s,req_repl,rlen)) {failed++;close(s);continue;}
    if(http10==0) 
	    if(shutdown(s,1)) { failed++;close(s);continue;}
    if(force==0) 
    {
            /* read all available data from socket */
	    while(1)
	    {
              if(timerexpired) break; 
	      i=read(s,buf,1500);
              /* fprintf(stderr,"%d\n",i); */
	      if(i<0) 
              { 
                 failed++;
                 close(s);
                 goto nexttry;
              }
	       else
		       if(i==0) break;
		       else
			       bytes+=i;
	    }
    }
    if(close(s)) {failed++;continue;}
    speed++;


 }
}

/**
 * @param low
 * @param high
 * @return [low,high]
 */
static int get_random(int low,int high)
{
	return (rand()%(high-low+1))+low;
}

/**
 * 统计key在data中出现的次数
 * @param data 待查找的字符串
 * @param key  要查找的字符串
 * @return key在data中出现的次数
 */
int _count_string(char *data, char *key)
{
    int count = 0;
    int klen = strlen(key);
    char *pos_start = data, *pos_end;
    while (NULL != (pos_end = strstr(pos_start, key))) {
        pos_start = pos_end + klen;
        count++;
    }
    return count;
}


/**
 * 将data中的rep字符串替换成to字符串，以动态分配内存方式返回新字符串
 * 这个函数不需要保证data能保证容量。
 * @param data 待替换某些字符串的数据
 * @param rep  待替换的字符串
 * @param to   替换成的字符串
 * @param free_data 不为0时要释放data的内存
 * @return 返回新分配内存的替换完成的字符串，注意释放。
 */
static char *malloc_replace(char *data, char *rep, char *to, int free_data)
{
    int rep_len = strlen(rep);
    int to_len  = strlen(to);
    int counts  = _count_string(data, rep);
    int m = strlen(data) + counts * (to_len - rep_len);
    char *new_buf = (char *) malloc(m + 1);
    if (NULL == new_buf) {
        free(data);
        return NULL;
    }
    memset(new_buf, 0, m + 1);
    char *pos_start = data, *pos_end, *pbuf = new_buf;
    int copy_len;
    while (NULL != (pos_end = strstr(pos_start, rep))) {
        copy_len = pos_end - pos_start;
        strncpy(pbuf, pos_start, copy_len);
        pbuf += copy_len;
        strcpy(pbuf, to);
        pbuf += to_len;
        pos_start  = pos_end + rep_len;
    }
    strcpy(pbuf, pos_start);
    if (free_data)
        free(data);
    return new_buf;
}
