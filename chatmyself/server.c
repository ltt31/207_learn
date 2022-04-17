#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sqlite3.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include"tpool.h"
 
#define SIZE 20    //定义最大在线人数是20
#define PORT 8888 //定义端口号
#define TRUE   1
#define FALSE -1
 
struct Usr     //定义结构体，成员变量包括姓名、套接口描述符和禁言标志
{
	char name[SIZE];
	int socket;
	int flag;//用来判断该用户是否被禁言，没有被禁设为0，被禁设为1
};
 
struct Msg  //定义在线结构体
{
	struct Usr usr[SIZE];//定义多个结构体变量
	char msg[1024];  //保存消息内容
	char buf[1024];
	char name[SIZE];
	char fromname[SIZE];
	char toname[SIZE];
	char password[SIZE];
	int cmd;      //消息动作位标志
	int filesize;  //文件大小
	int flag;    //用来判断用户权限   0代表普通用户，1代表超级用户
};
 
struct Usr usr[SIZE];//定义多个结构体变量，存放在线用户列表
int count ;//全局变量不初始化默认为0，统计在线人数
 
pthread_mutex_t mutex; //互斥锁，用来避免两个客户端同时访问全局变量

//查找用户名
int find_name(struct Msg *msg)
{
	//定义指向数据库的指针
	sqlite3 * database;
	//打开用户数据库
	int ret = sqlite3_open("usr.db", &database);
	if (ret != SQLITE_OK)
	{
		printf ("打开数据库失败\n");
		return FALSE;
	}
	
	//遍历查找用户名
	char *errmsg = NULL;
	char **resultp = NULL;
	int nrow, ncol;
	char *sql = "select name from usr;";
	ret = sqlite3_get_table(database, sql, &resultp, &nrow, &ncol, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("用户查找失败：%s\n", errmsg);
		return FALSE;
	}
	
	int i;
	for(i = 0 + ncol; i < (nrow + 1)*ncol; i += ncol)
	{
		if(strcmp(resultp[i], msg->name) == 0)
		{
			return TRUE;
		}
			
	}
	return FALSE;
}

//查找用户名和密码
int find_np(struct Msg *msg)
{
	//打开数据库
	sqlite3 * database;
	int ret = sqlite3_open("usr.db", &database);
	if (ret != SQLITE_OK)
	{
		printf ("打开数据库失败\n");
		return FALSE;
	}
	
	//遍历查找对应的用户名和数据库是否和输入的吻合
	char *errmsg = NULL;
	char **resultp = NULL;
	int nrow, ncolumn;
	char *sql = "select * from usr;";
	ret = sqlite3_get_table(database, sql, &resultp, &nrow, &ncolumn, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("用户查找失败：%s\n", errmsg);
		return FALSE;
	}
	
	int i;
	for(i = 0; i < (nrow + 1)*ncolumn; i++)
	{
		if(strcmp(resultp[i], msg->name) == 0 &&
		strcmp(resultp[i + 1], msg->password) == 0)
		
		return TRUE;
	}
	return FALSE;
}

//检查该用户是否在线
int check_ifonline(struct Msg *msg)
{
	int i;
	for(i = 0; i < count; i++)
	{
		if(strcmp(msg->name, usr[i].name) == 0)//在线
		{
			return TRUE;
		}		
	}
	if(i == count)//不在线
	{
        return FALSE;
	}		
}

//查看用户权限
int check_root(struct Msg *msg)
{
	if(strcmp(msg->name, "root") == 0)
	{
		return TRUE;
	}		
	else 
	{
		return FALSE;
	}		
}

//添加到在线用户列表
void add_usr(struct Msg *msg, int client_socket)
{
	pthread_mutex_lock(&mutex);    // 抢锁
	
	strcpy(usr[count].name, msg->name);
	usr[count].socket = client_socket;
	count++;
	
	pthread_mutex_unlock(&mutex);  // 解锁
}
 
//查看在线用户
void see_online(int client_socket, struct Msg *msg)
{
	int i;
	for(i=0; i<20; i++)
	{
		msg->usr[i] = usr[i];
	}
	
	write(client_socket, msg, sizeof(struct Msg));
}
 
