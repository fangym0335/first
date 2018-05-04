#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#define FIELD_LEN 250
#define NV_PAIRS 200
#define DEFAULT_WORK_DIR_4CGI  "/home/App/ifsf/"  //前庭控制器的默认工作目录,用于GetWorkDir()函数.
#define RUN_WORK_DIR_FILE  "/var/run/ifsf_main.wkd"  //应用程序运行目录文件,用于GetWorkDir()函数.
#define DEFAULT_WORK_DIR  "/home/App/ifsf/"  //前庭控制器的默认工作目录,用于GetWorkDir()函数.

typedef struct name_value_st{
char name[FIELD_LEN + 1];
char value[FIELD_LEN + 1];
}NAME_VALUE;

NAME_VALUE nv[NV_PAIRS];
int getCgiInput();
static void makespace(char *s);
static char *split(char *s, char stop);
static char *convert(char *s);
static int hexchr2hex(char c);
static void internal_error(const  char* reason );
static void cgi_error(const  char* reason );
static void cgi_ok(const  char* reason );
static void  cgi_refresh( char* htmltable);
//static void cgi_redirect(const  char* url );
static void moved( char* script_name, char* url );
static int ReadHtmlFile(const char *sFileName,  char *formbuff);
static int ReadTextFile(const char *sFileName,  char *formbuff);
static int ReadALine( FILE *f, char *s, int MaxLen );
static int ReadALine2( FILE *f, char *s);
static int IsTheFileExist(const  char * sFileName );
static int getbcdtime(char *my_bcd);
static char *TrimAll( char *buf );
static int SaveUserTime(const char *username);
static int   GetUserTime( char *bcd_time, const char *username);
static int CheckUserTime(const int validmin, const char *checkname);
static int convert2printlist(char *scrbuff,char *destbuff, int sizebuff);
static int quot2list(char *scrbuff,char *destbuff, int sizebuff);

static int  GetWorkDir( char *workdir) ;


#define CGIDECODER
//get cgi input to nv[],  and return field count.
int getCgiInput(){
	char *method;
	char * my_data = 0;
	char *tmp_ptr, *tmp;
	int data_len;
	int i;

	method = getenv("REQUEST_METHOD");
	if(NULL == method) {
		cgi_error("NULL == method!!");
		exit(1);
	}	
	//GET method:
	if (strcmp(method, "GET") == 0){		
		tmp_ptr = getenv("QUERY_STRING");
		if(NULL == tmp_ptr) {
			return 0;
		}	
		data_len = strlen(tmp_ptr);
		my_data = (char *)malloc( sizeof(char) * (data_len + 1));
		if(NULL == my_data ){
			cgi_error("NULL == my_data!!");
			exit(1);
			//return -1;
		}		
		strcpy(my_data, getenv("QUERY_STRING"));
		my_data[data_len] = '\0';
		
	}
	//POST method
	else if (strcmp(method, "POST") == 0){
		tmp_ptr = getenv("CONTENT_LENGTH");
		if(NULL == tmp_ptr) {
			return 0;
		}
		data_len = atoi(tmp_ptr);
		my_data = (char *)malloc( sizeof(char) * (data_len + 1));
		if(NULL == my_data ){
			cgi_error("NULL == my_data!!");
			exit(1);
			//return -1;
		}
		fread(my_data,1, data_len, stdin);
	}
	else{
		cgi_error("not GET or POST error!!");
		exit(1);
		//return -1;
	}
	bzero(&(nv[0].name), sizeof(NAME_VALUE)*NV_PAIRS);	
	i = 0;
	while (my_data[0] !='\0'){
		tmp = split(my_data, '=');// split data to get field name.
		makespace(tmp); //convert back space
		tmp = convert(tmp); //convert char like chinese.
		bzero(&(nv[i].name), sizeof(nv[i].name));
		strcpy(nv[i].name, tmp);
		tmp = split(my_data, '&');// split data to get value.
		makespace(tmp); //convert back space
		tmp = convert(tmp); //convert char like chinese.
		bzero(&(nv[i].value), sizeof(nv[i].value));
		strcpy(nv[i].value, tmp);
		i ++;
	}
	free(my_data);
	return i--;	
}

