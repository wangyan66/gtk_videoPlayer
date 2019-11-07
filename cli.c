/* Client.c */
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <vlc/vlc.h>

#define MAX_BUFFER_SIZE 1024         
#define FILE_NAME_MAX_SIZE 512     
#define BORDER_WIDTH 6

/*Clinet */
static char userName[50];          //identify a client
static int fileReading = 0;        
pthread_t SendingThread;	//发送线程ID
pthread_t ReceivingThread;//接收线程ID
int sockfd;

/*GTK Player */
void build_interface();/*用于创建播放器界面的函数*/
void client_init();/*客户端初始化函数*/
void vlc_init();/*vlc初始化函数*/
void new_game();/*游戏开始函数*/

void destroy(GtkWidget *widget, gpointer data);
void player_widget_on_realize(GtkWidget *widget, gpointer data);
void on_open(GtkWidget *widget, gpointer data);
void open_media(const char* uri);
void on_playpause(GtkWidget *widget, gpointer data);
void on_stop(GtkWidget *widget, gpointer data);
void play(void);
void pause_player(void);
void getMediaValue(char *Buff);
gboolean _update_scale(gpointer data);
void on_value_change(GtkWidget *widget, gpointer data);
void sendMediaValue(int num);
libvlc_media_t *media;
libvlc_media_player_t *media_player;
void sendScaleValue(float value);
libvlc_instance_t *vlc_inst;
void ReceiveFile(char* dest, int Socket);
GtkWidget *playpause_button,*play_icon_image,*pause_icon_image,*stop_icon_image,
		  *process_scale;
GtkWidget *window,
		  *vbox,
		  *hbox,
		  *menubar,
		  *filemenu,
		  *fileitem,
		  *filemenu_openitem,
		  *player_widget,
		  *hbuttonbox,
		  *stop_button;
GtkAdjustment *process_adjuest;

float video_length, current_play_time;

void destroy(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
}

void player_widget_on_realize(GtkWidget *widget, gpointer data) {
    libvlc_media_player_set_xwindow((libvlc_media_player_t*)data, GDK_WINDOW_XID(gtk_widget_get_window(widget)));
}


void on_open(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;

    dialog = gtk_file_chooser_dialog_new("open file", GTK_WINDOW(widget), action, _("Cancel"), GTK_RESPONSE_CANCEL, _("Open"), GTK_RESPONSE_ACCEPT, NULL);

    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *uri;
        uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
        open_media(uri);
        g_free(uri);
    }
    gtk_widget_destroy(dialog);
}

void open_media(const char* uri) {
    media = libvlc_media_new_location(vlc_inst, uri);
    libvlc_media_player_set_media(media_player, media);

	current_play_time = 0.0f;
	gtk_scale_set_value_pos(GTK_SCALE(process_scale), current_play_time/video_length*100);
    play();
	g_timeout_add(500,_update_scale,process_scale);
    libvlc_media_release(media);
}

void on_playpause(GtkWidget *widget, gpointer data) {
	sendMediaValue(-1);
    if(libvlc_media_player_is_playing(media_player) == 1) {
        pause_player();
    }
    else {
        play();
    }
}

void on_stop(GtkWidget *widget, gpointer data) {
	sendMediaValue(-2);
    printf("on_stop\n");
    pause_player();
    libvlc_media_player_stop(media_player);
}


void on_value_change(GtkWidget *widget, gpointer data)
{
	float scale_value = gtk_adjustment_get_value(process_adjuest);
	//printf("%f\n",scale_value);
	libvlc_media_player_set_position(media_player, scale_value/100);
	sendScaleValue(scale_value/100);
	
}
gboolean _update_scale(gpointer data){
	// 获取当前打开视频的长度，时间单位为ms
	video_length = libvlc_media_player_get_length(media_player);
	current_play_time = libvlc_media_player_get_time(media_player);

	g_signal_handlers_block_by_func(G_OBJECT(process_scale), on_value_change, NULL);
	gtk_adjustment_set_value(process_adjuest,current_play_time/video_length*100);
	g_signal_handlers_unblock_by_func(G_OBJECT(process_scale), on_value_change, NULL);
	//printf("update_scale\n");
	return G_SOURCE_CONTINUE;
}