//保存一条聊天记录
void insert_record(struct Msg *msg)
{
	//定义指向数据库的指针
	sqlite3 * database;
	//打开数据库
	int ret = sqlite3_open("allrecord.db", &database);
	if (ret != SQLITE_OK)
	{
		printf ("打开数据库失败\n");
		return ;
	}
	
	//获取系统当前时间
	time_t timep;  
    char s[30];   
    time(&timep);  
    strcpy(s,ctime(&timep));
	int count = strlen(s);
	s[count-1] = '\0';
	
	char *errmsg = NULL;
	char *sql = "create table if not exists allrecord(time TEXT,fromname TEXT,toname TEXT,word TEXT);";
	ret = sqlite3_exec(database, sql, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("聊天记录表创建失败：%s\n", errmsg);
		return;
	}
	
	char buf[1200];
	sprintf(buf, "insert into allrecord values('%s','%s','%s','%s')",s,msg->fromname, msg->toname,msg->msg);
	ret = sqlite3_exec(database, buf, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("添加聊天记录失败：%s\n", errmsg);
		return ;
	}
	
	sqlite3_close(database);
	return ;
}
 
//群聊
void chat_group(int client_socket, struct Msg *msg)
{
	int i = 0;
	//首先排除掉自己给自己发消息
	for (i = 0; i < SIZE; i++)
	{
		if (usr[i].socket != 0 && strcmp(msg->fromname, usr[i].name) == 0)
		{
			break;
		}
	}
	if(usr[i].flag == 0)     //判断该用户有没有被禁言,为0即为未被禁言
	{
		printf ("%s 发一次群消息\n", msg->fromname);
		//insert_record(msg);
		
		
		for (i = 0; i < SIZE; i++)//遍历告诉出自己以外的在线用户通知群消息，套接口不为0并且也不是自己给自己发消息
		{
			if (usr[i].socket != 0 && strcmp(msg->fromname, usr[i].name) != 0)
			{
				write (usr[i].socket, msg, sizeof(struct Msg));	
			}
		}
	}
	else
	{
		msg->cmd = 1003;
		write (client_socket, msg, sizeof(struct Msg));
	}
	
}
 
//私聊
void chat_private(int client_socket, struct Msg *msg)
{
	int i;
	//排除自己给自己发
	for (i = 0; i < SIZE; i++)
	{
		if (usr[i].socket != 0 && strcmp(msg->fromname, usr[i].name) == 0)
		{
			break;
		}
	}
	if(usr[i].flag == 0)//未被禁言
	{
		printf("%s给%s发了一条消息\n", msg->fromname, msg->toname);
		//insert_record(msg);//保存聊天记录
		
		for (i = 0; i < SIZE; i++)//找到符合的套接口描述符和符合的接收消息的人才算发送成功，再写给客户端
		{
			if (usr[i].socket != 0 && strcmp(usr[i].name, msg->toname)==0)
			{
				write (usr[i].socket, msg, sizeof(struct Msg));	
				break;
			}
		}
	}
	else//用户被禁言，无法发送消息
	{
		msg->cmd = 1003;
		write (client_socket, msg, sizeof(struct Msg));
	}
}
 
//获取文件大小
int file_size(char* filename)  
{  
    struct stat statbuf;  
    stat(filename,&statbuf);  
    int size = statbuf.st_size;  
  
    return size;  
}
 
//上传文件
void send_file(int client_socket, struct Msg *msg)
{
	printf("用户%s在聊天室内上传了一个文件%s\n",msg->fromname,msg->msg);
	
	int i;
	for (i = 0; i < SIZE; i++)
	{
		if (usr[i].socket != 0 && strcmp(usr[i].name, msg->fromname) != 0)
		{
			write (usr[i].socket, msg, sizeof(struct Msg));	
			break;
		}
	}
	
	int fd = open(msg->msg, O_RDWR | O_CREAT, 0777);
	if(fd == -1)
	{
		perror("open");
		printf("文件传输失败\n");
	}
	
	int size = msg->filesize;
	char buf[65535];
	memset(buf, 0, 65535);
	
	int ret = read(client_socket, buf, size);
	if(ret == -1)
	{
		perror("read");
		return;
	}	
	write(fd, buf, ret);
	close(fd);
}
 
