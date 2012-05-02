#include <reg52.h>
#include <intrins.h>
#include <string.h>

#define uint unsigned int
#define uchar unsigned char
#define SERIAL_MAX	100	//串口数据返回最大值
#define NUM_MAX		75	//保存血压心率数组最大值9*11+2+1
#define SMS_TXT	20	//短信文本最大值
/**
 * M580发送AT指令后所处状态
 */
#define RST			0//模块已恢复出厂设置
#define AT			1//模块能发送AT指令
#define CMGF_1		2//设置短信文本模式
#define CSCS_GSM	3//设置TE的字符集为GSM
#define CMGS		4
#define SMS_SUC		5//短信发送成功
#define CSMS		6//短信服务初始化为Phase2+

/**
 * 定义P0口各位使用
 */
sbit COM_A = P0^0;	//com0~com3
sbit COM_B = P0^1;
sbit BP_A = P0^2;	//blood-pressure & pulse
sbit BP_B = P0^3;
sbit BP_C = P0^4;
sbit ODD_SEG = P0^5;
sbit EVEN_SEG = P0^6;
sbit MEM_SEG = P0^7;
/**
 * 定义P1口各位使用
 */
sbit MEM_KEY = P1^0;
sbit ON_KEY = P1^2;

uchar data i, j, com_dat;	//全局变量，频繁使用
uchar data M580_AT;	//M580 AT指令控制变量
uchar *ptr;
/**
 * digit-LCD屏数码显示的位数；高压百位到心率个位
 * seg-每一位数码中段位：b a g f c e d
 * num-储存数码字形码数组位
 */
uchar data digit, seg, num;
uchar idata ip_get[16];
uchar idata font[NUM_MAX];	//字形码数组

/*buffer[100]用来存储串口发送的数据，com_dat用来记录串口发送的个数*/
uchar xdata buffer[SERIAL_MAX];	//从串口接收的数据

/**
 * 需要使用的常量数据
 */
uchar code *IP_PHONE = "";	//设置IP输入手机号
uchar code *BP = "##11011110#10001001#10001000#11011110#11001000#00000000#11111011#01111110#";
uchar code *BP1 = "##10001001#11011110#10001000#11011110#11001000#00000000#11111011#01111110#";
uchar code *INFO = "AT+CNUM: \"BillRyan\",\"13800000000\",129";

void delayms(uint xms);		//延时函数（毫秒级）
void serial_init(void);		//串口初始化
void send_txd(uchar ch);	//向串口发送单个字符
void send_str(uchar *str);	//发送字符串
void sms_text(uchar *phone, uchar *text);//发送短信文本
void sms_text_init(void);	//文本短信使用初始化
void gprs_init(void);		//GPRS使用初始化
void tcp_send(uchar *ip, uchar *dat);	//发送tcp数据包
void send_bp(uchar *bp);	//BP
void int0_init(void);	//初始化外部中断
void M580_MEM(void);
void receive_IP(void);		//接收IP地址

void main()
{
	delayms(5000);
	//M580_MEM();
	serial_init();
	sms_text_init();
	receive_IP();
	gprs_init();
	while(1)
	{
		send_bp(BP);
		send_bp(BP1);
		tcp_send(ip_get,BP);
	}
}/* end function main */

void M580_MEM(void)
{
	MEM_KEY = 0;
	delayms(500);
	MEM_KEY = 1;
	delayms(500);
}


void send_bp(uchar *bp)
{
	/* AT_TCPSETUP */
	send_str("AT+TCPSETUP=0,117.39.56.158,9090\r");
	delayms(1500);
	send_str("AT+TCPSEND=0,74\r");
	delayms(2000);
	if(*(bp+1) != '#')
	{
		send_txd('#');
	}
	send_str(bp);
	send_txd(0x0D);
	delayms(2000);
}

void gprs_init(void)
{
	/* 选择使用M580内部TCP/IP协议栈 */
	send_str("AT+XISP=0\r");
	delayms(300);
	/* 设置PDP格式为中国移动 */
	send_str("AT+CGDCONT=1,\"IP\",\"CMNET\"\r");
	delayms(1000);
	/* 建立PPP连接 */
	send_str("AT+XIIC=1\r");
	delayms(1000);	
}/* end function gprs_init() */

void tcp_send(uchar *ip, uchar *dat)
{
	uchar b, s, g;
	uint n = 0;
	while(*dat)
	{
		n++;
		dat++;
	}
	b = n/100 + 48;
	s = n%100/10 + 48;
	g = n%10 + 48;
	/* 在链路0上建立到指定IP的连接 */
	send_str("AT+TCPSETUP=0,");
	send_str(ip);
	send_str(",9090\r");
	delayms(1500);
	/* 在链路0上发送数据 */
	send_str("AT+TCPSEND=0,");
	if('0' != b)
	{
		send_txd(b);
		send_txd(s);
		send_txd(g);
	}
	else if('0' != s)
	{
		send_txd(s);
		send_txd(g);
	}
	else
		send_txd(g);
	send_txd(0x0D);
	delayms(2000);
	send_str(dat);
	send_txd(0x0D);
	delayms(2000);
}