// replace '+' with space.
static void makespace(char * s){
	int i, len;
	len = strlen(s);
	for(i =0; i < len; i ++){
		if (s[i] == '+'){
			s[i] = ' ';
		}
	}
}

//split s with stop, return the begin of s to the first found char, stop. 
static char *split(char *s, char stop){
	char *data;
	char *tmp;
	int i, len, j;

	len = strlen(s);
	tmp = s;
	data = (char *)malloc( sizeof(char) * (len + 1));
	if(NULL == data ){
		return NULL;
	}

	for(i =0; i < len; i ++){
		if (s[i] != stop){
			data[i] = s[i] ;
		}
		else {
			i ++;
			break;
		}
	}
	data[i] = '\0';
	for(j = i; j < len; j++) 
		s[j - i] = tmp[j];
	s[len -i] = '\0';
	return data;
}

//convert char  like chinese, +, -, :,  to hex.
static char *convert( char *s){
	int  i,j,len;
	char *data;

	len = strlen(s);
	data = (char *)malloc( sizeof(char) * (len + 1));
	if(NULL == data ){
		return NULL;
	}
	j = 0;
	for(i =0; i < len; i ++){
		if (s[i] != '%'){
			data[j] = s[i] ;
			j++;
		}
		else {
			data[j] = (char) ( 16 * hexchr2hex(s[i+1]) + hexchr2hex(s[i+2]));
			j++;
			i += 2;
		}
	}
	data[j] = '\0';
	return data;	
}

// change a char to int(0-15). if it not a hex char, return 0;  
static int hexchr2hex(char c){
	switch(c){
		case '0': return 0;
		case '1': return 1;
		case '2': return 2;
		case '3': return 3;
		case '4': return 4;
		case '5': return 5;
		case '6': return 6;
		case '7': return 7;
		case '8': return 8;
		case '9': return 9;
		case 'a': return 10;
		case 'A': return 10;
		case 'b': return 11;
		case 'B': return 11;
		case 'c': return 12;
		case 'C': return 12;
		case 'd': return 13;
		case 'D': return 13;
		case 'e': return 14;
		case 'E': return 14;
		case 'f': return 15;
		case 'F': return 15; 
		
	}
	return 0;
}
static void internal_error(const  char* reason )
    {
    char* title = "500 Internal Error";

    (void) printf( "\
<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n\
<BODY  BGCOLOR=#99cc99 TEXT=#000000 LINK=#2020ff VLINK=#4040cc><H2>%s</H2>\n\
Something unusual went wrong during the request:\n\
<BLOCKQUOTE>\n\
%s\n\
</BLOCKQUOTE>\n\
</BODY></HTML>\n",  title, title, reason );
    }
static void cgi_error(const  char* reason )
    {
    char* title = "500 Internal Error";

    (void) printf( "\
<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n\
<BODY   BGCOLOR=#99cc99 TEXT=#000000 LINK=#2020ff VLINK=#4040cc><H2>%s</H2>\n\
Something unusual went wrong during excute cgi  for  the request:\n\
<BLOCKQUOTE>\n\
%s\n\
</BLOCKQUOTE>\n\
</BODY></HTML>\n",  title, title, reason );
    }
static void cgi_ok(const  char* reason )
    {
    char* title = "cgi ok!";

    (void) printf( "\
<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n\
<BODY    BGCOLOR=#99cc99 TEXT=#000000 LINK=#2020ff VLINK=#4040cc><H2>%s</H2>\n\
Nothing  went wrong during excute cgi  for  the request:\n\
<BLOCKQUOTE>\n\
%s\n\
</BLOCKQUOTE>\n\
</BODY></HTML>\n",  title, title, reason );
    }
static void  cgi_refresh( char* htmltable)
    {
    (void) printf( "\
Content-type: text/html\n\
\n\
<HTML><HEAD><TITLE>Refresh test</TITLE></HEAD>\n\
<meta http-equiv=refresh content=3>\n\
<BODY><H2>Refresh Page</H2>\n\
 %s \n\
</BODY></HTML>\n", htmltable );
    }