//从用户数据库删除一个用户
void del_fromsql(struct Msg *msg)
{
	//打开用户数据库
	sqlite3 * database;
	int ret = sqlite3_open("usr.db", &database);
	if (ret != SQLITE_OK)
	{
		printf ("打开数据库失败\n");
		return;
	}
	//删除用户
	char *errmsg = NULL;
	char buf[100];
	sprintf(buf, "delete from usr where name = '%s';", msg->name);
	ret = sqlite3_exec(database, buf, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("删除用户失败：%s\n", errmsg);
		return;
	}
	
	sqlite3_close(database);
	return;
}
 
//注销用户
void delete_user(int client_socket,struct Msg *msg)
{
	int i,j;
	for(i=0; i<count; i++)
	{
		if(strcmp(msg->name, usr[i].name) == 0)//遍历找到当前用户
		{
			break;
		}
	}
	for(j = i; j < count; j++)//覆盖
	{
		usr[j] = usr[j+1];//在线人数少一个
	}
	count--;//在线人数减一
	printf("正在注销用户%s\n",msg->name);

	del_fromsql(msg);//从用户数据库中删除

	printf("已从数据库中删除\n");
	usleep(500000);

	write(client_socket, msg, sizeof(struct Msg));
	return;
	
}
 
//退出当前账号
void off_line(int client_socket,struct Msg *msg)
{
	pthread_mutex_lock(&mutex);    // 抢锁
	
	printf("用户%s下线\n",msg->name);
	int i,j;
	for(i=0; i<count; i++)
	{
		if(strcmp(msg->name, usr[i].name) == 0)
		{
			break;
		}
	}
	for(j=i; j<count; j++)
	{
		usr[j] = usr[j+1];
	}
	count--;
	
	pthread_mutex_unlock(&mutex);  // 解锁
	write(client_socket, msg, sizeof(struct Msg));
	
	return;
}
 
//用户下载文件
void download_file(int client_socket,struct Msg *msg)
{
	printf("用户%s下载了文件%s\n", msg->name, msg->msg);
	int size = file_size(msg->msg);//该函数在上传文件上方
	msg->filesize = size;
	write(client_socket, msg, sizeof(struct Msg));
	
	usleep(100000);
	
	int fd = open(msg->msg, O_RDONLY, 0777);
	if(fd == -1)
	{
		perror("open");
		printf("文件下载失败\n");
	}
	
	char buf[65535];
	memset(buf, 0, 65535);
 
	int ret = read(fd, buf, size);
	if(ret == -1)
	{
		perror("read");
		return;
	}	
	write(client_socket, buf, ret);
	close(fd);	
}

//修改密码
void change_password(int client_socket,struct Msg *msg)
{
	printf("%s 请求更改密码\n",msg->fromname);

	for (int i = 0; i < SIZE; i++)//判断
	{
		if (usr[i].socket != 0 && strcmp(msg->fromname, usr[i].name) == 0)//遍历在线
		{
			msg->cmd = 101;
			//打开数据库
	        sqlite3 * database;
	        int ret = sqlite3_open("usr.db",&database);
	        if(ret != SQLITE_OK)
	        {
		        printf("打开数据库失败\n");
		        return ;
	        }
			char * sql = NULL;
            char ** pResult = NULL;
            char * errmsg = NULL;
            int nrow,ncol;

            sql = "select * from usr;";

            ret = sqlite3_get_table(database,sql,&pResult,&nrow,&ncol,&errmsg);
            //处理错误信息
            if(ret != SQLITE_OK)
            {
                printf("数据库操作失败：%s\n",errmsg);
                return ;
            }
			//遍历找到fromname并且密码正确
			int i;
            for(i = 0 + ncol;i < (nrow + 1)*ncol;i += ncol)
            {
                if(strcmp(pResult[i],msg->fromname) == 0 && strcmp(pResult[i + 1],msg->msg) == 0)
                {
            
                    printf("%s 验证通过\n",msg->fromname);
                    write(client_socket,msg,sizeof(struct Msg));

                    //修改密码
                    char buf[1050];
                    errmsg = NULL;
                    sprintf(buf,"update usr set password = '%s' where name = '%s';",msg->password,msg->fromname);
                    ret = sqlite3_exec(database, buf, NULL, NULL, &errmsg);
                    if (ret != SQLITE_OK)
                    {
                        printf ("数据库操作失败：%s\n", errmsg);
                        return;
                    }

                    sqlite3_free_table(pResult);

                    // 关闭数据库
                    sqlite3_close(database);
                    printf ("密码修改完成，已关闭数据库\n");
                    return ;
                }
            }
            printf ("%s 验证不通过，密码输入有误\n", msg->fromname);
            msg->cmd = 1;
            write (client_socket, msg, sizeof(struct Msg));
            sqlite3_free_table(pResult);
            // 关闭数据库
            sqlite3_close(database);
            printf ("操作完成，已关闭数据库\n");     
        }
			
	}
}
 
