#define LOG_TAG    "oled"


#include "oled.h"
#include <rtthread.h>
#include <math.h>
#include <elog.h>
#include "filter.h"
#include "drv_cpu_temp.h"
#include "drv_cpuusage.h"
#include "drv_oled.h"
#include "stdio.h"
#include "buzzer.h"
#include "key.h"
#include "adc.h"
/* 自定义OLED 坐标系如下: 

	127 ↑y
			---------
			|	      |
			|				|
			|				|
			|				|
			|				|
			|				|
		(0,0)-----→x  
							63
*/
/*---------------------- Constant / Macro Definitions -----------------------*/
	

/*----------------------- Variable Declarations -----------------------------*/


extern struct rt_event init_event;/* ALL_init 事件控制块 */
extern struct SAngle 	stcAngle;

extern struct JY901Type JY901;
extern uint8 VehicleMode;

float slope = 0.0; //东北天坐标系下 航向斜率 slope

char *VehicleModeName[2] = {"AUV","ROV"};
volatile MENU_LIST_e MENU = StatusPage; //OLED初始页面为 状态页. volatile是一种类型修饰符。
																				//volatile 的作用 是作为指令关键字，确保本条指令不会因编译器的优化而省略，且要求每次直接读值。
uint32 total_mem,used_mem,max_used_mem;
/* OLED 变量 初始化. */
OledType oled = {	 
								 .pagenum = StatusPage,		 //页码 pagenum
								 .pagechange = StatusPage,	   //暂存页码 检测页码是否改变 pagechange
								 .pagechange_flag = 0,              //页码改变标志位 pagechange flag
								 .pagename = {	
									"StatusPage",
									"GyroscopePage",
									"FlashPage",
									"PicturePage"} //页名 pagename
								
};
/*----------------------- Function Implement --------------------------------*/

/*******************************************
* 函 数 名：menu_define
* 功    能：菜单选择
* 输入参数：none
* 返 回 值：none
* 注    意：页码切换时，蜂鸣器响一声
********************************************/
void menu_define(void) //菜单定义
{

	if(oled.pagenum >= OLED_Page_MAX || oled.pagenum < StatusPage) oled.pagenum = StatusPage; //超出页面范围 则为第一页
	if(oled.pagechange != oled.pagenum){
			buzzer_bibi(1,1);
			rt_kprintf("Current Menu_Page: %s \n",oled.pagename[oled.pagenum-1]);
			OLED_Clear();
			oled.pagechange_flag = 1;
	}
	else {oled.pagechange_flag = 0;}
	oled.pagechange = oled.pagenum;

	switch(oled.pagenum){
			case 1:{
				MENU = StatusPage;	 OLED_StatusPage();			break;
			}
			case 2:{
				MENU = GyroscopePage;OLED_GyroscopePage();break;
			}
			case 3:{
				MENU = FlashPage;		 											break;
			}
			case 4:{
				MENU = PicturePage;	 OLED_PicturePage();		  break;
			}	
	}
}

/*******************************************
* 函 数 名：oled_thread_entry
* 功    能：OLED线程任务
* 输入参数：none
* 返 回 值：none
* 注    意：菜单号越大 刷新速率越大
********************************************/
void oled_thread_entry(void* parameter)
{
	Boot_Animation();	//开机动画
	OLED_Clear();
	while(1)
	{	
			menu_define();//菜单定义选择
			rt_thread_mdelay(1000/pow(MENU+2,2)); //菜单号越大 刷新速率越大
	}
	
}