/*
static void cgi_redirect(const  char* url )
    {
    char* title = "cgi ok!";

    (void) printf( "\
<HTML> <HEAD><TITLE>%s</TITLE><meta http-equiv=refresh content=3 url=%s></HEAD>\n\
<BODY   onunload=\"document.location.href=%s\"   BGCOLOR=#99cc99 TEXT=#000000 LINK=#2020ff VLINK=#4040cc><H2>%s</H2>\n\
do cmd ok  for  the request:\n\
<BLOCKQUOTE>\n\
waiting...\n\
</BLOCKQUOTE>\n\
</BODY></HTML>\n", title, url, url , title);
    }

static void not_found( char* script_name )
    {
    char* title = "404 Not Found";

    (void) printf( "\
<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n\
<BODY   BGCOLOR=#99cc99 TEXT=#000000 LINK=#2020ff VLINK=#4040cc><H2>%s</H2>\n\
The requested filename, %s, is set up to be redirected to another URL;\n\
however, the new URL has not yet been specified.\n\
</BODY></HTML>\n",  title, title, script_name );
    }

*/
static void
moved( char* script_name, char* url )
    {
    char* title = "Moved";

    (void) printf( "\
Location: %s\n\
Content-type: text/html\n\
\n\
<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n\
<BODY><H2>%s</H2>\n\
The requested filename, %s, has moved to a new URL:\n\
<A HREF=\"%s\">%s</A>.\n\
</BODY></HTML>\n", url, title, title, script_name, url, url );
    }
	
static int ReadHtmlFile(const char *sFileName,  char *formbuff){
	struct stat statbuf;
	int iRet;
	int fd;
	long i,len,ret;
	int READSIZE = 16;
	//char *sFileName = "pcdform.txt";
	char *buff;
	extern int errno;

	if(  (NULL == sFileName) || (NULL == formbuff) ){
		return -1;
	}
	//get file size to len.
	iRet = stat( sFileName, &statbuf );
	if( iRet == -1 ){
		if( errno == ENOENT ){	/*无此文件*/		
			char pbuff[256];
			bzero(pbuff,sizeof(pbuff));
			sprintf(pbuff, "[%d] error [%s]",errno, strerror( errno ) );
			cgi_error(pbuff);
			//return -2;
			exit(1);
		}
		else {	
			char pbuff[256];
			bzero(pbuff,sizeof(pbuff));
			sprintf(pbuff, "[%d] error [%s]",errno, strerror( errno ) );
			cgi_error(pbuff);
			//return -1;
			exit(1);
		}
	}
	len =  statbuf.st_size;
	
	//read form html file to formnuff.
	fd = open( sFileName, O_RDONLY);
	if( fd < 0 ){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff,"打开配置文件[%s]失败.\n", sFileName );
		cgi_error(pbuff);
		//return -1;
		exit(1);
	}
	buff = formbuff;
	i = len;
	//sometime len is 0 when i test it, the reson i don't know, so i add if else.
	if(len >0){
		while( 1 )
		{	
			if(i <= READSIZE){ //last read.
				ret = read( fd, buff, i );
				if(errno == EINTR){
				continue;
				}
				else if( ret < 0 ){	//出错//		
					cgi_error( "读取HtmlFile 文件尾有误" );
					close(fd);
					//return -1;
					exit(1);
				}
				if( ret != i  ){
					cgi_error( "读取HtmlFile 文件尾有误" );
					close( fd );
					//return -1;
					exit(1);
				}
				break;//all read out.
			}
			ret = read( fd, buff, READSIZE );		
			if( ret == 0 )		//文件结束//
				break;
			else if(errno == EINTR){
				continue;
			}
			else if( ret < 0 ){	//出错//		
				cgi_error( "读取HtmlFilet 文件有误" );
				close(fd);
				//return -1;
				exit(1);
			}
			if( ret != READSIZE ){
				cgi_error( "读取HtmlFfile 文件有误" );
				close( fd );
				//return -1;
				exit(1);
			}
			buff  = buff + READSIZE;
			i  = i - READSIZE;
		}
	}	
	else{
		while( 1 )
		{	
			
			ret = read( fd, buff, READSIZE );		
			if( ret == 0 )		//文件结束//
				break;
			else if(errno == EINTR){				
				cgi_error( "读取HtmlFfilet 文件被中断" );
				close(fd);
				//return -1;
				exit(1);
				continue;
			}
			else if( ret < 0 ){	//出错//		
				cgi_error( "读取HtmlFfilet 文件出错" );
				close(fd);
				//return -1;
				exit(1);
			}
			if( ret < READSIZE ){
				break;//文件结束?
			}
			buff  = buff + READSIZE;
			//i  = i - READSIZE;
		}
	}
	
	close(fd);
	//memcpy(formbuff,outbuff,sizeof(outbuff));
	return 0;
}	