//设置禁言
void forbid_speak(int client_socket,struct Msg *msg)
{
	msg->cmd = 1003;
	printf("用户%s已被禁言\n",msg->msg);
	
	pthread_mutex_lock(&mutex);//抢锁
	int i;
	for (i = 0; i < SIZE; i++)//遍历查找需要被禁言的用户
	{
		if (usr[i].socket != 0 && strcmp(usr[i].name, msg->msg)==0)
		{
			write (usr[i].socket, msg, sizeof(struct Msg));
			usr[i].flag = 1;//设置为1即被禁言
			break;
		}
	}
	pthread_mutex_unlock(&mutex);//解锁
}
 
//解除禁言
void relieve_speak(int client_socket,struct Msg *msg)
{
	msg->cmd = 1004;
	printf("用户%s已被解除禁言\n",msg->msg);
	
	pthread_mutex_lock(&mutex);//抢锁
	int i;
	for (i = 0; i < SIZE; i++)//遍历用户，查找被禁言的用户
	{
		if (usr[i].socket != 0 && strcmp(usr[i].name, msg->msg)==0)
		{
			write (usr[i].socket, msg, sizeof(struct Msg));
			usr[i].flag = 0;//将其改为0即解除禁言标志
			break;
		}
	}
	pthread_mutex_unlock(&mutex);
}
 
//踢出聊天室
void kickout_room(int client_socket,struct Msg *msg)
{
	msg->cmd = 1005;
	printf("用户%s已被踢出聊天室\n",msg->msg);
	
	pthread_mutex_lock(&mutex);
	int i;
	for (i = 0; i < SIZE; i++)//遍历，通知其他人某某某已被踢出聊天室
	{
		if (usr[i].socket != 0 && strcmp(usr[i].name, msg->msg) != 0)
		{
			//给在线用户通知某某某被踢出聊天室
			write (usr[i].socket, msg, sizeof(struct Msg));
		}
	}
	pthread_mutex_unlock(&mutex);
	
	for (i = 0; i < SIZE; i++)//查到要踢出聊天室的人
	{
		if (usr[i].socket != 0 && strcmp(usr[i].name, msg->msg) == 0)
		{
			break;//让其退出
		}
	} 
	msg->cmd = 1006;//已被踢出聊天室
	write (usr[i].socket, msg, sizeof(struct Msg));
}
 
//超级用户
void surper_usr(int client_socket)
{
	//定义结构体变量
	struct Msg msg;
	
	while(1)
	{
		//读取客户端发送来的任务请求
		int ret = read(client_socket, &msg, sizeof(msg));
		if (ret == -1)
		{
			perror ("read");
			break;
		}
		if (ret == 0)
		{
			printf ("客户端退出\n");
			break;
		}
 
		switch (msg.cmd)
		{
			case 1:
				see_online(client_socket, &msg);//查看在线人数
				break;
			case 2:
				chat_group(client_socket, &msg);//群聊
				break;
			case 3:
				chat_private(client_socket, &msg);//私聊
				break;
			case 6:
				forbid_speak(client_socket, &msg);  // 设置禁言
				break;                           
			case 7:		                         
				relieve_speak(client_socket,&msg);  // 解除禁言
				break;                           
			case 8:  
				off_line(client_socket,&msg);//下线
				return;			
			case 9:  
				kickout_room(client_socket,&msg);   // 踢出聊天室
				break;		
			case 0:
			    change_password(client_socket,&msg);//修改密码
				break;      		
		}
	}
}
 