void play(void) {
    libvlc_media_player_play(media_player);
    pause_icon_image = gtk_image_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(playpause_button), pause_icon_image);
    printf("begin_play\n");
}

void pause_player(void) {
    libvlc_media_player_pause(media_player);
    play_icon_image = gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(playpause_button), play_icon_image);
    printf("pauser_player\n");
}


/*client function*/
void ReceiveFile(char* dest, int Socket);

//据文件名和服务器的套接字发送文件
void Sendfile(char* Filename, void* Socket)
{
	int *SocketCopy = Socket; 
	char buffer[1025];
	FILE *fp;
	fp = fopen(Filename, "r");

	//错误判断 
	if(NULL == fp)
	{
		printf("File:%s Not Found\n", Filename);
		return;
	}
	else
	{
		//循环发送文件
		int length =0;
		while((length = fread(buffer, sizeof(char), MAX_BUFFER_SIZE, fp)) > 0)
		{
			write(*SocketCopy, &length, sizeof(int));
			if(write(*SocketCopy, buffer, length) < 0)
			{
				printf("上传文件:%s 失败.\n", Filename);
				break;
			}
			bzero(buffer, MAX_BUFFER_SIZE);
		}
	}
	fclose(fp);
	printf("文件:%s 成功上传至服务器!\n", Filename);
	
}


//终端获得用户输入，将消息发送到服务器
void* Send(void* Socket)
{
	char sender[80];
	char Filename[FILE_NAME_MAX_SIZE];
	//保存套接字结构指针
	int *SocketCopy = Socket;
	while(1)
	{
		//文件传输信号特判
		if(fileReading)
			continue;
		//正常发送数据
		fgets(sender, sizeof(sender), stdin);
		
		//保存文件
		if(sender[1] == 'f' && sender[2] == 's')
		{
			fileReading = 1;
			char destination[50];
			for(int i = 4; i <  strlen(sender) - 1; ++i){
				destination[i - 4] = sender[i];
			}
			destination[strlen(sender) - 5] = '\0';
			//设置文件读取标志 并开始接收文件
			ReceiveFile(destination, *SocketCopy);
			continue;
		}		

		//不是文件传输，正常发送聊天消息
		int messageSize = strlen(sender) + 1;

		write(*SocketCopy, &messageSize, sizeof(int));
 		write(*SocketCopy, sender, messageSize);      
	
		//用户终端输入退出
		if(strcmp(sender, ":q!\n") == 0)
			exit(1);
		
		//发送文件
		else if(sender[1] == 'f' && sender[2] == 'w')
		{	
			printf("请输入文件名\n");
			scanf("%s", Filename);
			
			//打开文件 计算文件大小
			FILE *fp=fopen(Filename, "r");
			fseek(fp, 0L, SEEK_END);
			int Filesize=ftell(fp); 
			int intSize = sizeof(int);
			write(*SocketCopy, &intSize, sizeof(int));
			write(*SocketCopy, &Filesize, sizeof(int));

			//发送文件
	        Sendfile( Filename, SocketCopy );
			fileReading = 0;
		}		
	}
}