static int ReadTextFile(const char *sFileName,  char *formbuff){
	//struct stat statbuf;
	int iRet;
	FILE *f;
	long i;
	char *buff;
	int c;
	if(  (NULL == sFileName) || (NULL == formbuff) ){
		return -1;
		}
	iRet = access( sFileName, F_OK);
	if( iRet < 0 ){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff,"文件[%s]不存在.\n", sFileName );
		cgi_error(pbuff);
		//return -1;
		exit(1);
	}
	
	//read text file to formbuff.
	f = fopen( sFileName, "rt");
	if( !f  ){
		char pbuff[256];
		bzero(pbuff,sizeof(pbuff));
		sprintf( pbuff,"打开文件[%s]失败.\n", sFileName );
		cgi_error(pbuff);
		//return -1;
		exit(1);
	}
	
	buff = formbuff;	
	i = 0;
	while( 1 )
	{	
		c = fgetc( f);		
		if( c == EOF )
			break;
		buff[i] = c;
		i++;
	}
	buff[i] = 0;
	
	fclose(f);
	//memcpy(formbuff,buffer,strlen(buffer));
	return i;
}	


static int ReadALine( FILE *f, char *s, int MaxLen ){
	int i, c;

	for( i = 0; i < MaxLen; i++ )
	{
		c = fgetc( f);
		if( c == '\t' )
			break;//tab is also the end.
		if( c == EOF || c == '\n' )
			break;
		s[i] = c;
	}

	s[i] = 0;

	if( i == MaxLen )
	{
		for( ; ; )
		{
			c = fgetc(f);
			if( c == EOF || c == '\n' )
				break;
		}
	}
	if( c == EOF )
		return 1;
	else
		return 0;
}

static int ReadALine2( FILE *f, char *s){
	int i, c;
	i=0;
	while( 1)
	{
		c = fgetc( f);
		if( c == '\t' )
			break;//tab is also the end.
		if( c == EOF || c == '\n' )
			break;
		s[i] = c;
		i++;
	}

	s[i] = 0;	
	if( c == EOF )
		return 1;
	else
		return 0;
}

static int IsTheFileExist(const char * sFileName ){
	int ret;
	extern int errno;
	if( sFileName == NULL || *sFileName == 0 )
		return -1;
	ret = access( sFileName, F_OK );
	//cgi_ok("in  IsTheFileExist after call stat");
	//exit( 0 );
	if( ret < 0 )
	{
		if( errno == ENOENT )
		{
			return 0;//not exist
		}
		else
		{			
			return -1;
		}
		
	}
	return 1;
}