//普通用户
void common_usr(int client_socket)
{
	//定义结构体变量
	struct Msg msg;
	
	while(1)
	{
		//读从客户端发来的消息
		int ret = read(client_socket, &msg, sizeof(msg));
		//错误处理
		if (ret == -1)
		{
			perror ("read");
			break;
		}
		if (ret == 0)
		{
			printf ("客户端退出\n");
			break;
		}
 
		switch (msg.cmd)
		{
			case 1:
				see_online(client_socket, &msg);//查看在线人数
				break;
			case 2:
				chat_group(client_socket, &msg);//群聊
				break;
			case 3:
				chat_private(client_socket, &msg);//私聊
				break;
			case 6:
				send_file(client_socket, &msg);//上传文件
				break;
			case 7:		
				delete_user(client_socket,&msg);//注销用户
				return;
			case 8:                  
				off_line(client_socket,&msg);//用户下线
				return;
			case 9:
				download_file(client_socket,&msg);//下载文件
				break;
			case 0:
			    change_password(client_socket,&msg);//修改密码
				break;        		
		}
	}
	
}
 
// 登陆
void login(int client_socket, struct Msg *msg)
{
	int flag1 = 0; //用来判断该用户有没有成功登陆  1代表成功
	//检查该用户有没有注册
	printf("正在查找该用户有没有注册...\n");
	if(find_name(msg) == TRUE)//查找用户名是否注册
	{
		if(find_np(msg) == TRUE)//查找用户名和密码是否和输入的吻合
		{
			if(check_ifonline(msg) == TRUE)//判断是否在线
			{
				msg->cmd = 3;
				printf("用户%s已经登陆过了\n",msg->name);
			}
			else
			{
				msg->cmd = 0;
				
				printf("检查该用户权限\n");
				if(check_root(msg) == TRUE)
				{
					printf("该用户是超级用户\n");
					msg->flag = 1;
				}
				else
				{
					printf("该用户是普通用户\n");
					msg->flag = 0;
				}
				printf("用户%s登陆成功\n",msg->name);
				flag1 = 1;
				
				add_usr(msg, client_socket);    //添加到在线用户列表
			}	
		}
		else
		{
			msg->cmd = 1;//已注册但密码输入错误
			printf("用户%s密码输入错误\n",msg->name);
		}
	}
	else
	{
		msg->cmd = 2;
		printf("用户%s还没有注册\n",msg->name);
	}
	
	write(client_socket, msg, sizeof(struct Msg));
	
	if(flag1 == 1)
	{
		if(msg->flag == 1)
		{
			surper_usr(client_socket);
		}		
		if(msg->flag == 0)
		{
			common_usr(client_socket);
		}		
	}
}

//添加用户到数据库
int insert_sql(struct Msg *msg)
{
	//定义指向数据库的指针
	sqlite3 * database;
	//打开数据库
	int ret = sqlite3_open("usr.db", &database);
	//打开错误处理
	if (ret != SQLITE_OK)
	{
		printf ("打开数据库失败\n");
		return FALSE;
	}
	
	//像表中插入数据
	char *errmsg = NULL;
	char buf[100];
	sprintf(buf, "insert into usr values('%s','%s');", msg->name, msg->password);
	ret = sqlite3_exec(database, buf, NULL, NULL, &errmsg);//调用接口函数
	//调用失败处理
	if (ret != SQLITE_OK)
	{
		printf ("添加用户失败：%s\n", errmsg);
		return FALSE;
	}
	
	sqlite3_close(database);
	return TRUE;
}

// 注册
void reg(int client_socket, struct Msg *msg)
{
	//查找用户是否已经被注册
	printf("正在查找该用户是否被注册...\n");
	if(find_name(msg) == TRUE)
	{
		printf("用户%s已经被注册\n",msg->name);
		msg->cmd = 0;	
	}
	else
	{
		if(insert_sql(msg) == TRUE)
		{
			msg->cmd = 1;
			printf("用户%s成功添加到数据库\n",msg->name);
		}
	}
	write(client_socket, msg, sizeof(struct Msg));
}

//对客户端发来的信息进行处理
void * handle_client(void* arg)
{
	//定义结构体变量
	struct Msg msg;
	int client_socket = *((int *)arg);//强制类型转换
	
	while(1)
	{
		// 从客户端读一个结构体数据
		int ret = read(client_socket, &msg, sizeof(msg));
		if (ret == -1)
		{
			perror ("read");
			break;
		}
		// 代表客户端退出
		if (ret == 0)
		{
			printf ("客户端退出\n");
			break;
		}
		
		printf("从客户端读到一个用户：%s, %s, %d\n", msg.name, msg.password, msg.cmd);
		
		switch (msg.cmd)
		{
			case 1:   //注册
				reg(client_socket, &msg);
				break;
			case 2:   //登录
				login(client_socket, &msg);
				break;
		}	
	}
	close (client_socket);
}
 
 

 
//建立所有用户的聊天记录数据库
// void setup_record()
// {
// 	sqlite3 * database;
	