/*******************************************
* 函 数 名：OLED_StatusPage
* 功    能：显示系统状态页面
* 输入参数：none
* 返 回 值：none
* 注    意：系统第一页 【状态页】
********************************************/
void OLED_StatusPage(void)
{
		char str[100];
		float temp = 0.0f;  //暂存cpu温度初始采集值
	  uint8 cpu_usage_major, cpu_usage_minor; //整数位、小数位
	
		temp = get_cpu_temp();

		OLED_ShowMyChar(119,0,0,16,1); //3G数据图标2
		//OLED_ShowMyChar(0,32,1,16,1); //Wifi图标
	
		sprintf(str,"Mode: [ %s 00%d ] ",VehicleModeName[VehicleMode],boma_value_get());
		OLED_ShowString(0,0, (uint8 *)str,12); 
	
		sprintf(str,"Voltage:%.2f v  \r\n",get_vol());
		OLED_ShowString(0,16,(uint8 *)str,12); 
	
    cpu_usage_get(&cpu_usage_major, &cpu_usage_minor);

	  sprintf(str,"CPU Usage:%d.%d %% ",cpu_usage_major, cpu_usage_minor);//%字符的转义字符是%%  %这个字符在输出语句是向后匹配的原则
		OLED_ShowString(0,32,(uint8 *)str,12); 
		
		sprintf(str,"Temperature:%.2f C \r\n",KalmanFilter(&temp));//显示卡尔曼滤波后的温度
		OLED_ShowString(0,48,(uint8 *)str,12);
		OLED_Refresh_Gram();//更新显示到OLED
}


/*******************************************
* 函 数 名：OLED_GyroscopePage
* 功    能：显示九轴模块参数【加速度、角速度、欧拉角、磁场】
* 输入参数：none
* 返 回 值：none
* 注    意：OLED第二页 【九轴参数页】
********************************************/
void OLED_GyroscopePage(void)
{
		char str[100];
		sprintf(str,"Acc:%.2f %.2f %.2f  ",JY901.Acc.x,JY901.Acc.y,JY901.Acc.z);
		OLED_ShowString(0,0,(uint8 *)str,12); 	
		
		sprintf(str,"Gyro:%.1f %.1f %.1f ",JY901.Gyro.x,JY901.Gyro.y,JY901.Gyro.z);
		OLED_ShowString(0,16,(uint8 *)str,12); 	
		
		sprintf(str,"Ang:%.1f %.1f %.1f  ",JY901.Euler.Roll,JY901.Euler.Pitch,JY901.Euler.Yaw);
		OLED_ShowString(0,32,(uint8 *)str,12); 	
		 
		sprintf(str,"Mag:%d %d %d  ",JY901.Mag.x,JY901.Mag.y,JY901.Mag.z);
		OLED_ShowString(0,48,(uint8 *)str,12); 
		
	  OLED_Refresh_Gram();//更新显示到OLED
}


/*******************************************
* 函 数 名：OLED_PicturePage
* 功    能：显示OLED电子罗盘图像页面
* 输入参数：none
* 返 回 值：none
* 注    意：OLED第四页 【图像页】
********************************************/
void OLED_PicturePage(void)
{
		static uint8 y=0;
		char str[100];
		static int Angle_x = 0,Angle_y = 0;
		
		draw_fill_circle(31+Angle_x,31+Angle_y,6,0); //清空实心圆，用于刷新坐标
		draw_line(31,31,slope,0); //清除上一次画的线 进行刷新
	
		OLED_Refresh_Gram();//更新显示到OLED
	
		Angle_x = JY901.Euler.Roll/5;
		Angle_y = JY901.Euler.Pitch/5;
		slope = tan((float)Deg2Rad(JY901.Euler.Yaw));  //转化弧度制 解算东北天坐标系下 航向斜率slope
	
		for(y = 28;y <= 36;y++){ //补圆顶、底部的缺失点
				OLED_DrawPoint(y,0,1);
				OLED_DrawPoint(y,63,1);
		}

		draw_line(31,31,slope,1);
		
		sprintf(str,"Rol:%3.1f  ",JY901.Euler.Roll); //横滚角Roll 
		OLED_ShowString(65,0, (uint8 *)str,12);
		
		sprintf(str,"Pit:%3.1f  ",JY901.Euler.Pitch);//俯仰角Pitch
		OLED_ShowString(65,16, (uint8 *)str,12);
		
		sprintf(str,"Yaw:%3.1f  ",JY901.Euler.Yaw); //偏航角Yaw
		OLED_ShowString(65,32, (uint8 *)str,12);
		
		sprintf(str,"k:%.1f   ",slope);
		OLED_ShowString(65,48,(uint8 *)str,12); 
		

		OLED_ShowString(29,2 ,(uint8 *)"N",12);
		OLED_ShowString(29,51,(uint8 *)"S",12);
		OLED_ShowString(3	,28,(uint8 *)"W",12);
		OLED_ShowString(55,28,(uint8 *)"E",12);
		
		draw_circle(31,31,32);//画固定圆
		draw_fill_circle(31+Angle_x,31+Angle_y,6,1); //画实心圆
	
		OLED_Refresh_Gram();//更新显示到OLED						
}