static int getbcdtime(char *my_bcd){
		char tmp[4];
		time_t result;
		struct tm *my_time;
		time(&result);
		my_time = gmtime(&result);
		my_time->tm_year += 1900;
		my_time->tm_mon += 1;

		my_bcd[0] = my_time->tm_year /1000;		//year
		my_time->tm_year -= my_bcd[0]*1000;
		my_bcd[1] = my_time->tm_year /100;
		my_time->tm_year -= my_bcd[1] *100;
		my_bcd[2] = my_time->tm_year /10;
		my_time->tm_year -= my_bcd[2] *10;
		my_bcd[3] = my_time->tm_year %10;
		my_bcd[0] = my_bcd[0] << 4;
		my_bcd[0] = my_bcd[0] | my_bcd[1];
		my_bcd[2] = my_bcd[2] << 4;
		my_bcd[1] = my_bcd[2] | my_bcd[3];

		tmp[0] = my_time->tm_mon / 10;				//month
		tmp[1] = my_time->tm_mon % 10;
		tmp[0] = tmp[0] <<4;
		tmp[3] = tmp[0] | tmp[1];
		my_bcd[2] = tmp[3];

		tmp[0] = my_time->tm_mday / 10;				//day
		tmp[1] = my_time->tm_mday % 10;
		tmp[0] = tmp[0] <<4;
		tmp[3] = tmp[0] | tmp[1];
		my_bcd[3] = tmp[3];

		tmp[0] = my_time->tm_hour / 10;				//hour
		tmp[1] = my_time->tm_hour % 10;
		tmp[0] = tmp[0] <<4;
		tmp[3] = tmp[0] | tmp[1];
		my_bcd[4] = tmp[3];

		tmp[0] = my_time->tm_min / 10;				//minute
		tmp[1] = my_time->tm_min % 10;
		tmp[0] = tmp[0] <<4;
		tmp[3] = tmp[0] | tmp[1];
		my_bcd[5] = tmp[3];

		tmp[0] = my_time->tm_sec / 10;				//second
		tmp[1] = my_time->tm_sec % 10;
		tmp[0] = tmp[0] <<4;
		tmp[3] = tmp[0] | tmp[1];
		my_bcd[6] = tmp[3];
//printf("my year is : %x %x %x %x %x %x %x\n",my_bcd[0],my_bcd[1],my_bcd[2],my_bcd[3],my_bcd[4],my_bcd[5],my_bcd[6]);
		return 0;
}
//-
//去除字符串两侧空格及回车换行符,需要字符串带结束符
//-
static char *TrimAll( char *buf )
{
	int i,firstchar,endpos,firstpos;

	if(  NULL  ==buf){
		return NULL;
		};

	endpos = firstchar = firstpos = 0;
	for( i = 0; buf[ i ] != '\0'; i++ )
	{
		if( buf[i] ==' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\r')
		{
			if( firstchar == 0 )
				firstpos++;
		}
		else
		{
			endpos = i;
			firstchar = 1;
		}
	}

	for( i = firstpos; i <= endpos; i++ )
		buf[i - firstpos] = buf[i];
	buf[i - firstpos] = 0;
	return buf;
}
//-
//save current time to filename is value of username and extend of  filename is "time". 
//-
static int SaveUserTime(const char *username)
{
	int fd;
	int ret;
	char temp[128];
	char bcd_time[8];
	//char names[] = "system";
	/*打开文件*/
	bzero(temp,sizeof(temp));
	if( (NULL != username) && (username[0] != 0) )
		sprintf(temp,"/var/run/%s.time",username);
	else
		strcpy(temp,"/var/run/username.time");
	
	fd = open( temp, O_WRONLY|O_CREAT|O_TRUNC, 00644 );
	if( fd < 0 )
	{
		//UserLog( "打开当前运行工作路径信息文件[%s]失败.", "/var/run/ifsf_main.wkd" );
		return -1;
	}
	bzero(bcd_time,sizeof(bcd_time));
	ret = getbcdtime(bcd_time);
	write( fd, bcd_time, 7 );//strlen(bcd_time)
	close(fd);
	return 0;
}

