#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>
#include <sqlite3.h>
#include<pthread.h>
 
#include <sys/stat.h>
#include <fcntl.h>
 
#define PORT  8888  //端口号
#define SIZE 20
int flag = 0;
int offline = 0; //用来判断该用户有没有下线  0代表没有，1代表下线
struct Usr
{
	char name[SIZE];
	int socket;
	int flag;//用来判断该用户是否被禁言，没有被禁设为0，被禁设为1
};
 
struct Msg
{
	struct Usr usr[SIZE];
	char msg[1024];
	char buf[1024];
	char name[SIZE];
	char fromname[SIZE];
	char toname[SIZE];
	char password[SIZE];
	int cmd;
	int filesize;
	int flag;    //用来判断用户权限   0代表普通用户，1代表超级用户
};
 
struct Msg tmp;
 
//显示主界面
void maininterface()
{
	system("clear");
    printf("\033[42;37m***********************************************\n\033[0m");
    printf("\033[42;37m                                               \n\033[0m");
    printf("\033[42;30m             欢迎来到tt聊天室                  \n\033[0m");
    printf("\033[42;37m                                               \n\033[0m");
    printf("\033[42;37m***********************************************\n\033[0m");
    printf("\033[42;37m                                               \n\033[0m");
    printf("\033[42;37m                                               \n\033[0m");
    printf("\033[42;30m                  1. 注册                      \n\033[0m");
    printf("\033[42;30m                  2. 登录                      \n\033[0m");
    printf("\033[42;30m                  3. 退出                      \n\033[0m");
    printf("\033[42;37m                                               \n\033[0m");
    printf("\033[42;37m                                               \n\033[0m");
    printf("\033[42;37m***********************************************\n\033[0m");
    printf("***********请输入命令：");
}
 
//显示普通用户界面
void cominterface(struct Msg *msg)
{
	system("clear");
	printf("\t\t-.- %s:\n", msg->name);
    printf("\033[44;37m***********************************************\n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;30m             这里是tt聊天室                    \n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;37m***********************************************\n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;30m              1. 查看在线用户                  \n\033[0m");
    printf("\033[44;30m              2. 进入群聊                      \n\033[0m");
    printf("\033[44;30m              3. 进入私聊                      \n\033[0m");
    printf("\033[44;30m              4. 删除聊天记录                  \n\033[0m");
    printf("\033[44;30m              5. 查看聊天记录                  \n\033[0m");
    printf("\033[44;30m              6. 上传文件                      \n\033[0m");
    printf("\033[44;30m              7. 注销当前用户                  \n\033[0m");
    printf("\033[44;30m              8. 退出当前账号                  \n\033[0m");
    printf("\033[44;30m              9. 下载文件                      \n\033[0m");
	printf("\033[44;30m              0. 修改密码                      \n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;37m***********************************************\n\033[0m");
    printf("请输入命令：");
	
}
 
//显示超级用户界面
void supinterface(struct Msg *msg)
{
	system("clear");
    printf("\033[44;37m***********************************************\n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;30m             这里是tt聊天室                    \n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;37m***********************************************\n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;30m              1. 查看在线用户                  \n\033[0m");
    printf("\033[44;30m              2. 进入群聊                      \n\033[0m");
    printf("\033[44;30m              3. 进入私聊                      \n\033[0m");
    printf("\033[44;30m              4. 删除聊天记录                  \n\033[0m");
    printf("\033[44;30m              5. 查看聊天记录                  \n\033[0m");
    printf("\033[44;30m              6. 设置禁言                      \n\033[0m");
    printf("\033[44;30m              7. 解除禁言                      \n\033[0m");
    printf("\033[44;30m              8. 退出当前账号                  \n\033[0m");
    printf("\033[44;30m              9. 踢出聊天室                    \n\033[0m");
	printf("\033[44;30m              0. 修改密码                      \n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;37m                                               \n\033[0m");
    printf("\033[44;37m***********************************************\n\033[0m");
    printf("请输入命令：");

	
}