//接受文件
void ReceiveFile(char* dest, int Socket)
{
	//prepared to receive file
	char buffer[MAX_BUFFER_SIZE];
	printf("输入您想保存文件的位置%s\n", dest);
	FILE *fp = fopen(dest, "w");
	if(NULL == fp)
	{
		printf("File:\t%s Can Not Open To Write\n", dest);
		exit(1);
	}
	bzero(buffer, MAX_BUFFER_SIZE);

	//读取文件的大小
	char filesize[20];
	char filesizeStringSize[2];
	int L1 = read(Socket, filesizeStringSize, 2);
	int L2 = read(Socket, filesize, atoi(filesizeStringSize) + 1);
	int filesizeInt = atoi(filesize);

	//接收文件所需变量
	int length = 0;
	int i = 0;
	fileReading = 1;

	//根据文件大小分部接受
	while(i < filesizeInt/1024 + 1)
	{	
		length = read(Socket, buffer, MAX_BUFFER_SIZE); 
		if(fwrite(buffer, sizeof(char), length - 2, fp) < length - 2)
		{
			printf("文件\t%s写入失败\n", dest);
			return;
		}
		printf("第%d部分文件接受完毕!\n", ++i);
		bzero(buffer, MAX_BUFFER_SIZE);
	}

	printf("成功接受完成文件至目录%s!\n", dest);
	fileReading = 0;
	fclose(fp);
}



//从服务器接收消息
void* Receive(void* Socked)
{
	int *SockedCopy = Socked;
	char Receiver[80];

	while(1){
		if(fileReading == 1)
			continue;
		//循环接受信息
		int reveiverEnd = 0;
		reveiverEnd  = read (*SockedCopy, Receiver, 1000);
		if(Receiver[0] == '!' && Receiver[1] == '!')
			fileReading = 1;
		Receiver[reveiverEnd] = '\0';
                if(Receiver[0] == ':' && Receiver[1] == 'm')
			getMediaValue(Receiver);
		fputs(Receiver, stdout);
		Receiver[0] = '\0';
	}
}
// 处理同步视频播放信号
void getMediaValue(char *Buff)
{

	int len = strlen(Buff);
	char *newStr;
	int i= 0;
	for(i=3;i<len;i++){
		newStr[i-3]=Buff[i];
	}
	newStr[i-3]='\0';
	float num=atof(newStr);
	//对接受的播放信号进行处理
	if(num==-1.0){//1
		if(libvlc_media_player_is_playing(media_player) == 1) {
        		pause_player();
	    	}
	    	else {
			play();
	    	}
	}else if(num == -2.0){
		pause_player();
    		libvlc_media_player_stop(media_player);
	}else{
		libvlc_media_player_set_position(media_player,num);
	}
}
//发送视频同步播放信息
void sendMediaValue(int num){
	char *sender;
	if(num==-1){//1
		sender = ":mv-1";
	}else{
		sender = ":mv-2";
	}
	int messageSize = strlen(sender) + 1;
	write(sockfd, &messageSize, sizeof(int));
 	write(sockfd, sender, messageSize);
}

void sendScaleValue(float value){
	char sender[80];
	sprintf(sender,":mv%f",value);
	int messageSize = strlen(sender) + 1;
	
	write(sockfd, &messageSize, sizeof(int));
 	write(sockfd, sender, messageSize);
	
}

