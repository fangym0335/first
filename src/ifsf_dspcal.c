/*******************************************************************
计算器操作函数
-------------------------------------------------------------------
2007-07-13 by chenfm

2008-03-06 - Guojie Zhu <zhugj@css-intelligent.com>
    gpIfsf_shm->convertUsed[] 改为 gpIfsf_shm->convert_hb[].lnao.node
    修改C_DATA的一些Data_Id的缺省设置
2008-03-12 - Guojie Zhu <zhugj@css-intelligent.com>
    修正ReadC_DATA回复数据的格式问题
*******************************************************************/
#include "ifsf_dspcal.h"
#include "ifsf_pub.h"
//#include "pub.h"

extern IFSF_SHM *gpIfsf_shm; //该结构是全局的量，只有一个,在ifsf_main.c中定义.

//FP_C_DATA操作函数. node为加油机逻辑节点号,要用到gpIfsf_shm,后面定义.
//默认初始化计算器.	监听模式,加油点,油品,计量器和加油模式各一个.
int DefaultC_DATA(const char node){
	//int index;
	int i, fp_state;
	DISPENSER_DATA *dispenser_data;
	FP_C_DATA *fp_c_data;
	NODE_CHANNEL *pNode_channel;
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计算器初始化失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计算器初始化失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计算器初始化失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	//index = IFSF_DP_SEM_INDEX + i;
	fp_c_data =  (FP_C_DATA *)( &(dispenser_data->fp_c_data.Nb_Products) );
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if(fp_state < dispenser_data->fp_data[i].FP_State)
			fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) 
			return -1;		
	}
	if( fp_state >3 ) 
		return -1;	
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->node_channel[i].node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点通道数据找不到,计算器初始化失败");
		return -1;
	}
	pNode_channel = (NODE_CHANNEL *)(&(gpIfsf_shm->node_channel[i]) );
	//Act_P(index);
	//(unsigned char)
	fp_c_data->Nb_Products = pNode_channel->cntOfProd;//'\x01';
	fp_c_data->Nb_Fuelling_Modes = '\x01';
	fp_c_data->Nb_Meters = '\x01';
	fp_c_data->Nb_FP =pNode_channel->cntOfFps; // '\x01';
	fp_c_data->Country_Code[0] ='\x0' ; //'\x00'; from bulliten, use new ifsf Country_Code of China
	fp_c_data->Country_Code[1] = '\x86'; //'\x86';0086 is old  ifsf Country_Code of China
	fp_c_data->Blend_Tolerance = '\0';
	//fp_c_data->Clear_Display_Mode = '\0';
	
	//Default value of Auth_State_Mode and Max_Auth_Time is 0(auto auth).in  Full Mode Auth_State_Mode is 1.