/*******************************************
* 函 数 名：Boot_Animation
* 功    能：开机动画
* 输入参数：none
* 返 回 值：none
* 注    意：none
* 说    明：绘制ETA简易LOGO
********************************************/
void Boot_Animation(void)
{
		static uint8 x=0,y=0;

		for(x = 63;x>=18;x--){
				OLED_DrawPoint(108-0.7*x,x,1);//画斜线 x,y反置
				OLED_DrawPoint(17 +0.7*x,x,1);
				y = 64-x;
				OLED_DrawPoint(64-0.7*y,y,1);
				OLED_DrawPoint(64+0.7*y,y,1);
				rt_thread_delay(2);
			  OLED_Refresh_Gram();//更新显示到OLED
		}
		
		for(x = 30;x <= 94;x++){
				OLED_DrawPoint(125-x,47,1);
				OLED_DrawPoint(x,18,1);
				rt_thread_delay(2);
			  OLED_Refresh_Gram();//更新显示到OLED
		}

		OLED_ShowString(60,20,(uint8 *)"E",16);
	  OLED_Refresh_Gram();//更新显示到OLED
		rt_thread_delay(100);
		
}



/*******************************************
* 函 数 名：draw_fill_circle
* 功    能：绘制填充圆
* 输入参数：(x0,y0)圆心坐标、r为半径
						dot为1时 画实心圆，dot为0时 清空实心圆
* 返 回 值：none
* 注    意：none
********************************************/
void draw_fill_circle(uint8 x0,uint8 y0,uint8 r,uint8 dot)//写画实心圆心(x0,y0),半径r
{	
		uint8 x = 0,y = 0,R = 0;
		for(x = x0-r;x <= x0+r;x++){
				for(y = y0-r; y <= y0+r ;y++ ){
						R = sqrt(pow(r,2)-pow(x-x0,2))+y0; //圆方程  x,y反置		
						if( (y >= y0 && y <= R) || (y < y0 && y >= 2*y0-R )|| 0 == dot ) {  //点限制在 圆方程内	
								OLED_DrawPoint(y,x,dot);
						}	
				}
		}
}

/*******************************************
* 函 数 名：draw_circle
* 功    能：绘制空心圆
* 输入参数：(x0,y0)圆心坐标、r为半径
* 返 回 值：none
* 注    意：none
********************************************/
void draw_circle(uint8 x0,uint8 y0,uint8 r) //圆心(x0,y0),半径r
{
		uint8 x,y;

		for(x = 0;x <= 63;x++){
				y = sqrt(pow(r,2)-pow(x-x0,2))+y0; //圆方程  x,y反置
				OLED_DrawPoint(y,x,1);      //上半圆
				OLED_DrawPoint(63-y,x,1);  //下半圆
		}
		
}