void receive_IP(void)
{
	uchar ipflag = 1;
	send_str("AT+CMGL=4\r");
	delayms(2000);
	sms_text("15332474460", "IP address?");
	com_dat = 0;
	while(0 != ipflag)
	{
		delayms(3000);
		if(10 < com_dat)
		{
			while('\n' != buffer[com_dat])
			{
				com_dat++;
			}
			com_dat++;
			i = 0;
			while('\r' != buffer[com_dat])
			{
				ip_get[i++] = buffer[com_dat++];
			}
			ip_get[i] = '\0';
			ipflag = 0;
		}
	}
}

void send_txd(uchar ch) 
{ 
	SBUF = ch;	
	while(!TI);
	TI = 0; 
}/* end function send_txd() */

void send_str(uchar *str) 
{
	while(*str)
	{
		send_txd(*str);
		str++;
	}
}/* end function send_str() */

void sms_text_init(void)
{
	/* 设置文本模式 */
	send_str("AT+CMGF=1\r");
	delayms(50);
	/* 选择TE的字符集为GSM */
	send_str("AT+CSCS=\"GSM\"\r");
	delayms(50);
	/* 首选短信存储器均为SIM */
	send_str("AT+CPMS=\"SM\",\"SM\",\"SM\"\r");
	delayms(50);
	/* 设置新短信提示方式，存储在SIM卡中 */
	send_str("AT+CNMI=2,1,0,0,0\r");
	delayms(50);
}/* end function sms_text_init() */

/* 选择目的手机号以及发送文本信息 */
void sms_text(uchar *phone, uchar *text)
{
	send_str("AT+CMGS=\"");
	send_str(phone);
	send_txd(0X22);	//发送双引号
	send_txd(0x0D);	//发送回车符号
	delayms(300);
	send_str(text);
	send_txd(0x1A);	//发送Ctrl+Z结束符
	delayms(1000);
}/* end function sms_text() */

void delayms(uint xms)
{
	uint i1,j1;
	for(i1 = xms; i1 > 0; i1--)
		for(j1 = 113; j1 > 0; j1--);
}/* end function delayms() */

void serial_init(void)
{
	com_dat = 0;
	TMOD = 0x21;//设定定时器方式2
	SCON = 0x50;//串口工作在方式1，并且启动串行接收
	TH1 = 0xFD;	//设置波特率9600bps
	TL1 = 0xFD;
	TR1 = 1;	//启动T1定时器
	ES = 1;		//开串口中断
	delayms(300);
}/*  end function serial_init() */

void serial_int(void) interrupt 4
{
	EA = 0;		//关总中断
	if(1 == RI)	//当硬件接收到一个数据时，RI会置高位
	{
		buffer[com_dat++] = SBUF;	//存取串口接收的数据
		RI = 0;	//软件置RI为0
		buffer[com_dat] = '\0';
		if(SERIAL_MAX - 1 == com_dat)
			com_dat = 0;	//防止数组溢出
	}
	EA = 1;		//开总中断
}/*  end function serial_int() */

void int0_init(void)
{
	digit = 0;	//数码管位数（共显示8位）
	seg = 4;	//防止进入T0中断读取数据
	num = 0;	//初始化字形码数组
	j = 0;		//外部中断信号标志位
	/**
	 * 定时器0初始化
	 * 工作方式1，初值1.953ms，定时3.905ms
	 */
	TMOD = 0x21;
	TH0 = 0xF8;
	TL0 = 0xF8;
	ET0 = 1;	//定时器0中断允许
	/* end timer 1 initialize */

	IT0 = 1;	//引脚下降沿有效
	EX0 = 1;	//打开外部中断1
}/* end fuction int0_init */

void INT0_int(void) interrupt 0
{	
	/**
	 * 定时器0初始化
	 * 工作方式1，初值1.953ms，定时3.905ms
	 */
	TH0 = 0xF8;
	TL0 = 0xF8;
	ET0 = 1;	//定时器0中断允许
	TR0 = 1;	//启动定时器0
	/* end timer 1 initialize */
	if(j < 2)
	{
		j++;
	}
	else
	{
		if(digit < 8)
		{
			BP_C = digit/4;
			BP_B = digit%4/2;
			BP_A = digit%2;
			digit++;
			seg = 0;
		}
		else
		{
			font[num] = '\0';
			EX0 = 0;	//关闭外部中断INT0
			ET0 = 0;	//关闭定时器0中断
			TR0 = 0;  	//关闭定时器0
		}
	}
}/* end function INT0_int() */

void T0_int(void) interrupt 1
{
	TH0 = 0xF1;	//重装初值，定时3.905ms
	TL0 = 0xF1;
	if(seg < 4)
	{
		COM_B = seg/2;
		COM_A = seg%2;
		/* 4051与LM324输出信号稳定后再读数 */
		for(i = 200; i > 0; i--)
		{
			 _nop_();
		}

		font[num++] = EVEN_SEG;
		font[num++] = ODD_SEG;
		seg++;
	}
	else
	{
		ET0 = 0;
		TR0 = 0;	//8段全读完后关T0
		font[num++] = '#';
	}
}/* end function int0_int */