//下线
void off_line(int socketfd, struct Msg *msg)
{
	msg->cmd = 8;
	write(socketfd, msg, sizeof(struct Msg));
	printf("\t\t正在退出...\n");
}
 
//查看在线用户
void see_online(int socketfd, struct Msg *msg)
{
	msg->cmd = 1;
	write(socketfd, msg, sizeof(struct Msg));
		
	return;
}
 
//保存一条聊天记录
void insert_mysql(struct Msg *msg)
{
	sqlite3 * database;
	int ret = sqlite3_open("person.db", &database);
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
	char *sql = "create table if not exists person(time TEXT,fromname TEXT,toname TEXT,word TEXT);";
	ret = sqlite3_exec(database, sql, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("用户表创建失败：%s\n", errmsg);
		return;
	}
	
	char buf[1200];
	sprintf(buf, "insert into person values('%s','%s','%s','%s');",s,msg->fromname, msg->toname,msg->msg);
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
void chat_group(int socketfd, struct Msg *msg)
{
	msg->cmd = 2;
	printf ("\t\t请输入要发送的内容: ");
	fgets(msg->msg, 1024, stdin);
	int len = strlen(msg->msg);
	msg->msg[len-1] = '\0';
	strcpy(msg->fromname, msg->name);
	strcpy(msg->toname, "all");
	insert_mysql(msg);
	
	write (socketfd, msg, sizeof(struct Msg));
	//printf("aaa\n");
}
 
//发送悄悄话
void chat_private(int socketfd, struct Msg *msg)
{
	msg->cmd = 3;
	
	printf ("\t\t请输入要发送的对象名称: \n");
	fgets(msg->toname, SIZE, stdin);
	int len = strlen(msg->toname);
	msg->toname[len-1] = '\0';
	printf ("\t\t请输入要发送的内容: \n");
	fgets(msg->msg, 1024, stdin);
	len = strlen(msg->msg);
	msg->msg[len-1] = '\0';
	strcpy(msg->fromname, msg->name);
	
	insert_mysql(msg);//保存聊天记录
	
	write(socketfd, msg, sizeof(struct Msg));
}
 
//删除聊天记录
void del_personsql()
{
	sqlite3 * database;
	int ret = sqlite3_open("person.db", &database);
	if (ret != SQLITE_OK)
	{
		printf ("打开数据库失败\n");
		return;
	}
	
	char *errmsg = NULL;
	char *sql = "create table if not exists person(time TEXT,fromname TEXT,toname TEXT,word TEXT);";
	ret = sqlite3_exec(database, sql, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("用户表创建失败：%s\n", errmsg);
		return;
	}
	
	char *buf = "drop table person";
	ret = sqlite3_exec(database, buf, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("删除聊天记录失败：%s\n", errmsg);
		return;
	}
	
	sqlite3_close(database);
	printf("\n\n\n");
	printf("\t\t删除成功\n");
	printf("\t\t按ENTER键返回\n");
	while(getchar() != '\n');
	return;
}
 
//查看聊天记录
void see_record()
{
	sqlite3 * database;
	int ret = sqlite3_open("person.db", &database);
	if (ret != SQLITE_OK)
	{
		printf ("打开数据库失败\n");
		return ;
	}
	
	char *errmsg = NULL;
	char *sql = "create table if not exists person(time TEXT,fromname TEXT,toname TEXT,word TEXT);";
	ret = sqlite3_exec(database, sql, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("用户表创建失败：%s\n", errmsg);
		return;
	}
	
	char **resultp = NULL;
	int nrow, ncolumn;
	char *buf = "select * from person";
	ret = sqlite3_get_table(database, buf, &resultp, &nrow, &ncolumn, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("数据库操作失败：%s\n", errmsg);
		return;
	}
	//判断表是否为空
	if(nrow == 0)
	{
		printf("\n\n\n");
		printf("\t\t没有聊天记录\n");
	}
	else
	{
		int i;
		system("clear");
		printf ("nrow = %d, ncolumn = %d\n", nrow, ncolumn);
		printf("\t\t%s",resultp[0]);
		printf("\t%12s",resultp[1]);
		printf("\t%8s",resultp[2]);
		printf("\t%s",resultp[3]);
		for (i = 4; i < (nrow+1)*ncolumn; i++)
		{
			if (i % ncolumn == 0)
			{
				printf ("\n");//换行
			}
				
			printf ("%12s", resultp[i]);
		}
		printf ("\n");	
		
	}
		
	
	printf("\t\t按ENTER键返回\n");
	while(getchar() != '\n');
	
	sqlite3_free_table(resultp);
	
	sqlite3_close(database);
	return;
	
}
 
//获取文件大小
int file_size(char* filename)  
{  
    struct stat statbuf;  //获取文件属性的结构体变量
    stat(filename,&statbuf);  
    int size=statbuf.st_size;  //st_size是结构体成员，获取文件大小，单位是字节
  
    return size;  
}
//上传文件
void send_file(int socketfd, struct Msg *msg)
{
	msg->cmd = 6;
	
	strcpy(msg->fromname, msg->name);
	
	printf("\t\t请输入要发送的文件名：\n");
	system("ls");
	fgets(msg->msg, 1024, stdin);
	int len = strlen(msg->msg);
	msg->msg[len-1] = '\0';
	
	int size = file_size(msg->msg);//该函数在其上方
	msg->filesize = size;
	write(socketfd, msg, sizeof(struct Msg));//将文件的大小和文件名发给服务器处理
	printf("\t\t正在上传，请稍后...\n");
	sleep(1);
	
	int fd = open(msg->msg, O_RDONLY, 0777);
	if(fd == -1)
	{
		perror("open");
		printf("文件传输失败\n");
	}
	
	char buf[65535];
	memset(buf, 0, 65535);
	
	int ret = read(fd, buf, size);//从fd文件中读取数据放进buf中，ret是实际读取的字节数
	if(ret == -1)
	{
		perror("read");
		return;
	}	
	write(socketfd, buf, ret);//将数据写进socketfd中
	close(fd);
	printf("\t\t上传文件成功\n");
	sleep(1);
 
}

//下载文件
void download_file(int socketfd, struct Msg *msg)
{
	msg->cmd = 9;
 
	printf("\t\t请输入要下载的文件名：\n");
	fgets(msg->msg, 1024, stdin);
	int len = strlen(msg->msg);
	msg->msg[len-1] = '\0';
 
	write(socketfd, msg, sizeof(struct Msg));
 
}
 
//注销当前用户
void delete_user(int socketfd, struct Msg *msg)
{
	msg->cmd = 7;
	write(socketfd, msg, sizeof(struct Msg));
	printf("\n\n\n");
	printf("\t\t正在注销...\n");
}

//修改密码
void change_password(int socketfd,struct Msg * msg)
{
	char ch[2];
	msg->cmd = 0;
    printf("\n您确定需要修改密码吗?(y/n): ");
    fgets(ch,2,stdin);
    while(getchar() != '\n');
    if(ch[0] != 'y')
    {
        printf("\n\t已取消密码修改\n");
        return ;
    }
    printf("\t请输入旧密码: ");
    scanf("%s",msg->msg);
    getchar();
    printf("\t请输入新密码: ");
    scanf("%s",msg->password);
    getchar();
    strcpy(msg->fromname, msg->name);

    write (socketfd, msg, sizeof(struct Msg)); // 向服务器发送注册信息

    printf ("\n\t正在校验数据,请稍候......\n");
    sleep(1);
}
 
//设置禁言
void forbid_speak(int socketfd, struct Msg *msg)
{
	msg->cmd = 6;
	printf("\t\t请输入要禁言的对象:\n");
	fgets(msg->msg, SIZE, stdin);
	int len = strlen(msg->msg);
	msg->msg[len-1] = '\0';
	
	write(socketfd, msg, sizeof(struct Msg));
	
}
 
//解除禁言
void relieve_speak(int socketfd, struct Msg *msg)
{
	msg->cmd = 7;
	printf("\t\t请输入要解除禁言的对象:\n");
	fgets(msg->msg, SIZE, stdin);
	int len = strlen(msg->msg);
	msg->msg[len-1] = '\0';
	
	write(socketfd, msg, sizeof(struct Msg));
}
 
//踢出聊天室
void kickout_room(int socketfd, struct Msg *msg)
{
	msg->cmd = 9;
	printf("\t\t请输入要踢除的对象:\n");
	fgets(msg->msg, SIZE, stdin);
	int len = strlen(msg->msg);
	msg->msg[len-1] = '\0';
	
	write(socketfd, msg, sizeof(struct Msg));
}
 

 
//超级用户
void super_usr(int socketfd, struct Msg *msg)
{
	char ch[SIZE];
	int x;
	while (1)
	{
		supinterface(msg);
		scanf("%s",ch);
		while(getchar() != '\n');
		
		switch(ch[0])
		{
			case '1':                   // 查看在线用户
				see_online(socketfd, msg);
				break;
			case '2':                   // 群发消息
				chat_group(socketfd, msg);
				break;
			case '3':                   // 发送悄悄话
				chat_private(socketfd, msg);
				break;
			case '4':                   // 删除聊天记录
				del_personsql();
				break;
			case '5':                   // 查看聊天记录
				see_record();
				break;
			case '6':                   // 设置禁言
				forbid_speak(socketfd, msg);
				break;
			case '7':                   // 解除禁言
				relieve_speak(socketfd, msg);
				break;
			case '8':                   // 退出当前账号
				off_line(socketfd, msg);
				return;			
			case '9':                   // 踢出聊天室
				kickout_room(socketfd, msg);
				break;
			case '0':
			    change_password(socketfd,msg);  //修改密码
				break;
			default: 
				printf("\n\n\n");
				printf("错误指令，请重新输入\n");
				sleep(1);
				break;
		}	
		
		
	}
}

//普通用户
void common_usr(int socketfd, struct Msg *msg)
{
	char ch[SIZE];
	while (1)
	{
		cominterface(msg);//普通用户界面
		//scanf("%s",ch);
		//while(getchar() != '\n');
		fgets(ch, SIZE, stdin);
		if(offline == 1)//下线
		{
			printf("\n\n\n");
			printf("\n\t\t请稍后...\n");
			off_line(socketfd, msg);
			return;
		}
		else
		{
			switch(ch[0])
			{
				case '1':                   // 查看在线用户
					see_online(socketfd, msg);
					break;
				case '2':                   // 群发消息
					chat_group(socketfd, msg);
					break;
				case '3':                   // 发送悄悄话
					chat_private(socketfd, msg);
					break;
				case '4':                   // 删除聊天记录
					del_personsql();
					break;
				case '5':                   // 查看聊天记录
					see_record();
					break;
				case '6':                   // 上传文件
					send_file(socketfd, msg);
					break;
				case '7':                   // 注销当前用户
					delete_user(socketfd, msg);
					return;
				case '8':                   // 退出当前账号
					off_line(socketfd, msg);
					return;
				case '9':
					download_file(socketfd, msg); // 下载文件
					break;
				case '0':                    //修改密码
				    change_password(socketfd,msg);
					break;
				default: 
					printf("\n\n\n");
					printf("错误指令，请重新输入\n");
					sleep(1);
					break;
			}
		}			
	}
}
 
//创建聊天记录数据库
void set_mysql()
{
	//定义指向数据库的指针
	sqlite3 * database;
	
	int ret = sqlite3_open("person.db", &database);
	if (ret != SQLITE_OK)
	{
		printf ("打开数据库失败\n");
		return;
	}
 
	char *errmsg = NULL;
	char *sql = "create table if not exists person(time TEXT,fromname TEXT,toname TEXT,word TEXT)";
	ret = sqlite3_exec(database, sql, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf ("聊天记录表创建失败：%s\n", errmsg);
		return;
	}
	sqlite3_close(database);
	return;
}
 
//下载文件
void fun(int socketfd, struct Msg *msg)
{
	printf("\n\t\t正在下载,请稍后...\n");
	int fd = open(msg->msg, O_RDWR | O_CREAT, 0777);
	if(fd == -1)
	{
		perror("open");
		printf("文件下载失败了\n");
	}
	
	int size = msg->filesize;
	char buf[65535];
	memset(buf, 0, 65535);
	
	int ret = read(socketfd, buf, size);
	if(ret == -1)
	{
		perror("read");
		return;
	}
	write(fd, buf, ret);
	close(fd);
	sleep(1);
	printf("\t\t文件下载完成\n");
}

//专门用来读取服务器
void * readMsg(void *arg)
 {
	 int socketfd = *((int*)arg);
	 struct Msg msg;
	 while (1)
	 {
		 int i;
		 
		 int ret = read (socketfd, &msg, sizeof(msg));
		 switch (msg.cmd)
		 {
			case 1:  //显示在线用户
			{
		        printf("\n\t\t当前在线户：\n");
				printf("\t\t");
				for(i=0; i<SIZE; i++)
				{
					printf("%-5s",msg.usr[i].name);
				}
				//sleep(1);
				printf("\n");
				//getchar();	
				break;
			}	
			case 2:   // 接受一条群聊信息
			{
				printf ("\n%s给大家发了一条消息： %s\n", msg.name, msg.msg);
				insert_mysql(&msg);       //保存聊天信息
				break;

			}	
			case 3:   // 接受一条私聊信息
			{
				printf ("\n%s给你发一条消息：%s\n", msg.fromname, msg.msg);
				getchar();
				insert_mysql(&msg);
				break;

			}	
			case 6:   // 收到有人上传文件的消息	
			{
				printf("\n用户%s上传了一份文件%s\n", msg.fromname, msg.msg);	
				getchar();
				break;
			}			
			case 7:   //该用户已被注销
			{
				return NULL;//注销用户之后用户不存在，不需要返回什么
			}
			case 8:  //该用户已下线
			{
				return NULL;
			}
			case 9:
			{
				fun(socketfd, &msg);//下载文件
				break;
			}
			case 101:
			{
				printf("用户%s修改密码成功\n",msg.fromname);
				sleep(1);
				break;

			}	
			case 1003:
			{
				printf("\n\t\t您已被管理员禁言\n");
				getchar();
				
			    break;
			}	
			case 1004:
			{
				printf("\n您已被管理员解除禁言\n");
				getchar();
			    break;
			}
			case 1005:
			{
				printf("\n用户%s已被管理员踢出聊天室\n",msg.msg);
				getchar();
			    break;
			}	
			case 1006:
			{
				printf("\n\n\n");
				printf("\n\t\t您已被管理员踢出聊天室\n");
				printf("\t\t按ENTER键返回主菜单\n");
				offline = 1;
				//off_line(socketfd, msg);
			    break;
			}
		 }	 
	 }
 }
 
//登陆函数
void login(int socketfd)
{
	struct Msg msg;
	memset(&msg, 0, sizeof(msg));
	msg.cmd = 2;
	
	printf("\n\n\n");
	printf("\t\t请输入姓名：");
	scanf("%s",msg.name);
	while(getchar() != '\n');
	printf("\n");
	printf("\t\t请输入密码：");
	scanf("%s",msg.password);
	while(getchar() != '\n');
	write(socketfd, &msg, sizeof(msg));
	
	read(socketfd, &msg, sizeof(msg));
	
	printf("%d\n",msg.cmd);
	if(msg.cmd == 0)
	{
		printf("\n\n\n");
		printf("\t\t正在登陆...\n");
		sleep(2);
		printf("\n\n\n");
		printf("\t\t登陆成功\n");
		
		//启动一个线程专门用来接受聊天信息,此处读写分离，专门写一个用来读取服务器消息的函数
		pthread_t id;
		pthread_create(&id, NULL, (void *)readMsg,  (void *)&socketfd);
		pthread_detach(id);                 
		
		offline = 0;
		if(msg.flag == 1) //超级用户
		{
			super_usr(socketfd, &msg);
		}                   	
		if(msg.flag == 0)                   //普通用户
		{
			common_usr(socketfd, &msg);
		}				
	}
	if(msg.cmd == 1)
	{
		printf("\n\n\t\t正在登陆...\n");
		sleep(2);
		printf("\n\n\t\t密码错误\n");
	}
	if(msg.cmd == 2)
	{
		printf("\n\n\t\t正在登陆...\n");
		sleep(2);
		printf("\n\n\t\t用户不存在，请先注册\n");
	}
	if(msg.cmd == 3)
	{
		printf("\n\n\t\t正在登陆...\n");
		sleep(2);
		printf("\n\n\t\t该用户已经登陆，请勿重复登陆\n");
	}		
	sleep(1);
	return;
}
 
//注册函数
void reg(int socketfd)
{
	struct Msg msg;
	memset(&msg, 0, sizeof(msg));
	msg.cmd = 1;
	
	printf("\n\n\n");
	printf("\t\t请输入姓名：");
	scanf("%s",msg.name);
	while(getchar() != '\n');
 
	printf("\n");
	printf("\t\t请输入密码：");
	scanf("%s",msg.password);
	while(getchar() != '\n');
 
	write(socketfd, &msg, sizeof(msg));
	
	read(socketfd, &msg, sizeof(msg));
	if(msg.cmd == 0)//注册失败
	{
		printf("\n\n\n\t\t注册失败，该用户已被注册\n");
	}
	if(msg.cmd == 1)//注册成功
	{
		printf("\n\n\n");
		
		set_mysql();      //注册成功就建立一个用户自己的数据库存放在本地，存放聊天记录
		printf("\t\t注册成功\n");
	}
	sleep(2);
	return;
}

// 客户端向服务器发送数据
void ask_server(int socketfd)
{
	int x;
	while (1)
	{
		maininterface();
		scanf("%d",&x);
		while(getchar() != '\n');
		switch(x)
		{
			case 1:     // 注册
			{
				reg(socketfd);
				break;

			}	
			case 2:     // 登陆
			{
				login(socketfd);
				break;
			}
				
			case 3:     // 退出
			{
				system("clear");
				return;
			}
			default: 
			{
				printf("\n\n\n\t\t姐妹~手抖啦!要输入正确的指令呐!\n");
				sleep(1);
				break;
			}		
		}	
	}
}
 
int main(int argc,char*argv[])
{	
	if(argc != 2)
	{
		printf("需要添加IP地址\n");
		exit(0);
	}
	// 创建与服务器通信的套接字
	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketfd == -1)
	{
		perror ("socket");
		return -1;
	}
	
	// 连接服务器
	struct sockaddr_in addr;
	bzero(&addr,sizeof(addr));//清空处理
	addr.sin_family = AF_INET;     
	addr.sin_port = htons(PORT); 
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	
	// 成功的情况下，可以通过socketfd与服务器进行通信
	int ret = connect(socketfd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret == -1)
	{
		perror ("connect");
		return -1;
	}
 
	printf ("成功连上服务器\n");
	
	//通过与套接口和服务器建立通信，调用访问服务器函数
	ask_server(socketfd);
	
	close(socketfd);
	
	return 0;
}