// 	int ret = sqlite3_open("allrecord.db", &database);
// 	if (ret != SQLITE_OK)
// 	{
// 		printf ("打开数据库失败\n");
// 		return;
// 	}
//     //创建表
// 	char *errmsg = NULL;
// 	char *sql = "create table if not exists allrecord(time TEXT,fromname TEXT,toname TEXT,word TEXT);";
// 	ret = sqlite3_exec(database, sql, NULL, NULL, &errmsg);//调用接口函数
// 	if (ret != SQLITE_OK)
// 	{
// 		printf ("聊天记录表创建失败：%s\n", errmsg);
// 		return;
// 	}
	
// 	sqlite3_close(database);
// 	return;
// }
 
//建立用户数据库，并在里面添加超级用户
int setup_sql()
{
	//定义指向数据库的指针
	sqlite3 * database;
	
	//创建数据库
	int ret = sqlite3_open("usr.db", &database);
	if (ret != SQLITE_OK)
	{
		printf ("打开数据库失败\n");
		return FALSE;
	}
 
    //创建表
	char *errmsg = NULL;
	char *sql = "create table if not exists usr(name TEXT,password TEXT);";
	ret = sqlite3_exec(database, sql, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("用户表创建失败：%s\n", errmsg);
		return FALSE;
	}
	
	//定义结构体变量，添加超级用户
	struct Msg msg;
	strcpy(msg.name, "root");
	strcpy(msg.password, "123");
	
	insert_sql(&msg);
	
	sqlite3_close(database);
	return TRUE;
}
 
// 初始化套接字，返回监听套接字
int init_socket()
{
	//1、创建socket
	int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	//错误处理
	if (listen_socket == -1)
	{
		perror ("socket");
		return -1;
	}
	
	// 2、命名套接字，配置地址，绑定本地的ip地址和端口
	struct sockaddr_in addr;
	bzero(&addr,sizeof(addr));
	addr.sin_family  = AF_INET;     // 设置地址族
	addr.sin_port    = htons(PORT); // 设置本地端口
	addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 使用本地的任意IP地址
	
	int  ret = bind(listen_socket,  (struct sockaddr *)&addr, sizeof(addr));
	if (ret == -1)
	{
		perror ("bind");
		return -1;
	}
	
	// 3、监听本地套接字
	ret = listen(listen_socket, 5);
	//错误处理
	if (ret == -1)
	{
		perror ("listen");
		return -1;
	}
	
	printf ("等待客户端连接.......\n");
	return listen_socket;
}
 
// 处理客户端连接，返回与连接上的客户端通信的套接字
int  MyAccept(int listen_socket)
{
	struct sockaddr_in client_addr; //客户端地址
	int len = sizeof(client_addr);
	//与客户端建立连接
	int client_socket = accept(listen_socket, (struct sockaddr *)&client_addr,  &len);
	//错误处理
	if (client_socket == -1)
	{
		perror ("accept");
		return -1;
	}
	
	printf ("成功接收一个客户端: %s\n", inet_ntoa(client_addr.sin_addr));//将其转化为点分十进制字符串
	
	return client_socket;
}
 
int main()
{
	//创建用户数据库，并在里面添加超级用户
	setup_sql();
	//建立所有用户的聊天记录数据库
	//setup_record();
	
	pthread_mutex_init(&mutex, NULL);

	//调用函数，将创建套接口、配置地址、绑定和监听写在该函数中
	int listen_socket = init_socket();

    //线程池初始化
    struct threadpool *pool = threadpool_init(20, 100);

	
	while (1)
	{
		// 获取与客户端连接的套接字
		int client_socket = MyAccept(listen_socket);
		
		// 创建一个线程去处理客户端的请求，即handle_client函数，主线程依然负责监听
		// pthread_t tid;
		// pthread_create(&tid, NULL, (void *)handle_client,  (void *)&client_socket);	
		// pthread_detach(tid); 
		threadpool_add_job(pool, (void *)handle_client, (void *)&client_socket);

	}
	thread_destroy(pool);
	
	//关闭监听套接口
	close (listen_socket);
	
	//互斥锁使用完将其销毁
	pthread_mutex_destroy(&mutex);

	return 0;
	
}