//	fp_c_data->Auth_State_Mode = ( DEFAULT_AUTH_MODE == 0 ? 1:0);
	fp_c_data->Auth_State_Mode = 0; 
	//fp_c_data->Stand_Alone_Auth = '\0';
	fp_c_data->Max_Auth_Time = '\0'; //Work in  no time limited authrized Mode.
	
	fp_c_data->Max_Time_W_O_Prog = '\01'; 
	fp_c_data->Min_Fuelling_Vol = '\0' ; //change to fuelling status from started status at once.
	fp_c_data->Min_Display_Vol = '\0' ; 
	fp_c_data->Min_Guard_Time = '\0' ;
	fp_c_data->Pulser_Err_Tolerance = '\0';
	fp_c_data->Digits_Vol_Layout = '\x68';	//8 number, 2 decimal.
	fp_c_data->Digits_Amount_Layout = '\x68';
	fp_c_data->Digits_Unit_Price = '\x46';
	fp_c_data->Unit_Price_Mult_Fact = '\x0'; // 基本货币单位与单价货币单位的10进制倍率, 元/元
	fp_c_data->Amount_Rounding_Type[0] = '\0';
	fp_c_data->Amount_Rounding_Type[1] = '\x01'; //0001
	fp_c_data->Preset_Rounding_Amount = '\01';
	strncpy(fp_c_data->Manufacturer_Id, "PTR", 3);
	strncpy(fp_c_data->Dispenser_Model, "PTR", 3);
	strncpy(fp_c_data->Calculator_Type, "PTR", 3);
	memset(fp_c_data->Calculator_Serial_No, 32, 12); //0-11, By Default, Set to Space
	bzero(fp_c_data->Appl_Software_Ver, sizeof(fp_c_data->Appl_Software_Ver));
	strncpy(fp_c_data->Appl_Software_Ver, "PCDCSS", 6);
	fp_c_data->Appl_Software_Ver[10] = '\x01';
	fp_c_data->Appl_Software_Ver[11] = '\0'; //V1.00
	//fp_c_data->W_M_Software_Ver[0];//1-5
	//fp_c_data->W_M_Software_Date[0]; //1-3? date?
	fp_c_data->W_M_Security_Type = '\x0'; // no security type
	fp_c_data->Protocol_Ver[0] = '\x20';
	fp_c_data->Protocol_Ver[1] = '\x07';
	fp_c_data->Protocol_Ver[2] = '\x11';
	fp_c_data->Protocol_Ver[2] = '\x00';
	fp_c_data->Protocol_Ver[2] = '\x02';
	fp_c_data->Protocol_Ver[2] = '\x25'; //V2.25
	fp_c_data->SW_Change_Date[0]='\x20';
	fp_c_data->SW_Change_Date[1]='\x07';
	fp_c_data->SW_Change_Date[2]='\x12';
	fp_c_data->SW_Change_Date[3]='\x01'; //2007.12.01
	//fp_c_data->SW_Change_Personal_Nb[]; //0-7
	fp_c_data->SW_Checksum[0] = '0';     // 默认 0000
	fp_c_data->SW_Checksum[1] = '0';
	fp_c_data->SW_Checksum[2] = '0';
	fp_c_data->SW_Checksum[3] = '0';
	fp_c_data->Calc_Illumination = 1;
	fp_c_data->W_M_Polynomial[0] = '\x0'; // no security
	fp_c_data->W_M_Polynomial[1] ='\x0';
	fp_c_data->W_M_Seed[0] = '\0';
	fp_c_data->W_M_Seed[1] = '\0';
	//Act_V( index);
	return 0;	
}
int SetC_DAT(const char node, const FP_C_DATA *fp_c_dat){//设置计算器.根据型号设置.
	int index;
	int i, fp_state;
	DISPENSER_DATA *dispenser_data;
	FP_C_DATA *pfp_c_data;
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计算器set 失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计算器set 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计算器set 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	index = IFSF_DP_SEM_INDEX + i;
	pfp_c_data =  (FP_C_DATA *)( &(dispenser_data->fp_c_data) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) return -1;		
	}
	if(NULL == fp_c_dat){
		UserLog("给定的加油机计算器数据为空,计算器set 失败");
		return -1;
		}
	Act_P(index);
	memcpy(pfp_c_data, fp_c_dat, sizeof(FP_C_DATA) );
	Act_V(index);
	return 0;
}
//读写FP_C_DAT函数. 注意要构造的信息数据的类型,000读,001回答,010写,011主动需应答,100主动无需应答,111应答
//recvBuffer为收到的data,sendBuffer为要发送的data,msg_lg为生成的总长度.
//读取FP_C_DAT. FP_C_DATA地址为1.构造的信息数据的类型为001回答
int ReadC_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	//int index;
	int i, fp_state = 0 , recv_lg, tmp_lg = 0;
	DISPENSER_DATA *dispenser_data;
	FP_C_DATA *pfp_c_data;
	
	unsigned char node= recvBuffer[1];
	unsigned char buffer[256];
	memset(buffer, 0 , 256);
	if(  '\x01' != recvBuffer[0]  ){		
		UserLog("参数错误,不是加油机的数据,计算器Read 失败");
		return -1;
	}
	if( (NULL == recvBuffer) || (NULL == sendBuffer) || (NULL == msg_lg)  ){		
		UserLog("参数错误,计算器read 失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计算器read 失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计算器read 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计算器read 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	//index = IFSF_DP_SEM_INDEX + i;
	pfp_c_data =  (FP_C_DATA *)( &(dispenser_data->fp_c_data) );
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if( fp_state < (dispenser_data->fp_data[i].FP_State) ) 
			fp_state = (dispenser_data->fp_data[i].FP_State) ;
		//if( fp_state == 0 ) return -1;
	}
	
	buffer[0] = recvBuffer[2];
	buffer[1] = recvBuffer[3];
	buffer[2] = recvBuffer[0];
	buffer[3] = recvBuffer[1];
	buffer[4] = '\0'; //recvBuffer[4];
	buffer[5] = ( (recvBuffer[5] &0x1f)  |0x20); //answer
	recv_lg = recvBuffer[6];
	recv_lg = recv_lg * 256 + recvBuffer[7];
	buffer[8] = '\x01'; //recvBuffer[8];
	buffer[9] = '\x01'; //recvBuffer[9];
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg = 9;
	for(i=0; i < (recv_lg - 2); i++){
		switch( recvBuffer[10+i] ){
		case C_Nb_Products:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Nb_Products;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Nb_Products;
			break;
		case C_Nb_Fuelling_Modes: 
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Nb_Fuelling_Modes;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Nb_Fuelling_Modes;
			break;
		case C_Nb_Meters:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Nb_Meters;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Nb_Meters;
			break;
		case C_Nb_FP:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Nb_FP;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Nb_FP;
			break;
		case C_Country_Code:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Country_Code;
			tmp_lg++;
			buffer[tmp_lg] ='\x02';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Country_Code[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Country_Code[1];
			break;
		case C_Blend_Tolerance:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Blend_Tolerance;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Blend_Tolerance;
			break;
		case C_Drive_Off_Lights_Mode:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Drive_Off_Lights_Mode;
			tmp_lg++;
			buffer[tmp_lg] ='\0';
			break;
		case C_OPT_Light_Mode:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_OPT_Light_Mode;
			tmp_lg++;
			buffer[tmp_lg] ='\0';
			break;
		case C_Clear_Display_Mode:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Clear_Display_Mode;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Clear_Display_Mode;
			break;
		case C_Auth_State_Mode:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Auth_State_Mode;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Auth_State_Mode;
			break;
		case C_Stand_Alone_Auth:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Stand_Alone_Auth;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Stand_Alone_Auth;
			break;
		case C_Max_Auth_Time:/*!!*/
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Max_Auth_Time;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Max_Auth_Time;
			break;
		case C_Max_Time_W_O_Prog ://= 21
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Max_Time_W_O_Prog;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Max_Time_W_O_Prog;
			break;
		case C_Min_Fuelling_Vol:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Min_Fuelling_Vol;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Min_Fuelling_Vol;
			break;
		case C_Min_Display_Vol:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Min_Display_Vol ;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Min_Display_Vol;
			break;
		case C_Min_Guard_Time:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Min_Guard_Time ;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Min_Guard_Time;
			break;
		case C_Pulser_Err_Tolerance:/* = 26*/
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Pulser_Err_Tolerance ;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Pulser_Err_Tolerance;
			break;
		case C_Time_Display_Product_Name:/* = 28*/
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Time_Display_Product_Name ;
			tmp_lg++;
			buffer[tmp_lg] ='\0'; //O, no surport.
			break;
		case C_Digits_Vol_Layout: /*= 40*/ 
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Digits_Vol_Layout ;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Digits_Vol_Layout;
			break;
		case C_Digits_Amount_Layout:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Digits_Amount_Layout ;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Digits_Amount_Layout;
			break;
		case C_Digits_Unit_Price:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Digits_Unit_Price ;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Digits_Unit_Price;
			break;
		case C_Unit_Price_Mult_Fact:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Unit_Price_Mult_Fact ;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Unit_Price_Mult_Fact;
			break;
		case C_Amount_Rounding_Type:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Amount_Rounding_Type;
			tmp_lg++;
			buffer[tmp_lg] ='\x02';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Amount_Rounding_Type[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Amount_Rounding_Type[1];
			break;
		case C_Preset_Rounding_Amount:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Preset_Rounding_Amount ;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Preset_Rounding_Amount;
			break;
		case C_Price_Set_Nb: //O
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Price_Set_Nb ;
			tmp_lg++;
			buffer[tmp_lg] ='\0';
			break;
		case C_Manufacturer_Id:// = 50
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Manufacturer_Id ;
			tmp_lg++;
			buffer[tmp_lg] ='\x03';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Manufacturer_Id[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Manufacturer_Id[1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Manufacturer_Id[2];
			break;
		case C_Dispenser_Model:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Dispenser_Model ;
			tmp_lg++;
			buffer[tmp_lg] ='\x03';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Dispenser_Model[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Dispenser_Model[1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Dispenser_Model[2];
			break;
		case C_Calculator_Type:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Calculator_Type  ;
			tmp_lg++;
			buffer[tmp_lg] ='\x03';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Type[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Type[1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Type[2];
			break;
		case C_Calculator_Serial_No:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Calculator_Serial_No  ;
			tmp_lg++;
			buffer[tmp_lg] ='\x0C';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[3];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[4];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[5];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[6];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[7];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[8];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[9];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[10];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calculator_Serial_No[11];			
			break;
		case C_Appl_Software_Ver:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Appl_Software_Ver;
			tmp_lg++;
			buffer[tmp_lg] ='\x0C';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [3];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [4];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [5];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [6];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [7];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [8];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [9];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [10];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Appl_Software_Ver [11];		
			break;
		case C_W_M_Software_Ver:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_W_M_Software_Ver    ;
			tmp_lg++;
			buffer[tmp_lg] ='\x06';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Software_Ver [0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Software_Ver [1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Software_Ver [2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Software_Ver [3];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Software_Ver [4];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Software_Ver [5];
			break;
		case C_W_M_Software_Date:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_W_M_Software_Date    ;
			tmp_lg++;
			buffer[tmp_lg] ='\x04';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Software_Date [0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Software_Date [1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Software_Date [2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Software_Date [3];
			break;
		case C_W_M_Security_Type:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_W_M_Security_Type ;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->W_M_Security_Type;
			break;
		case C_Protocol_Ver:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Protocol_Ver;
			tmp_lg++;
			buffer[tmp_lg] ='\x06';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Protocol_Ver [0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Protocol_Ver [1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Protocol_Ver [2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Protocol_Ver [3];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Protocol_Ver [4];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Protocol_Ver [5];
			break;
		case C_SW_Change_Date :	
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_SW_Change_Date ;
			tmp_lg++;
			buffer[tmp_lg] ='\x04';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Date [0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Date [1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Date [2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Date [3];
			break;
		case C_SW_Change_Personal_Nb :
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_SW_Change_Personal_Nb;
			tmp_lg++;
			buffer[tmp_lg] ='\x07';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Personal_Nb [0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Personal_Nb [1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Personal_Nb [2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Personal_Nb [3];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Personal_Nb [4];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Personal_Nb [5];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Change_Personal_Nb [6];
			break;
		case C_SW_Checksum :
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_SW_Checksum;
			tmp_lg++;
			buffer[tmp_lg] ='\x04';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Checksum [0];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Checksum [1];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Checksum [2];
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->SW_Checksum [3];
			break;
		case C_Calc_Illumination: // = 70
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Calc_Illumination;
			tmp_lg++;
			buffer[tmp_lg] ='\x01';
			tmp_lg++;
			buffer[tmp_lg] = pfp_c_data->Calc_Illumination;
			break;
		case C_LCD_Backlight_Switch:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_LCD_Backlight_Switch;
			tmp_lg++;
			buffer[tmp_lg] ='\0';
			break;
		case C_Display_Intensity:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Display_Intensity;
			tmp_lg++;
			buffer[tmp_lg] ='\0';
			break;
		case C_W_M_Polynomial:// =80  // Write Only
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_W_M_Polynomial;
			tmp_lg++;
			buffer[tmp_lg] ='\x00';
			break;
		case C_W_M_Seed:              // Write Only
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_W_M_Seed;
			tmp_lg++;
			buffer[tmp_lg] ='\x00';
			break;
		default:
			tmp_lg++;
			buffer[tmp_lg] =recvBuffer[i+10] ;
			tmp_lg++;
			buffer[tmp_lg] ='\0';
			break;
		}
	}

	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ;
	buffer[7] = (unsigned char )((tmp_lg-8) % 256);  //big end.
	*msg_lg= tmp_lg;
	memcpy(sendBuffer, buffer, tmp_lg);	

	return 0;
}


//写FP_C_DATA函数. 构造的信息数据的类型为111应答.
//msg_lg为生成的总长度,为0表示不需要应答信息,sendBuffer和msg_lg不需要应答可以设置为NULL,.
int WriteC_DATA(const unsigned char *recvBuffer, unsigned char *sendBuffer, int *msg_lg){
	int ret;
	int i,  j, lg;
	int  fp_state = 0 , recv_lg, tmp_lg = 0;	
	int cmd_lg;
	DISPENSER_DATA *dispenser_data;
	FP_C_DATA *pfp_c_data;	
	unsigned char tmpMSAck, tmpDataAck;
	unsigned char node=recvBuffer[1];
	unsigned char buffer[512], cmd[10];
	
	memset(buffer, 0 , sizeof(buffer) );
	memset(cmd, 0 , sizeof(cmd));
	if( '\x01' != recvBuffer[0]  ){		
		UserLog("参数错误,不是本加油机的数据,计算器Write 失败");
		return -1;
	}
	if( NULL == recvBuffer ){		
		UserLog("参数错误,计算器Write 失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计算器Write 失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计算器Write 失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计算器Write 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	//index = IFSF_DP_SEM_INDEX + i;
	pfp_c_data =  (FP_C_DATA *)( &(dispenser_data->fp_c_data) );
	fp_state = 1;
	for(i=0; i < MAX_FP_CNT; i++){ 
		if(fp_state < dispenser_data->fp_data[i].FP_State)
			fp_state = dispenser_data->fp_data[i].FP_State;
		if( fp_state >9 ) 
			return -1;		
	}
	//UserLog("Write C_DAT of %d", node);
	
	buffer[0] = recvBuffer[2]; //LNAR
	buffer[1] = recvBuffer[3]; //LNAR
	buffer[2] = recvBuffer[0]; //LNAO
	buffer[3] = recvBuffer[1]; //LNAO
	buffer[4] = '\0'; //recvBuffer[4]; //IFSF_MC
	buffer[5] = ( (recvBuffer[5] &0x1f) |0xE0); //M_St
	recv_lg = recvBuffer[6];  //M_Lg, big end mode.
	recv_lg = recv_lg * 256 + recvBuffer[7]; 
	buffer[8] = '\x01'; //recvBuffer[8]; //Data_Ad_Lg
	buffer[9] = '\x01'; //recvBuffer[9]; //Data_ad:C_DAt.
	//buffer[10] = ; //MS_Ack
	//buffer[11] = recvBuffer[10]; 
	// recvBuffer[10]:Data_Id, 11:Data_Lg, 12:Data_El.  Data begin here.
	tmp_lg = 10;
	i=0;
	tmpMSAck = '\0'; // all is  ok.
	while( i < (recv_lg -2) ) {
		tmpDataAck = '\0'; //ok ack.
		lg =  (int )recvBuffer[11+i]; //Data_Lg.
		switch(recvBuffer[10+i]) {
		case C_Nb_Products: //(1-8),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Nb_Products;
			if ((lg == 1) && (fp_state < 3) && (recvBuffer[12 + i] >= 1) &&
							(recvBuffer[12 + i] <= FP_FM_CNT)) {
				//Act_P(index);
				pfp_c_data->Nb_Products = recvBuffer[12 + i];
				//Act_V(index);
			} else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				tmpMSAck = '\x05'; 
				//UserLog("write  C_DAT's Nb_Products error!fp state not in 1,2. ") 
			} else  { 
				tmpDataAck = '\x01';
				tmpMSAck = '\x05';
				//UserLog("write  C_DAT's Nb_Products error!too big or small.") 
			}
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Nb_Fuelling_Modes: //(1-8),w(1-2), but now only 1 mode.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Nb_Fuelling_Modes;
			if ((lg == 1) && (fp_state < 3) && (recvBuffer[12 + i] >= 1) &&
						(recvBuffer[12 + i] <= FP_FM_CNT)) {
				pfp_c_data->Nb_Fuelling_Modes = recvBuffer[12 + i];
			} else  if (fp_state >= 3)  {
				tmpDataAck = '\x02';
				tmpMSAck = '\x05';
			} else {
				tmpDataAck = '\x01'; 
				tmpMSAck = '\x05';
			}

			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Nb_Meters://(1-16),w(1-2). 
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Nb_Meters;
			if( ( lg== 1 )  && (fp_state < 3) && (recvBuffer[12+i] >= 1) && (recvBuffer[12+i] <= 16) )
				pfp_c_data->Nb_Meters = recvBuffer[12+i];
			else  if (fp_state >= 3)  {
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
			} else { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
			}
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Nb_FP:
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Nb_FP;
			/*
			if( (lg == 1 ) && (fp_state < 3) && (recvBuffer[12+i] >= 1) && (recvBuffer[12+i] <= 4) )
				pfp_c_data->Nb_FP = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Country_Code://w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Country_Code;
			/*
			if( (lg == 2 ) && (fp_state < 3) )
				for(j=0; j<lg; j++)  pfp_c_data->Country_Code[j] = recvBuffer[12+i+j]; //lg=2.there use lg for change.
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02'; 
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Blend_Tolerance: //(0-99),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Blend_Tolerance;
			/*
			if(  (lg == 1 ) && (fp_state < 3)  && (  (recvBuffer[12+i] & 0x0f) >= 0 ) && (  (recvBuffer[12+i]& 0x0f)<= 9  )  \
				&& ( (recvBuffer[12+i]>>4 & 0x0f)>= 0 ) && (  (recvBuffer[12+i]>>4& 0x0f) <= 9  )    ) 
				pfp_c_data->Blend_Tolerance = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';				
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Drive_Off_Lights_Mode: //O
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Drive_Off_Lights_Mode;
			tmpDataAck = '\x04'; //not exist
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_OPT_Light_Mode: //O
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_OPT_Light_Mode;
			tmpDataAck = '\x04';  // not exist
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Clear_Display_Mode: //w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Clear_Display_Mode;
			/*
			if( (lg == 1 ) && (fp_state < 3) )
				pfp_c_data->Clear_Display_Mode = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Auth_State_Mode: //(0-1),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Auth_State_Mode;
			
			if(  (lg == 1 ) && (fp_state < 3) && (fp_state > 0) && \
				(  (recvBuffer[12+i] == 0) || (recvBuffer[12+i] == 1 )  )   ){				
				//pfp_c_data->Auth_State_Mode = recvBuffer[13+i];
				cmd[0] = '\x19';
				cmd[1] = '\x00';
				cmd[2] = recvBuffer[12+i] ;
				cmd_lg =3;
				ret = SendMsg(ORD_MSG_TRAN, cmd , cmd_lg, node, 3);
				if(ret < 0) {
					tmpDataAck = '\x06';  
					tmpMSAck = '\x05';					
				}
				else{
					sleep(1); //test?
					if(pfp_c_data->Auth_State_Mode != recvBuffer[12+i] ) {
						tmpDataAck = '\x06';  
						tmpMSAck = '\x05';
						}
				}
			}
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x03';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x05';  
				tmpMSAck = '\x05';
				}
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Stand_Alone_Auth: //(0-1),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Stand_Alone_Auth;
			/*
			if( (lg == 1 ) && (fp_state < 3) && (fp_state > 0)&& (  (recvBuffer[12+i] == 0) || (recvBuffer[12+i] == 1 ) )  )
				pfp_c_data->Stand_Alone_Auth = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
					
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Max_Auth_Time://!!, (0-255),set only 0 can be writen,w(1-9). 0 means auth time is unlimited.only read.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Max_Auth_Time;
			/*
			if( (lg == 1 ) && (fp_state < 10 ) && (recvBuffer[12+i] == 0)  )
				pfp_c_data->Max_Auth_Time = recvBuffer[12+i];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Max_Time_W_O_Prog ://Ad= 21, (0-255),w(1-2),value only 0 can be writen, 0 means no check.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Max_Time_W_O_Prog;
			/*
			if( (lg == 1 ) && (fp_state < 3) && (recvBuffer[12+i] == 0) )
				pfp_c_data->Max_Time_W_O_Prog = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Min_Fuelling_Vol: //(0-255),w(1-2).
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Min_Fuelling_Vol;
			/*
			if( (lg == 1 ) && (fp_state < 3)   ) 
				pfp_c_data->Min_Fuelling_Vol = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Min_Display_Vol://(0-255),w(1-2). .
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Min_Display_Vol ;
			/*
			if( (lg == 1 ) && (fp_state < 3)  ) 
				pfp_c_data->Min_Display_Vol = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Min_Guard_Time: //(0-255),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Min_Guard_Time ;
			/*
			if( (lg == 1 ) && (fp_state < 3)  )
				pfp_c_data->Min_Guard_Time = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Pulser_Err_Tolerance://Ad = 26,(0-255),w(1-2),
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Pulser_Err_Tolerance ;
			/*
			if( (lg == 1 ) && (fp_state < 3) )
				pfp_c_data->Pulser_Err_Tolerance = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Time_Display_Product_Name://O,Ad = 28,(0-255),w(1-2),no support.
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Time_Display_Product_Name ;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Digits_Vol_Layout: //Ad= 40, BCD2,w(1-2),
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Digits_Vol_Layout ;
			/*
			if( (lg == 1 ) && (fp_state < 3) && (  (recvBuffer[12+i] & 0x0f) >= 0 ) && (  (recvBuffer[12+i]& 0x0f)<= 9  )  \
				&& ( (recvBuffer[12+i]>>4 & 0x0f)>= 0 ) && (  (recvBuffer[12+i]>>4& 0x0f) <= 9  )    ) 
				pfp_c_data->Digits_Vol_Layout = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Digits_Amount_Layout://BCD2,w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Digits_Amount_Layout ;
			/*
			if( (lg == 1 ) && (fp_state < 3)  && (  (recvBuffer[12+i] & 0x0f) >= 0 ) && (  (recvBuffer[12+i]& 0x0f)<= 9  )  \
				&& ( (recvBuffer[12+i]>>4 & 0x0f)>= 0 ) && (  (recvBuffer[12+i]>>4& 0x0f) <= 9  )    ) 
				pfp_c_data->Digits_Amount_Layout = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Digits_Unit_Price://BCD2,w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Digits_Unit_Price ;
			/*
			if( (lg == 1 ) && (fp_state < 3)  && ( (recvBuffer[12+i] & 0x0f) >= 0  ) && (  (recvBuffer[12+i]& 0x0f)<= 9  )  \
				&& ( (recvBuffer[12+i]>>4 & 0x0f)>= 0 ) && (  (recvBuffer[12+i]>>4& 0x0f) <= 9  )    ) 
				pfp_c_data->Digits_Unit_Price = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Unit_Price_Mult_Fact://(bit8:0-1,bit1-4:0-9),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Unit_Price_Mult_Fact ;
			/*
			if( (lg == 1 ) && (fp_state < 3)  && ( ( recvBuffer[12+i] & 0x0f) >= 0) && ( (recvBuffer[12+i] & 0x0f)<= 9) )
				pfp_c_data->Unit_Price_Mult_Fact = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Amount_Rounding_Type:  //bcd4,w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Amount_Rounding_Type;
			/*
			if( (lg == 2 ) && (fp_state < 3) )
				for(j=0; j<2; j++)  pfp_c_data->Amount_Rounding_Type[j] = recvBuffer[12+i+j]; 
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Preset_Rounding_Amount: //bcd2,w(1-9)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Preset_Rounding_Amount ;
			/*
			if( (lg == 1 ) && (fp_state < 3)   && (  (recvBuffer[12+i] & 0x0f) >= 0 ) && (  (recvBuffer[12+i] & 0x0f) >= 0 ) && (  (recvBuffer[13+i]& 0x0f)<= 9  )  \
				&& ( (recvBuffer[12+i]>>4 & 0x0f)>= 0 ) && (  (recvBuffer[12+i]>>4& 0x0f) <= 9  )    ) 
				pfp_c_data->Preset_Rounding_Amount = recvBuffer[12+i];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			*/
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
				
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Price_Set_Nb: //O
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Price_Set_Nb ;
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Manufacturer_Id:// Ad= 50,R(1-9).
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Manufacturer_Id ;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Dispenser_Model://R(1-9).
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Dispenser_Model ;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Calculator_Type://R(1-9).
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Calculator_Type  ;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Calculator_Serial_No://R(1-9).
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Calculator_Serial_No  ;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;		
			break;
		case C_Appl_Software_Ver: //R(1-9).
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Appl_Software_Ver   ;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;			
			break;
		case C_W_M_Software_Ver: //R(1-9).
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_W_M_Software_Ver    ;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_W_M_Software_Date: //R(1-9).
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_W_M_Software_Date    ;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_W_M_Security_Type://R(1-9).
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_W_M_Security_Type ;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Protocol_Ver://R(1-9).
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_Protocol_Ver;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_SW_Change_Date :	//R(1-9),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_SW_Change_Date ;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_SW_Change_Personal_Nb : //R(1-9),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_SW_Change_Personal_Nb;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_SW_Checksum : //R(1-9),w(1-2)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char) C_SW_Checksum;
			tmpDataAck = '\x02';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Calc_Illumination: // Ad= 70,(0-1),w(1-9)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Calc_Illumination;
			if( (lg == 1 ) && (fp_state < 10)  && ( (recvBuffer[12+i] == 0) || (recvBuffer[12+i] ==1 ) )   )
				pfp_c_data->Calc_Illumination = recvBuffer[13+i];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_LCD_Backlight_Switch: //O
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_LCD_Backlight_Switch;
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_Display_Intensity://O
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_Display_Intensity;
			tmpDataAck = '\x04';  
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_W_M_Polynomial:// Ad=80,W(1-9)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_W_M_Polynomial;
			if( (lg == 2 ) && (fp_state < 10) && (fp_state > 0) )
				for(j=0; j<2; j++)  pfp_c_data->W_M_Polynomial[j] = recvBuffer[12+i+j]; //lg=2.there use 2 for change.
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		case C_W_M_Seed: //w(1-9)
			tmp_lg++;
			buffer[tmp_lg] = (unsigned char)C_W_M_Seed;
			if( (lg == 2 ) && (fp_state < 10) && (fp_state > 0)  )
				for(j=0; j<2; j++)  pfp_c_data->W_M_Seed[j] = recvBuffer[12+i+j]; //lg=2.there use 2 for change.
			else  if (fp_state >=10)  { 
				tmpDataAck = '\x02';  
				tmpMSAck = '\x05';
				}
			else  { 
				tmpDataAck = '\x01';  
				tmpMSAck = '\x05';
				}
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		default:
			tmp_lg++;
			buffer[tmp_lg] =recvBuffer[10+ i]  ;
			tmpDataAck = '\x04';  //not exist.
			tmpMSAck = '\x05';
			tmp_lg++;
			buffer[tmp_lg] =tmpDataAck;
			i = i + 2 + lg;
			break;
		}
	}
	
	if( (i + 2) != recv_lg )	{
		UserLog("(i + 2) != recv_lg, write C_Dat error!! exit. i[%d],recv_lg[%d]", i, recv_lg);
		//exit(0);
		return -2;
	}
	tmp_lg++;
	buffer[6] = (unsigned char )((tmp_lg-8) / 256) ; //M_Lg
	buffer[7] = (unsigned char )((tmp_lg-8) %256);  //big end.
	buffer[10] = tmpMSAck;  
	if(NULL != msg_lg) *msg_lg= tmp_lg;
	if(NULL != sendBuffer)  memcpy(sendBuffer, buffer, tmp_lg);
	return 0;
}


//只写FP_C_DATA的单个数据项函数,用于设备自己改写或者初始化.
//data(Data_Id+Data_Lg+Data_El), 成功返回0,失败返回-1或者Data_Ack.
int SetSglC_DATA(const char node, const unsigned char *data)
{
	int  j, lg; //i, 
	int  fp_state = 0 ;//recv_lg, tmp_lg = 0;
	unsigned char tmpDataAck; 
	DISPENSER_DATA *dispenser_data;
	FP_C_DATA *pfp_c_data;	
	UserLog("Set a data in C_DAT of %d",node);
	if( NULL == data ){		
		UserLog("参数错误,计算器Write  失败");
		return -1;
	}
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计算器Write  失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计算器Write 失败");
		return -1;
	}
	for(j=0;j < MAX_CHANNEL_CNT; j++){
		if ( node == gpIfsf_shm->convert_hb[j].lnao.node) {
		      	break; //找到了加油机节点
		}
	}
	if(j == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计算器Write 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[j]) );
	pfp_c_data =  (FP_C_DATA *)( &(dispenser_data->fp_c_data) );
	for(j=0; j < MAX_FP_CNT; j++){ 
		if( fp_state < (dispenser_data->fp_data[j].FP_State) )  fp_state = (dispenser_data->fp_data[j].FP_State) ;		
	}	
	tmpDataAck = '\0'; //ok ack.
	lg =  data[1]; //Data_Lg.
	switch( data[0] )
	{
		case C_Nb_Products: //(1-8),w(1-2)
			if( ( lg== 1 ) && (fp_state < 3) && (data[2] >= 1) && (data[2]<= 8) ) 
				pfp_c_data->Nb_Products = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';
				}
			else  { 
				tmpDataAck = '\x01';
				}			
			break;
		case C_Nb_Fuelling_Modes: //(1-8),w(1-2), but now only 1 mode.
			if( ( lg== 1 ) && (fp_state < 3) && (data[2] == 1) )
				pfp_c_data->Nb_Fuelling_Modes = data[2];
			else  if (fp_state >= 3)  {
				tmpDataAck = '\x02';
				}
			else  {
				tmpDataAck = '\x01'; 
				}			
			break;
		case C_Nb_Meters://(1-16),w(1-2). 
			if( ( lg== 1 )  && (fp_state < 3) && (data[2] >= 1) && (data[2] <= 16) )
				pfp_c_data->Nb_Meters = data[2];
			else  if (fp_state >= 3)  {
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Nb_FP:
			if( (lg == 1 ) && (fp_state < 3) && (data[2] >= 1) && (data[2] <= 4) )
				pfp_c_data->Nb_FP = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Country_Code://w(1-2)
			if( (lg == 2 ) && (fp_state < 3) )
				for(j=0; j<lg; j++)  pfp_c_data->Country_Code[j] = data[2+j]; //lg=2.there use 2 for change.
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Blend_Tolerance: //(0-99),w(1-2)
			if( (lg == 1 ) && (fp_state < 3)  && (  (data[2] & 0x0f) >= 0 ) && (  (data[2]& 0x0f)<= 9  )  \
				&& ( (data[2]>>4 & 0x0f )>= 0 ) && (  (data[2]>>4& 0x0f)  <= 9  )    ) 
				pfp_c_data->Blend_Tolerance = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Drive_Off_Lights_Mode:			
			tmpDataAck = '\x04'; 
			break;
		case C_OPT_Light_Mode:
			tmpDataAck = '\x04'; 
			break;
		case C_Clear_Display_Mode: //w(1-2)
			if( (lg == 1 ) && (fp_state < 3) )
				pfp_c_data->Clear_Display_Mode = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Auth_State_Mode: //(0-1),w(1-2)
			if( (lg == 1 ) && (fp_state < 3) &&  ( (data[2] == 0) || (data[2] == 1 ) )   )
				pfp_c_data->Auth_State_Mode = data[2]; //dispenser must be set first!!
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Stand_Alone_Auth: //(0-1),w(1-2)
			if( (lg == 1 ) && (fp_state < 3) && ( (data[2] == 0) || (data[2] == 1 ) )   )
				pfp_c_data->Stand_Alone_Auth = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Max_Auth_Time://!!, (0-255),only 0 can be writen,w(1-9). means auth time is unlimited.
			if( (lg == 1 ) && (fp_state < 10 ) && (data[2] == 0)  )
				pfp_c_data->Max_Auth_Time = data[2];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Max_Time_W_O_Prog ://Ad= 21, (0-255),w(1-2),value only 0 can be writen, means no check.
			if( (lg == 1 ) && (fp_state < 3) && (data[2] == 0) )
				pfp_c_data->Max_Time_W_O_Prog = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Min_Fuelling_Vol: //(0-255),w(1-2).
			if( (lg == 1 ) && (fp_state < 3)  )
				pfp_c_data->Min_Fuelling_Vol = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Min_Display_Vol://(0-255),w(1-2). .
			if( (lg == 1 ) && (fp_state < 3)   )
				pfp_c_data->Min_Display_Vol = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Min_Guard_Time: //(0-255),w(1-2)
			if( (lg == 1 ) && (fp_state < 3)   )
				pfp_c_data->Min_Guard_Time = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Pulser_Err_Tolerance://Ad = 26,(0-255),w(1-2),
			if( (lg == 1 ) && (fp_state < 3)  )
				pfp_c_data->Pulser_Err_Tolerance = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Time_Display_Product_Name://O,Ad = 28,(0-255),w(1-2),no support.
			tmpDataAck = '\x04';  
			break;
		case C_Digits_Vol_Layout: //Ad= 40, BCD2,w(1-2),
			if( (lg == 1 ) && (fp_state < 3) && (  (data[2] & 0x0f) >= 0 ) && (  (data[2]& 0x0f)<= 9  )  \
				&& ( (data[2]>>4 & 0x0f) >= 0 ) && (  (data[2]>>4& 0x0f ) <= 9  )    ) 
				pfp_c_data->Digits_Vol_Layout = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Digits_Amount_Layout://BCD2,w(1-2)
			if( (lg == 1 ) && (fp_state < 3)  && (  (data[2] & 0x0f) >= 0 ) && (  (data[2]& 0x0f)<= 9  )  \
				&& ( (data[2]>>4 & 0x0f) >= 0 ) && (  (data[2]>>4& 0x0f ) <= 9  )    ) 
				pfp_c_data->Digits_Amount_Layout = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Digits_Unit_Price://BCD2,w(1-2)
			if( (lg == 1 ) && (fp_state < 3) && (  (data[2] & 0x0f) >= 0 ) && (  (data[2]& 0x0f)<= 9  )  \
				&& ( (data[2]>>4 & 0x0f) >= 0 ) && (  (data[2]>>4& 0x0f ) <= 9  )    ) 
				pfp_c_data->Digits_Unit_Price = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Unit_Price_Mult_Fact://(bit8:0-1,bit1-4:0-9),w(1-2)
			if( (lg == 1 ) && (fp_state < 3)  && ( ( data[2] & 0x0f) >= 0) && ( (data[2] & 0x0f)<= 9) )
				pfp_c_data->Unit_Price_Mult_Fact = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Amount_Rounding_Type: //bcd4,w(1-2)
			if( (lg == 2 ) && (fp_state < 3) )
				for(j=0; j<lg; j++)  pfp_c_data->Amount_Rounding_Type[j] = data[2+j]; //lg=2.there use 2 for change.
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Preset_Rounding_Amount: //bcd2,w(1-9)
			if( (lg == 1 ) && (fp_state < 3)  && (  (data[2] & 0x0f) >= 0 ) && (  (data[2]& 0x0f)<= 9  )  \
				&& ( (data[2]>>4 & 0x0f) >= 0 ) && (  (data[2]>>4& 0x0f ) <= 9  )    ) 
				pfp_c_data->Preset_Rounding_Amount = data[2];
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Price_Set_Nb: //O
			tmpDataAck = '\x04';  
			break;
		case C_Manufacturer_Id:// Ad= 50,R(1-9).
			if( (lg == 3 ) && (fp_state < 3) )
				for(j=0; j<lg; j++)  pfp_c_data->Manufacturer_Id[j] = data[2+j]; //lg=3.there use lg for change.
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Dispenser_Model://R(1-9).
			if( (lg == 3 ) && (fp_state < 3) )
				for(j=0; j<lg; j++)  pfp_c_data->Dispenser_Model[j] = data[2+j]; //lg=3.there use lg for change.
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Calculator_Type://R(1-9).
			if( (lg == 3 ) && (fp_state < 3) )
				for(j=0; j<lg; j++) 
					pfp_c_data->Calculator_Type[j] = data[2+j]; //lg=3.there use 3 for change.
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Calculator_Serial_No://R(1-9).
			if( (lg == 12 ) && (fp_state < 3) )
				for(j=0; j<lg; j++) 
					pfp_c_data->Calculator_Serial_No[j] = data[2+j]; 
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Appl_Software_Ver: //R(1-9).
			if( (lg == 12 ) && (fp_state < 3) )
				for(j=0; j<lg; j++) 
					pfp_c_data->Appl_Software_Ver[j] = data[2+j]; 
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}	
			break;
		case C_W_M_Software_Ver: //R(1-9).
			if( (lg == 6 ) && (fp_state < 3) )
				for(j=0; j<lg; j++) 
					pfp_c_data->W_M_Software_Ver[j] = data[2+j]; 
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}	 
			break;
		case C_W_M_Software_Date: //R(1-9).
			if( (lg == 4 ) && (fp_state < 3) )
				for(j=0; j<lg; j++)  
					pfp_c_data->W_M_Software_Date[j] = data[2+j]; 
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}	
			break;
		case C_W_M_Security_Type://R(1-9).
			if( (lg == 1 ) && (fp_state < 3) )
				 pfp_c_data->W_M_Security_Type = data[2]; 
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				} 
			break;
		case C_Protocol_Ver://R(1-9).
			if( (lg == 6 ) && (fp_state < 3) )
				for(j=0; j<lg; j++)  
					pfp_c_data->Protocol_Ver[j] = data[2+j]; 
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}	
			break;
		case C_SW_Change_Date :	//R(1-9),w(1-2)
			if( (lg == 4 ) && (fp_state < 3) )
				for(j=0; j<lg; j++)  
					pfp_c_data->SW_Change_Date[j] = data[2+j]; 
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}	
			break;
		case C_SW_Change_Personal_Nb : //R(1-9),w(1-2)
			if( (lg == 7 ) && (fp_state < 3) )
				for(j=0; j<lg; j++)  
					pfp_c_data->SW_Change_Personal_Nb[j] = data[2+j]; 
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_SW_Checksum : //R(1-9),w(1-2)
			if( (lg == 4 ) && (fp_state < 3) )
				for(j=0; j<lg; j++)  
					pfp_c_data->SW_Checksum[j] = data[2+j]; 
			else  if (fp_state >= 3)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_Calc_Illumination: // Ad= 70,(0-1),w(1-9)
			if( (lg == 1 ) && (fp_state < 10)  &&  ( (data[2] == 0) || (data[2] ==1 )  )  )
				pfp_c_data->Calc_Illumination = data[2];
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_LCD_Backlight_Switch: //O
			tmpDataAck = '\x04'; 
			break;
		case C_Display_Intensity://O
			tmpDataAck = '\x04'; 
			break;
		case C_W_M_Polynomial:// Ad=80,W(1-9)
			if( (lg == 2 ) && (fp_state < 10) )
				for(j=0; j<lg; j++)  
					pfp_c_data->W_M_Polynomial[j] = data[2+j]; //lg=2.there use 2 for change.
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		case C_W_M_Seed: //w(1-9)
			if( (lg == 2 ) && (fp_state < 10) )
				for(j=0; j<2; j++)  
					pfp_c_data->W_M_Seed[j] = data[2+j]; //lg=2.there use 2 for change.
			else  if (fp_state >= 10)  { 
				tmpDataAck = '\x02';  
				}
			else  { 
				tmpDataAck = '\x01';  
				}
			break;
		default:
			tmpDataAck = '\x04'; 
			break;
		}
	 return  (int )(tmpDataAck);
	}
	

int RequestC_DATA(const char node){//Find CD in gpIfsf_shm.
	int i, tmp_lg, ret;  //fp_state, 
	//DISPENSER_DATA *dispenser_data;
	//FP_C_DATA *pfp_c_data;
	unsigned char buffer[512];
	memset(buffer, 0 , 512 );
	if( (node <= 0) || (node > 127 ) ){
		UserLog("加油机节点值错误,计算器初始化失败");
		return -1;
		}
	if( NULL == gpIfsf_shm ){		
		UserLog("共享内存值错误,计算器初始化失败");
		return -1;
	}
//	gpIfsf_shm->convertUsed[node-1] =(unsigned char) node;   // ?????? jie @ 03.06
	/*
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计算器初始化失败");
		return -1;
	}
	for(i=0; i < MAX_CHANNEL_CNT; i++){
		if( node == gpIfsf_shm->convert_hb[i].lnao.node)  break; //找到了加油机节点
	}
	if(i == MAX_CHANNEL_CNT ){
		UserLog("加油机节点数据找不到,计算器read 失败");
		return -1;
	}
	dispenser_data = (DISPENSER_DATA *)( &(gpIfsf_shm->dispenser_data[i]) );
	pfp_c_data =  (FP_C_DATA *)( &(dispenser_data->fp_c_data) );
	for(i=0; i < MAX_FP_CNT; i++){ 
		if( fp_state < (dispenser_data->fp_data[i].FP_State) )  fp_state = (dispenser_data->fp_data[i].FP_State) ;

		//if( fp_state == 0 ) return -1;
	}
	*/
	
	buffer[0] = '\x02';
	buffer[1] = '\x01';
	buffer[2] = '\x01';
	buffer[3] = (unsigned char)node;
	buffer[4] = '\0';  //IFSF_MC
	buffer[5] =  '\0'; //M_St; //read.
	
	buffer[8] = '\x01'; //recvBuffer[8];
	buffer[9] = '\x01'; //recvBuffer[9];
	//buffer[10] = recvBuffer[10]; //It's Data_Id. Data begin.
	tmp_lg=9;
	
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Nb_Products  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Nb_Fuelling_Modes  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Nb_Meters  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Nb_FP  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char) C_Country_Code ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Blend_Tolerance  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char) C_Clear_Display_Mode ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Auth_State_Mode  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char) C_Stand_Alone_Auth ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char) C_Max_Auth_Time ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Max_Time_W_O_Prog  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char) C_Min_Fuelling_Vol ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char) C_Min_Display_Vol ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char) C_Min_Display_Vol ;
	tmp_lg++;
	/*
	C_Pulser_Err_Tolerance = 26, C_Time_Display_Product_Name = 28, 
	C_Digits_Vol_Layout = 40,	C_Digits_Amount_Layout, C_Digits_Unit_Price, C_Unit_Price_Mult_Fact,
	C_Amount_Rounding_Type, C_Preset_Rounding_Amount, C_Price_Set_Nb,
	C_Manufacturer_Id = 50, C_Dispenser_Model, C_Calculator_Type,
	C_Calculator_Serial_No, C_Appl_Software_Ver, C_W_M_Software_Ver,
	C_W_M_Software_Date, C_W_M_Security_Type, C_Protocol_Ver,  C_SW_Change_Date ,
	C_SW_Change_Personal_Nb ,  C_SW_Checksum , C_Calc_Illumination = 70,
	C_LCD_Backlight_Switch , C_Display_Intensity , C_W_M_Polynomial =80, C_W_M_Seed, 
	*/
	buffer[tmp_lg] = (unsigned char) C_Pulser_Err_Tolerance ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Time_Display_Product_Name  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Digits_Vol_Layout  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char) C_Digits_Amount_Layout ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Digits_Unit_Price  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Unit_Price_Mult_Fact  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Amount_Rounding_Type  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_Preset_Rounding_Amount  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char) C_Calc_Illumination ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char)C_W_M_Polynomial  ;
	tmp_lg++;
	buffer[tmp_lg] = (unsigned char) C_W_M_Seed ;
	
	tmp_lg++;
	buffer[6] =(unsigned char)(tmp_lg /256) ;
	buffer[7] = (unsigned char)(tmp_lg % 256) ; 
	ret= SendData2CD(buffer, tmp_lg, 5);
	if (ret <0 ){
		UserLog("加油机通讯错误,计算器不能Request ");
		return -1;
	}
	return 0;
	
	
}
/*--------------------------------------------------------------*/