static int   GetUserTime( char *bcd_time, const char *username) {
	FILE *f;
	int i, r,c;
	char s[96] ;
	bzero(s,sizeof(s));
	if( (NULL != username) && (username[0] != 0) )
		sprintf(s,"/var/run/%s.time",username);
	else
		strcpy(s,"/var/run/username.time");
	r = IsTheFileExist(s);
	if(r != 1){
		return -2;
	}
	f = fopen( s, "rt" );
	if( !f  ){	
		return -3;
	}	
	bzero(s, sizeof(s));
	for(i=0;i<7;i++){
		c = fgetc( f);		
		s[i] = c;
	}
	s[i] = 0;			
	if( (i >= 6) &&(s[0] != 0) ){
		memcpy(bcd_time, s, 7);
	}
	else{
		fclose( f );
		return -1;
	}
	fclose( f );
	return 0;
}
//only in 1-59 min is valid, check time and ghange time to current.
static int CheckUserTime(const int validmin, const char *checkname){
	int ret;
	//char username[20];
	char bcd_time[8];
	char bcd_time_c[8];
	int min = validmin;
	
	
	if(min < 1)
		min =1;
	else if(min > 59 )
		min = 59;
	bzero(bcd_time,sizeof(bcd_time));
	if( (NULL != checkname) && (checkname[0] != 0) ){
		ret = GetUserTime(bcd_time, checkname);
		if(ret < 0){
			//cgi_error(" ret = GetUserTime(bcd_time, checkname)");
			//exit(1);
			return -2;
		}
	}
	else{
		ret = GetUserTime(bcd_time, NULL);
		if(ret < 0){
			//cgi_error(" GetUserTime(bcd_time, NULL)");
			//exit(1);
			return -2;
		}
	}
	bzero(bcd_time_c,sizeof(bcd_time_c));	
	ret = getbcdtime(bcd_time_c);	
	if(    memcmp(bcd_time_c, bcd_time, 5) == 0   ){
		if(    ( (unsigned int)bcd_time_c[5] - (unsigned int)bcd_time[5] ) <= (min -1)  ){
			ret = SaveUserTime(checkname);
			return 0;			
		}
		else{
			//cgi_error(" ( (unsigned int)bcd_time_c[5] - (unsigned int)bcd_time[5] ) <= (min -1)  error");
			//exit(1);
			return -2;
		}
	}
	//cgi_error(" CheckUserTime  error at end");
	//exit(1);
	return -1;	
	
}


static int convert2printlist(char *scrbuff,char *destbuff, int sizebuff){
	int i,j;
	if(   (NULL== scrbuff)||  (NULL== destbuff) ||0 == sizebuff){
		return -1;
		}
	i = 0;
	j = 0;
	while(j<sizebuff-1){
		destbuff[j] = scrbuff[i];
		if(scrbuff[i] =='%'){
			memcpy(&destbuff[i],"&#37" ,4);
			j +=4;
			}
		else if(scrbuff[i] == (char)34 ){  // 双引号"
			memcpy(&destbuff[i],"&#34" ,4);
			j +=4;
			}	
		i++;
		j++;
	}
	return 0;
}
static int quot2list(char *scrbuff,char *destbuff, int sizebuff){
	int i,j;
	if(   (NULL== scrbuff)||  (NULL== destbuff) ||0 == sizebuff){
		return -1;
		}
	i = 0;
	j = 0;
	while(j<sizebuff-1){
		destbuff[j] = scrbuff[i];
		if(scrbuff[i] == (char)34 ){  // 双引号"
			memcpy(&destbuff[i],"&#34" ,4);
			j +=4;
			}	
		i++;
		j++;
	}
	return 0;
}









static int   GetWorkDir( char *workdir) {
	FILE *f;
	int i, r,trytime = 3;
	char s[96] ;

	if(  workdir == NULL ){		
		return -1;
	}	
	f = fopen( RUN_WORK_DIR_FILE, "rt" );
	if( ! f ){	// == NULL
		memcpy( workdir,DEFAULT_WORK_DIR, strlen(DEFAULT_WORK_DIR) );
		return 0;
	}	
	bzero(s, sizeof(s));
	for(i=0; i<trytime; i++){
		r = ReadALine( f, s, sizeof(s) );		
		if( s[0] == '/' )
			break;
	}
	fclose( f );
	if(s[0] == '/' ){
		memcpy(workdir,s, strlen(s));	
		return 0;
	}
	else{
		memcpy( workdir,DEFAULT_WORK_DIR, strlen(DEFAULT_WORK_DIR) );
		return 0;
	}
}