void client_init(){
	int  n, MessageSize; 	//发送消息大小
	
	struct sockaddr_in serv, cli;//服务器与客户端套接口地址数据结构
	char rec[1000];	//接受数据缓冲区
	char send[80];//发送数据缓冲区
	char serAddress[80];//服务器地址
	
	//输入服务器地址
	//回车默认本地
	printf("请输入服务器地址(默认本地): ");
	fgets(serAddress, sizeof(serAddress), stdin);
	if(serAddress[0] == '\n')
	{ 
		strcpy(serAddress, "127.0.0.1\n");
	}
	serAddress[strlen(serAddress) - 1] = '\0';

	//输入用户名
	Start: printf("请输入用户名: " );
	fgets(userName, sizeof(userName), stdin);
	userName[strlen(userName) - 1] = '\0'; //cut the '\n' ending
 	MessageSize = strlen(userName);

	//创建TCP socket
 	sockfd = socket (PF_INET, SOCK_STREAM, 0);

	//创建服务器信息
 	bzero (&serv, sizeof (serv));
	serv.sin_family = PF_INET;
	serv.sin_port = htons (8888);
 	serv.sin_addr.s_addr = inet_addr (serAddress /*server address*/);

	//连接至服务器
	if(connect (sockfd, (struct sockaddr *) &serv, sizeof (struct sockaddr)) == -1)
	{//错误退出
		printf("connect %s failed\n", serAddress);
		exit(1);
	}

	//发送用户名至服务器
	write(sockfd, &MessageSize, sizeof(int));
 	write (sockfd, userName, sizeof(userName));

	//获得连接请求的回复信息
	n = read (sockfd, rec, 1000);
 	rec[n] = '\0';	

	//判断是否连接成功
	if(rec[0] == 'R')
	{
		//名字重复 重新输入
		rec[0] = '\0';
		printf("用户名存在，请换一个.\n");
	 	goto Start; 
	}
	else//连接成功
	{
		fputs(rec, stdout);
		//创建线程 运行send线程函数	
		pthread_create(&SendingThread, 0, Send, &sockfd);
		//创建线程 运行Receive线程函数	
		pthread_create(&ReceivingThread, 0, Receive, &sockfd);
	}
}
void build_interface(){
	// setup window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_title(GTK_WINDOW(window), "GTK+ libVLC Demo");

    //setup box
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, FALSE);
    gtk_container_add(GTK_CONTAINER(window), vbox);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, FALSE);

    //setup menu
    menubar = gtk_menu_bar_new();
    filemenu = gtk_menu_new();
    fileitem = gtk_menu_item_new_with_label ("File");
    filemenu_openitem = gtk_menu_item_new_with_label("Open");
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), filemenu_openitem);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(fileitem), filemenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), fileitem);

    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
    g_signal_connect(filemenu_openitem, "activate", G_CALLBACK(on_open), window);

    //setup player widget
    player_widget = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(vbox), player_widget, TRUE, TRUE, 0);

    //setup controls
    playpause_button = gtk_button_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON);
    stop_button = gtk_button_new_from_icon_name("media-playback-stop", GTK_ICON_SIZE_BUTTON);

    g_signal_connect(playpause_button, "clicked", G_CALLBACK(on_playpause), NULL);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop), NULL);

    hbuttonbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_set_border_width(GTK_CONTAINER(hbuttonbox), BORDER_WIDTH);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_START);

    gtk_box_pack_start(GTK_BOX(hbuttonbox), playpause_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbuttonbox), stop_button, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), hbuttonbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
}
void vlc_init(){
    //setup vlc
    vlc_inst = libvlc_new(0, NULL);
    media_player = libvlc_media_player_new(vlc_inst);
    g_signal_connect(G_OBJECT(player_widget), "realize", G_CALLBACK(player_widget_on_realize), media_player);


	//setup scale
	process_adjuest = gtk_adjustment_new(0.00, 0.00, 100.00, 1.00, 0.00, 0.00);
	process_scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL,process_adjuest);
	gtk_box_pack_start(GTK_BOX(hbox), process_scale, TRUE, TRUE, 0);
	gtk_scale_set_draw_value (GTK_SCALE(process_scale), FALSE);
	gtk_scale_set_has_origin (GTK_SCALE(process_scale), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(process_scale), 5);
	g_signal_connect(G_OBJECT(process_scale),"value_changed", G_CALLBACK(on_value_change), NULL);

    gtk_widget_show_all(window);
    gtk_main ();
    printf("Release Media Player\n");
    libvlc_media_player_release(media_player);
    libvlc_release(vlc_inst);

}
int main ( int argc, char *argv[] )
{
	client_init();//初始化客户端 用户登陆
    gtk_init (&argc, &argv);
	build_interface();//创建播放器界面
	vlc_init();//初始化vlc并运行
	pthread_exit(&SendingThread);
	pthread_exit(&ReceivingThread);
	close(sockfd);
	return 0;
}