/*******************************************
* 函 数 名：draw_line
* 功    能：画一过固定点线段
* 输入参数：(x0,y0)为固定点坐标、k为斜率
* 返 回 值：none
* 注    意：使用不同坐标系 为了解决函数上 x映射y时只能多对一的关系
						该线段长度为圆方程的半径，将线段长度限制在圆内
********************************************/
void draw_line(uint8 x0,uint8 y0,float k,uint8 dot) //过固定点(x0,y0),斜率k   dot:0,清空;1,填充	  
{
		uint8 x,y;
/* 以下函数使用该坐标系: 
	
	                127 ↑y
			-----------------
			|								|
			|								|
			|								|
			|								|
		←---------------(0,0)x  
			63
*/
		for(x = 0;x <= 63;x++){
				y = sqrt(pow(20,2)-pow(x-31,2))+31+1; //圆方程  x,y反置
			
				if( (JY901.Euler.Yaw >-135 && JY901.Euler.Yaw <-90 ) ||(JY901.Euler.Yaw >90 && JY901.Euler.Yaw < 145 ) || 0 == dot ){ //上半圆
						if(  ((x-x0)/k+y0) >= 31 && ((x-x0)/k+y0) < y ) {  //点限制在 圆方程内
								OLED_DrawPoint(x,((x-x0)/k+y0),dot);}
				}
				
				if( (JY901.Euler.Yaw < -45 && JY901.Euler.Yaw > -90) || (JY901.Euler.Yaw < 90 && JY901.Euler.Yaw> 45) || 0 == dot  ){ //上半圆
						if(  ((x-x0)/k+y0) <= 31 && ((x-x0)/k+y0)> 63-y ) {  //点限制在 圆方程内
								OLED_DrawPoint(x,((x-x0)/k+y0),dot);}
				}
		}
/* 以下函数使用该坐标系: 
	127 ↑y
			---------
			|	      |
			|				|
			|				|
			|				|
			|				|
			|				|
			|				|
		(0,0)-----→x  
							63
*/
		for(x = 0;x <= 63;x++){
				y = sqrt(pow(20,2)-pow(x-31,2))+31+1; //圆方程  x,y反置
		
				if( (JY901.Euler.Yaw>=-45 && JY901.Euler.Yaw <= 0) || (JY901.Euler.Yaw >=-180 && JY901.Euler.Yaw <= -135)  || 0 == dot ){  // JY901.Angle[2] < 0
						if( (k*(x-x0)+y0) >= 31 && (k*(x-x0)+y0) < y ) {  //点限制在 圆方程内   上半圆
								OLED_DrawPoint((k*(x-x0)+y0),x,dot);}
				}
				
				if( (JY901.Euler.Yaw > 0 && JY901.Euler.Yaw <= 45) || (JY901.Euler.Yaw >=135 && JY901.Euler.Yaw <= 180)  || 0 == dot ){  // JY901.Angle[2] < 0
						if(((k*(x-x0)+y0)< 31 && (k*(x-x0)+y0) > 63-y)) {  //点限制在 圆方程内  下半圆
								OLED_DrawPoint((k*(x-x0)+y0),x,dot);} 
				}
		}

}
/* OLED 线程初始化 */
int oled_thread_init(void)
{
    rt_thread_t oled_tid;
		/*创建动态线程*/
    oled_tid = rt_thread_create("oled", //线程名称
                    oled_thread_entry,	//线程入口函数【entry】
                    RT_NULL,				    //线程入口函数参数【parameter】
                    1024,							  //线程栈大小，单位是字节【byte】
                    15,								  //线程优先级【priority】
                    10);							  //线程的时间片大小【tick】= 100ms

    if (oled_tid != RT_NULL){
				OLED_Init();
				log_i("OLED_Init()");
				rt_thread_startup(oled_tid);
				rt_event_send(&init_event, OLED_EVENT);
				oled.pagechange = oled.pagenum;  //初始化暂存页面
		}
		return 0;
}
INIT_APP_EXPORT(oled_thread_init);



/* OLED 下一页 */
void next_oled_page(void)
{
		oled.pagenum ++;
}
MSH_CMD_EXPORT(next_oled_page,next_oled_page[page++]);

/* OLED 上一页 */
void last_oled_page(void)
{
		oled.pagenum --;
}
MSH_CMD_EXPORT(last_oled_page,last_oled_page[page--]);


void get_slope(void)
{
		char str[100];
		sprintf(str,"k= %f\n",slope);
		rt_kprintf(str);
}
MSH_CMD_EXPORT(get_slope,get_slope[k]);


void get_cpuusage(void)
{
		rt_memory_info(&total_mem, &used_mem, &max_used_mem);
    rt_kprintf("Total_Mem:%ld  Used_Mem:%ld  Max_Used_Mem:%ld\n",total_mem,used_mem,max_used_mem);

}
MSH_CMD_EXPORT(get_cpuusage,get_cpuusage);
//STM32F405RGT6   Total_Mem:131048  Used_Mem:22044  Max_Used_Mem:22368

