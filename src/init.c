/*
2007-7-13 change for ifsf by chenfm. 
2008-03-04 - Jiangbo Yin <yinjb@css-intelligent.com>
    增加init_tlg_shm()
*/

#include "init.h"

extern char isChild;

static void RemoveIPC()
{
	int i;
	UserLog( "清理IPC" );	
	RemoveShm( GUN_SHM_INDEX );	/*不判断返回码.有就销毁*/	
	//RemoveShm( IFSF_SHM_INDEX );
	RemoveMsq( GUN_MSG_TRAN );
	RemoveMsq( ORD_MSG_TRAN );	
	RemoveMsq( TOTAL_MSG_TRAN );
	RemoveMsq( TCP_MSG_TRAN );
	RemoveMsq( TCP_MSG_TRAN2 );
	RemoveMsq( TANK_MSG_ID );	
	RemoveSem( PORT_SEM_INDEX );
	RemoveSem( ERR_SEM_INDEX );
	RemoveSem( WAYNE_RCAP2L_SEM_INDEX);
	RemoveSem( TCC_ADJUST_SEM_INDEX);
	//#ifdef IFSF
	RemoveShm( IFSF_SHM_INDEX );	/*不判断返回码.有就销毁*/ 
	RemoveSem( IFSF_SEM_INDEX );
	RemoveSem( HEARTBEAT_SEM_INDEX );
	RemoveSem( HEARTBEAT_SEM_RECV_INDEX );
	RemoveSem( HEARTBEAT_SEM_CONVERT_INDEX );
	 //RemoveSem( IFSF_FP_SEM_INDEX );
	RemoveSem( IFSF_DP_SEM_INDEX );
	RemoveSem( IFSF_DP_TR_SEM_INDEX);	
	RemoveSem( CD_SOCK_INDEX);
	
	//#endif
	return;

}
void WhenExit()
{
	UserLog( "进程[%d]退出,其父进程为[%d], 当前主进程的父进程为[%d]",  \
		getpid() ,getppid(), gpGunShm->ppid);
	if( (isChild == 0) &&(gpGunShm->ppid == getppid()) )	  /*父进程负责ipc的删除*/
		RemoveIPC(); 

	return;
}



/*!
 * \brief   数字字符串转换转换为压缩BCD
 * \param   asc   待转换数字字符串
 * \param   len   待转换数字字符串长度
 * \param   bcd   转换结果
 * \param   align 对齐方式, 0 - 左对齐右补零, 1 - 右对齐左补零
 * \return  int   成功返回0，失败返回-1(Ascii字符有非数字字符)
 */
int Asc2Bcd(const unsigned char *asc, int len, unsigned char *bcd, int align)
{
	int pos = 0;
	int i = 0;

	if ((len % 2) == 1) {
		if (align == 1) {
			bcd[pos++] = (asc[0] & 0x0F);
			i = 1;
		} else {
			bcd[len / 2] = 0x00;
		}
	}

	for (; i < len; i += 2, pos++) {
		bcd[pos] = ((asc[i] << 4) | (asc[i + 1] & 0x0F));
		if (bcd[pos] > 0x99) {
			return -1;
		}
	}

	return 0;
}


int JsonReadOilCfg(const json_t *fm_r)
{
	int rc,idx_r;
	json_t *val_r = NULL;
	int fd;
	OIL_CONFIG buff;
	char soilno[9];
	const char *oil_price_r = "1.00";
	const char *oil_name_r;
	int oil_no_r;
	char stest[7];	

	json_array_foreach(fm_r, idx_r, val_r) {
		rc = json_unpack(val_r, "{s:i,s:s}", "prod_nb", &oil_no_r,\
					"prod_name", &oil_name_r); //, "oil_price", &oil_price_r);
		
		if (rc != 0) {
			RunLog("parsing oil_map, unpack failed");
			return -1;
		}
	  

		RunLog("oil_map[%d]:{oil_no=%d,oil_name=%s,oil_price=%s}",
					idx_r,oil_no_r,oil_name_r,oil_price_r);

		memset((char *)&buff,0,sizeof(OIL_CONFIG));   
		//处理每条油品配置
		//油品号
		sprintf(soilno,"%08d",oil_no_r);  	
		buff.oilno[0] =  soilno[0]<<4|(soilno[1]&0x0f);	 
		buff.oilno[1] =  soilno[2]<<4|(soilno[3]&0x0f);		 	
		buff.oilno[2] =	 soilno[4]<<4|(soilno[5]&0x0f);	 	 	
		buff.oilno[3] =	 soilno[6]<<4|(soilno[7]&0x0f);	
         
		//油品名称
		Asc2Bcd(oil_name_r, strlen(oil_name_r), buff.oilname, 0);
		//memcpy(&buff.oilname,oil_name_r,sizeof(buff.oilname));
		//油品价格
		bzero(stest,sizeof(stest));
		sprintf(stest,"%06d",(int)((float)(atof(oil_price_r) * 100)));
			
		buff.price[0]  = stest[0]<<4|(stest[1]&0x0f); 
		buff.price[1]  = stest[2]<<4|(stest[3]&0x0f); 
		buff.price[2]  = stest[4]<<4|(stest[5]&0x0f); 

		memcpy(&(gpIfsf_shm->oil_config[idx_r]), (char *)&buff,  sizeof(OIL_CONFIG));
			
	}
	return idx_r;
}

int JsonReadDspCfg(const json_t *fm_r)
{
	int rc,idx_r;
	json_t *val_r = NULL;
	int fd, ret;
	unsigned char buff[16];
	char soilno[9];
	int sort_gun_r,node_r,chn_log_r,node_driver_r,fp_r,log_fp_r,phy_gun_r,logical_gun_r,oil_no_r,terminal_id_r,is_used_r;

	json_array_foreach(fm_r, idx_r, val_r) {
		rc = json_unpack(val_r, "{s:i,s:i, s:i,s:i,s:i,s:i,s:i,s:i, s:i,s:i,s:i}", "global_noz", &sort_gun_r,
					"node", &node_r, "chn_log", &chn_log_r,"node_driver",&node_driver_r,"phy_fp",&fp_r,"fp",&log_fp_r,
					"phy_gun",&phy_gun_r,"lnz",&logical_gun_r,"prod_nb",&oil_no_r,"terminal_id",&terminal_id_r,"is_used",&is_used_r);
		
		if (rc != 0) {
			RunLog("parsing dsp_dev_map, unpack failed");
			return -1;
		}

		RunLog("dsp_dev_map[%d]:{sort_gun=%d,node=%d,chn_log=%d,node_driver=%d,fp=%d,log_fp=%d,\
			                                             phy_gun=%d,logical_gun=%d,oil_no=%d,terminal_id=%d,is_used=%d}",
					idx_r,sort_gun_r,node_r,chn_log_r,node_driver_r,fp_r,log_fp_r,phy_gun_r,logical_gun_r,oil_no_r,terminal_id_r,is_used_r);

		memset(buff,0,sizeof(buff));  
		//处理每条油枪配置
		//buff[0] = sort_gun_r;		 					/*大排序枪号*/
		buff[0] = node_r;		 	 					/*节点号*/
		buff[1] = chn_log_r;							/*背板连接号*/		
		buff[2] = node_driver_r;	 					/*油机品牌号*/		
		buff[3] = fp_r;		        					/*机头号*/		
		buff[4] = log_fp_r;                					/*节点面板号*/	
		buff[5] = phy_gun_r;		 					/*节点枪号*/ 
		buff[6] = logical_gun_r;							 /*面板枪号*/
		
		sprintf(soilno,"%08d",oil_no_r);  					/*油品号*/
		buff[7] =  soilno[0]<<4|(soilno[1]&0x0f);	 
		buff[8] =  soilno[2]<<4|(soilno[3]&0x0f);		 	
		buff[9] =soilno[4]<<4|(soilno[5]&0x0f);	 	 	
		buff[10] =soilno[6]<<4|(soilno[7]&0x0f);	 	
		//buff[12] = terminal_id_r;       					 /*控制终端号*/ 
		buff[11] = is_used_r;      						 /*启用标志*/         

		memcpy(&(gpIfsf_shm->gun_config[idx_r]), buff,  sizeof(GUN_CONFIG));
	}
	return idx_r;
}


int JsonReadGeneralCfg(const json_t *fm_r)
{
	int rc,idx_r;
	json_t *val_r = NULL;
	int fd, ret;
	unsigned char buff[16];
	char soilno[9];
	int tank_heartbeat;

	
	rc = json_unpack(fm_r, "{s:i}", "tank_heartbeat", &tank_heartbeat);
	if (rc != 0) {
		RunLog("parsing cd.cfg General tank_heartbeat, unpack failed");
		return -1;
	}
	RunLog("************当前液位仪心跳模式************");
	RunLog("0.关闭液位仪心跳  1.开启液位仪心跳");
	RunLog("tank_heartbeat = %d",tank_heartbeat);
	RunLog("");
	RunLog("****************************************************");

	gpIfsf_shm->tank_heartbeat = tank_heartbeat;

	return 0;
}

int ReadJsonCfg(const char *cfgfn)
{
	json_t *root;
	json_t *child;
	int ret;
	
	root = json_load_file(cfgfn, 0, NULL);
	if (root == NULL)
		return -1;

	child = json_object_get(root, "product");
	if (child != NULL) {
		gpIfsf_shm->oilCnt = JsonReadOilCfg(child);
	}
	else goto READJSONFAILED;
	
	child = json_object_get(root, "dispenser");
	if (child != NULL) {
		gpIfsf_shm->gunCnt = JsonReadDspCfg(child);
	}
	else goto READJSONFAILED;

	child = json_object_get(root, "general");
	
	if (child != NULL) {
		if (JsonReadGeneralCfg(child) < 0) //默认为开启液位仪心跳发送
			gpIfsf_shm->tank_heartbeat = 1;
			
	}
	else goto READJSONFAILED;

	json_decref(root);
	return 0;

READJSONFAILED:
	json_decref(root);
	return -1;
}

/*
* 	breif : 当存在cd.cfg和cd_main时读取内存方式为1, 否则为0
*
*
*/

int inline GetCfgMode()
{
	return (IsFileExist("/home/App/ifsf/etc/cd.cfg") && IsFileExist("/home/App/ifsf/bin/cd_main"));
}

int init_tlg_shm(void)
{
	int node;
	int ret;

	node = atoi(TLG_NODE);
	if (node == 0) {
		RunLog("没有配置液位仪节点");
		return 1;
	}

	gpIfsf_shm->tlg_data.tlg_node = node;
	gpIfsf_shm->tlg_data.tlg_drv = atoi(TLG_DRV);
	gpIfsf_shm->tlg_data.tlg_serial_port = atoi(TLG_SERIAL_PORT);
	gpIfsf_shm->tlg_data.tlg_dat.Nb_Tanks = atoi(TLG_PROBER_NUM);
	ret = AddTLGSendHB(node);
	if (ret < 0) {
		return -1;
	}
	ret = DefaultTLG(node);
	if (ret < 0) {
		return -1;
	}
	return 0;
}



/*系统初始化*/
int InitAll( int n )
{
	int fd;								/*文件描述符*/
	int ret;
	int size;
	int dpCnt;
	int i, j, m, k;
	int startPort = 3;
	int ifsf_size ;
	int NodeRecCount = 0;           //NodeChannel的记录条数
	int PreVrPanelNo;
	int PreOpwGunNo;
		
	unsigned char chn;
	unsigned char port;
	unsigned char portChnl[MAX_PORT_CNT];
	unsigned char buff[sizeof(GUN_CONFIG)+1];	
	unsigned char oilbuff[sizeof(OIL_CONFIG)+1];  //modify by liys 
	unsigned char iNozzleCount;                   //modify by liys
	unsigned char NodeGunCount[MAX_CHANNEL_CNT];
	unsigned char NodeOil[MAX_CHANNEL_CNT][FP_PR_CNT][4]; //节点上的油品
	const unsigned char zero_array[4] = {0};

	DISPENSER_DATA *dispenser_data;
	NODE_CHANNEL *pNode_channel;
	GUN_CONFIG *pGun_Config;    //modify by liys
	OIL_CONFIG *poilConfig;
	
	extern IFSF_SHM * gpIfsf_shm; //该结构是全局的量，只有一个,在ifsf_main.c中定义.

	if (!GetCfgMode()) {
		poilConfig = (OIL_CONFIG *)(&oilbuff[0]);
		
		if( n <= 0 ||  n % sizeof(GUN_CONFIG) != 0 )	//文件不存在或者为空 modify by liys
		{
			UserLog( "配置文件有误. FileSize=[%d]", n );
			return -1;
		}


		dpCnt = n / sizeof(GUN_CONFIG); /*取配置文件的记录条数*/
		
		if( dpCnt > MAX_GUN_CNT ) {
			UserLog( "总加油枪数过多,请更改pub.h 中的MAX_GUN_CNT 参数并重新编译程序" );
			return -1;
		}
	}
	
	size = sizeof(GunShm);
	ifsf_size = sizeof(IFSF_SHM);
	
	/*创建共享内存*/	
	ret = CreateShm( GUN_SHM_INDEX, size );	
	if(ret < 0) {
		UserLog( "创建共享内存GunShm 失败.请检查系统." );
		return -1;
	}
	ret = CreateShm( IFSF_SHM_INDEX, ifsf_size );
	if( ret < 0 ) {
		UserLog( "创建共享内存IFSF_SHM 失败.请检查系统." );
		return -1;
	}
		
	gpGunShm = (GunShm*)AttachShm( GUN_SHM_INDEX );
	gpIfsf_shm = (IFSF_SHM* )AttachShm( IFSF_SHM_INDEX );		
	
	if ((gpIfsf_shm == NULL) && (gpGunShm == NULL)) {
		UserLog( "连接共享内存失败.请检查系统" );
		return -1;
	}
	
	InitIfsfShm();
	
	gpGunShm->sockCDPos = -1; //add in 2007.9.10
	gpGunShm->chnCnt= dpCnt;	
	atexit( WhenExit );


	if( 1 == atoi(SHOW_FCC_SEND_DATA)){
		RunLog("SHOW_FCC_SEND_DATA = [%d] 打印FCC发送的油机数据", atoi(SHOW_FCC_SEND_DATA));
	}else{
		RunLog("SHOW_FCC_SEND_DATA = [%d] 不打印FCC发送的油机数据", atoi(SHOW_FCC_SEND_DATA));
	}

	ret= GetPortChannel(portChnl);
	if(ret < 0 ) {
		UserLog( "GetPortChannel 失败.请检查" );
		return -1;
	}
	if (!GetCfgMode()){
	        // @@@@@@@@读oil.cfg 到OIL_CONFIG  modify by liys @@@@@@@@@@@@@@@//

		/*读配置文件oil.cfg信息 */
		/*文件中记录按节点号排序*/
		/*0 油品号              */
		/*1 油品名称            */
		/*2 油品价格            */	
	      
		do {
			m = GetOILCfgNameSize();	//modify by liys 
				
			if (m == -1) {
				UserLog("获取 oil.cfg 大小信息失败.");
				sleep(2);
			} else if (m == 0 || m == -2) {   /*0文件为空 -2文件不存在*/
				/*油枪配置文件为空.从后台取数据.此时别的交易概不运行*/
				UserLog("oil.cfg 文件为空.需配置油品信息");
				sleep(2);
			} else {
				break;
			}
		} while (m <=0 );

		fd = OpenCfg();
		if( fd < 0 ) {
			UserLog( "打开配置文件[oil.cfg]失败.请检查是否已配置油品信息");
			return -1;
		}

		memset(oilbuff, 0, sizeof(oilbuff));
		i = 0;
		gpIfsf_shm->oilCnt = 0;

		while ( 1 ) {
			n = read(fd, oilbuff, sizeof(OIL_CONFIG));
			if( n == 0 ) {		/*文件结束*/
				break;
			} else if(n < 0) {
				UserLog("读取 oil.cfg 配置文件出错");
				close(fd);
				return -1;
			}

		//	RunLog("第[%d]次读取到oil.cfg 数据[%d]个字符", i + 1, n);
			if( n != sizeof(OIL_CONFIG) ) {
				UserLog( "gun.cfg 配置信息有误" );
				close( fd );
				return -1;
			}
			
			poilConfig = (OIL_CONFIG *)(&oilbuff[0]);
			memcpy(&(gpIfsf_shm->oil_config[i]), poilConfig,  sizeof(OIL_CONFIG));
#if DEBUG
			UserLog("oil_config[%d]:%s", i, Asc2Hex((__u8 *)&gpIfsf_shm->oil_config[i], sizeof(OIL_CONFIG)));
#endif
			UserLog("oilno[%02x%02x%02x%02x], oilname[%s], price[%s]", 
				gpIfsf_shm->oil_config[i].oilno[0], gpIfsf_shm->oil_config[i].oilno[1],
				gpIfsf_shm->oil_config[i].oilno[2], gpIfsf_shm->oil_config[i].oilno[3],
			       	gpIfsf_shm->oil_config[i].oilname,
				Asc2Hex(gpIfsf_shm->oil_config[i].price, 3));
			i++;
			gpIfsf_shm->oilCnt++;
		}


		//@@@@@@@@@@@@读dsp.cfg 到GUN_CONFIG  modify by liys @@@@@@@@@@@@@@@@@@@@@@@@@//
	 
		/* 读配置文件dsp.cfg信息 */
		/* 文件中记录按节点号排序*/
		/* 0 节点号              */
		/* 1 逻辑通道号	         */
		/* 2 驱动编号            */
		/* 3 面板号              */
		/* 4 逻辑油枪号          */
		/* 5 物理油枪号          */
		/* 6 油品号              */
		/* 7 是否可用            */
		/* 8 逻辑面板号          */
		
		do {
			size = GetCfgNameSize();
			if (size == -1) {
				UserLog("获取 dsp.cfg 大小信息失败.");
				sleep(2);
			} else if (size == 0 || size == -2) {
				UserLog("dsp.cfg 为空. 请配置油枪信息");
				sleep(2);
			} else {
				break;
			}
		} while (size <= 0);

		fd = OpenCfg();
		if( fd < 0 ) {
			UserLog( "打开配置文件[dsp.cfg]失败.请检查是否已配置油枪" );
			return -1;
		}
	      
		memset( buff, 0, sizeof(buff) );
		i = 0;
		while (1) {
			n = read( fd, buff, sizeof(GUN_CONFIG) );
			if( n == 0 ) {	
				break;
			} else if( n < 0 ) {
				UserLog( "读取 dsp.cfg 配置文件出错" );
				close(fd);
				return -1;
			}

			//RunLog("第[%d]次读取到dsp.cfg 数据[%d]个字符\n",i+1, n);

			if( n != sizeof(GUN_CONFIG) ) {
				UserLog( "dsp.cfg 配置信息有误" );
				close( fd );
				return -1;
			}
			
			pGun_Config = (GUN_CONFIG*)(&buff[0]);
			memcpy(&(gpIfsf_shm->gun_config[i]), pGun_Config,  sizeof(GUN_CONFIG));
			RunLog("gun_config[%d]:%s", i,
				Asc2Hex((__u8 *)&gpIfsf_shm->gun_config[i], sizeof(GUN_CONFIG)));

			UserLog("node[%02x], chnlog[%02x], nodeDriver[%02x], phyfp[%02x], logfp[%02x],Log_Noz[%02x], Phy_Noz[%02x], oil_no[%s],isused[%02x]",\
			gpIfsf_shm->gun_config[i].node, \
			gpIfsf_shm->gun_config[i].chnLog, \
			gpIfsf_shm->gun_config[i].nodeDriver, \
			gpIfsf_shm->gun_config[i].phyfp, \
			gpIfsf_shm->gun_config[i].logfp, \
			gpIfsf_shm->gun_config[i].logical_gun_no, \
			gpIfsf_shm->gun_config[i].physical_gun_no, \
			Asc2Hex(gpIfsf_shm->gun_config[i].oil_no, 4), \
			gpIfsf_shm->gun_config[i].isused);

			i++;  // gun number ++
		}
		iNozzleCount = i;  // dsp.cfg 中油枪配置条数/gun_config 的记录数
		gpIfsf_shm->gunCnt = i;
	}
	else {
		ret = ReadJsonCfg("/home/App/ifsf/etc/cd.cfg");
		if (ret < 0) {
			RunLog("初始化JSON配置信息失败");
			return -1;
		}
		
		gpGunShm->chnCnt = iNozzleCount = gpIfsf_shm->gunCnt;
	}
	//@@@@@@@@@@@@@@  根据gun_config 对node_channel 赋值 @@@@@@@@@@@//
     
	/* 0 节点号                 */
	/* 1 逻辑通道号             */
	/* 2 驱动编号               */
	/* 3 加油机面板数           */
	/* 4 加油机油枪数           */
	/* 5 加油机油品数           */
	/* 6 加油机面板1-4油枪数    */
	/* 7 串口号                 */
	/* 8 物理通道号             */
	
	//下面用buff1存放从文件中读取NODE_CHANNEL 的记录.
	chn = 0;
	port = 0;
	i = 0;
	j = 0;
	m = 0;

	memset(NodeOil, 0, sizeof(NodeOil));
	//轮循遍历gun_config 结构体,节点号需从1开始顺序排,面板也是如此
	for (j = 0; j < iNozzleCount; j++) {             	
		if (gpIfsf_shm->gun_config[j].isused != 1) {
			// 若该条配置未启用, 跳过
//			RunLog("idx: %d, disabled , skip ......", j);
			continue;
		}

		gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].node = gpIfsf_shm->gun_config[j].node;
		gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].chnLog = gpIfsf_shm->gun_config[j].chnLog;
		gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].nodeDriver = gpIfsf_shm->gun_config[j].nodeDriver;

		//计算节点上的面板数
		if (gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].cntOfFps < gpIfsf_shm->gun_config[j].logfp) {
			gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].cntOfFps = gpIfsf_shm->gun_config[j].logfp;
		}

		//计算节点上的油枪数
		gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].cntOfNoz += 1;//相应节点的油枪数加1

		//计算节点上的油品数
		for (m = 0; m < 8; m++) {
			if (memcmp(&(NodeOil[gpIfsf_shm->gun_config[j].node - 1][m]),
					       	gpIfsf_shm->gun_config[j].oil_no, 4) == 0) {
				break;
			} else {
				if (memcmp(&(NodeOil[gpIfsf_shm->gun_config[j].node - 1][m]),
									 zero_array, 4) == 0) {
					memcpy(NodeOil[gpIfsf_shm->gun_config[j].node - 1][m],
								gpIfsf_shm->gun_config[j].oil_no, 4); 	

					gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].cntOfProd += 1;
					break;
				} 
				continue;
			}              
	      	}


		//计算面板上的油枪数
		gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].cntOfFpNoz[gpIfsf_shm->gun_config[j].logfp - 1] +=1;

		//计算串口号
		ret = GetSerialPort(gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].chnLog, portChnl);
		if(ret >0) {
			gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].serialPort = ret;
		} else {
			UserLog( "GetSerialPort 错误" );
			exit(-1);
		}

		//计算物理通道号 (0-7)
		ret = GetChnPhy(gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].chnLog, portChnl);
		if(ret >= 0) {
			gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].chnPhy = ret;
		} else {
			UserLog( "GetChnPhy 错误" );
			exit(-1);
		}

		//计算node_channel 结构体的记录条数

		if(NodeRecCount < gpIfsf_shm->gun_config[j].node) {
			NodeRecCount = gpIfsf_shm->gun_config[j].node;
		}
	}


	// 按原来的方式循环遍历node_channel
			 
	for (i = 0; i < NodeRecCount; i++) {
		pNode_channel = (NODE_CHANNEL*)(&gpIfsf_shm->node_channel[i]);
		if (pNode_channel->node == 0) {
			continue;
		}
					
		ret = AddDispenser(gpIfsf_shm->node_channel[i].node);
		dispenser_data = (DISPENSER_DATA *)(&(gpIfsf_shm->dispenser_data[i]));
		if(ret < 0){
			UserLog( "加油机[%d]初始化错误" ,gpIfsf_shm->node_channel[i].node);
			close( fd );
			return -1;			    
		}

		UserLog("node[%02x], chnlog[%02x], nodeDrive[%02x], cntOfFp[%02x], serial port[%02x], chnPhy[%02x]",  \
			gpIfsf_shm->node_channel[i].node, \
		       	gpIfsf_shm->node_channel[i].chnLog, \
		       	gpIfsf_shm->node_channel[i].nodeDriver, \
			gpIfsf_shm->node_channel[i].cntOfFps, \
		       	gpIfsf_shm->node_channel[i].serialPort, \
		       	gpIfsf_shm->node_channel[i].chnPhy );


		chn = pNode_channel->chnLog;       //buff[3];		/*通道号*/
		port = pNode_channel->serialPort;   //buff[4];		/*串口号*/

		if( chn > MAX_CHANNEL_CNT || port > MAX_PORT_CNT ) {
			UserLog( "配置文件配置信息有误" );
			close( fd );
			return -1;
		}


		gpGunShm->minInd[chn-1] = chn * INDTYP;
		gpGunShm->maxInd[chn-1] = chn* INDTYP + MAXADD;
		gpGunShm->useInd[chn-1] = gpGunShm->minInd[chn-1];
		// 因为采用消息队列通讯，为了不使因超时而放弃的消息堵塞队列，故在超时时将该type写入内存，
		// 用专门的程序回收.同时正常的type＋1
		
		gpGunShm->isCyc[chn-1] = 0;	/*未循环*/

		gpGunShm->chnLogUsed[chn-1] = 1;	  /*通道号等的配置都是从1开始.下标从0开始*/
		gpGunShm->portLogUsed[port-1] = 1;	  /*1表示通道或者串口在用*/
		gpGunShm->chnLogSet[chn-1] = 0;		  /*该物理通道属性还未设置*/


		//@@@@@@@@@@ 根据配置文件 oil.cfg 写ifsf 数据库modify by liys  2008-03-14 @@@@@@@@//

		dispenser_data = (DISPENSER_DATA *)(&(gpIfsf_shm->dispenser_data[i]));

		//< Test code ##01 added by jie @ 2009.08.21
		// 因RTX目前的换号实现机制需要FCC在Prod DB中配置所有可能使用的油品才能成功, 但限制是8种,
		// 这里将就RTX的临时处理方案, 几个月之后等RTX完整实现换号功能后撤销此处理.

		if (access("/home/App/ifsf/etc/huanhao.spec", F_OK) == 0) {
//			RunLog("## oilCnt: %d", gpIfsf_shm->oilCnt);
			if (gpIfsf_shm->oilCnt > 8) {
				RunLog("ERROR:Retalix 目前的实现机制不允许配置的油品数多于八种!\n");
				sleep(1);
				return -1;
			}

			for (n = 0; n < gpIfsf_shm->oilCnt; n++) {
				memcpy(dispenser_data->fp_pr_data[n].Prod_Nb, 
						gpIfsf_shm->oil_config[n].oilno, 4);
				memcpy(dispenser_data->fp_pr_data[n].Prod_Description, 
						gpIfsf_shm->oil_config[n].oilname, 16);
			}
			dispenser_data->fp_c_data.Nb_Products = gpIfsf_shm->oilCnt;
		}

		// End of test code 01>

		for(j = 0; j < MAX_GUN_CNT; j++) {
			pGun_Config = (GUN_CONFIG *)(&(gpIfsf_shm->gun_config[j]));
			if (pGun_Config->isused != 1) {
				// 若该配置没有启用, 则跳过
				continue;
			}

			if (gpIfsf_shm->node_channel[i].node == pGun_Config->node) {
				for(n = 0; n < FP_PR_CNT; n++) {
					if(memcmp(dispenser_data->fp_pr_data[n].Prod_Nb, \
								pGun_Config->oil_no,4) == 0) {
					      break;                           	
					}
				}

				//得到油品地址,如果找不到，则在product database 空位处插入一条，并记录地址		   
				if (n < FP_PR_CNT) {
					n = n + 1; 
				} else {
					for(n = 0; n < FP_PR_CNT; n++) {
						if ((dispenser_data->fp_pr_data[n].Prod_Nb[0] == 0 ) && \
							(dispenser_data->fp_pr_data[n].Prod_Nb[1] == 0 ) && \
							(dispenser_data->fp_pr_data[n].Prod_Nb[2] == 0 ) && \
							(dispenser_data->fp_pr_data[n].Prod_Nb[3] == 0 )) { // 为空
							memcpy(dispenser_data->fp_pr_data[n].Prod_Nb, \
										pGun_Config->oil_no, 4);
							int pr_idx;
							for (pr_idx = 0; pr_idx < gpIfsf_shm->oilCnt; ++pr_idx) {
								if (memcmp(dispenser_data->fp_pr_data[n].Prod_Nb,
									gpIfsf_shm->oil_config[pr_idx].oilno, 4) == 0) {
									memcpy(dispenser_data->fp_pr_data[n].Prod_Description, 
										gpIfsf_shm->oil_config[pr_idx].oilname, 16);
									break;
								}
							} // end of for pr_idx < gpIfsf_shm->oilCnt

							n = n + 1;
							break;
					       }
					}  // end of for n < FP_PR_CNT
				} // if else n >= FP_PR_CNT

			//根据面板号和逻辑枪号锁定logical nozzle database 中的数据，并修改pr_id 和physical_noz_id
			dispenser_data->fp_ln_data[pGun_Config->logfp - 1][pGun_Config->logical_gun_no - 1].PR_Id = n;
			dispenser_data->fp_ln_data[pGun_Config->logfp - 1][pGun_Config->logical_gun_no - 1].Physical_Noz_Id \
											= pGun_Config->physical_gun_no;
		                  
			}
		}

			//把油价初始化到ifsf 数据库中
		for(m = 0; m < MAX_OIL_CNT; m++) {
			poilConfig = (OIL_CONFIG *)(&(gpIfsf_shm->oil_config[m]));
			for(n = 0; n < FP_PR_CNT; n++) {
#if 0
				RunLog("init fp_fm_data: fp_pr_data[%d].Prod_Nb: %02x%02x%02x%02x, oilno: %s", n,
					dispenser_data->fp_pr_data[n].Prod_Nb[0],
					dispenser_data->fp_pr_data[n].Prod_Nb[1],
					dispenser_data->fp_pr_data[n].Prod_Nb[2],
					dispenser_data->fp_pr_data[n].Prod_Nb[3], Asc2Hex(poilConfig->oilno ,4));
#endif
				if (memcmp(dispenser_data->fp_pr_data[n].Prod_Nb, poilConfig->oilno ,4) == 0) {
					dispenser_data->fp_fm_data[n][0].Prod_Price[0] = 0x04;
					memcpy(&(dispenser_data->fp_fm_data[n][0].Prod_Price[1]), poilConfig->price, 3);
					break;
				}
			}
		}
	}



	//初始化videer-root大排序面板号 

	bzero(VrPanelNo,sizeof(VrPanelNo));
	PreVrPanelNo = 0;
	for (i = 0; i < NodeRecCount; i++) {
		for (j = 0; j < gpIfsf_shm->node_channel[i].cntOfFps; j++){			
			VrPanelNo[i][j] +=PreVrPanelNo + 1;
			PreVrPanelNo = VrPanelNo[i][j];			
		}
	}	

	// 初始化opw-root大排序枪号

	bzero(OpwGunNo,sizeof(OpwGunNo));
	PreOpwGunNo = 0;
	for (i = 0; i < NodeRecCount; i++ ){		
		for (j = 0; j < gpIfsf_shm->node_channel[i].cntOfNoz; j++){	
			for (k = 0;k < gpIfsf_shm->node_channel[i].cntOfFpNoz[j]; k++){
				OpwGunNo[i][j][k] += PreOpwGunNo + 1;
				PreOpwGunNo = OpwGunNo[i][j][k];			
			}
		}
	}
		
      	// fill tlg_data in shm
	if (init_tlg_shm() < 0) {
		RunLog("初始化液位仪数据到共享内存失败");
		return -1;
	}

	gpGunShm->sysStat = PAUSE_PROC;
	gpIfsf_shm->auth_flag = DEFAULT_AUTH_MODE;
  	ret = DefaultSV_DATA();
	if(ret < 0){
		UserLog( "FCC PCD 通讯数据库初始化错误" );		
		return -1;
	}
	gpGunShm->chnCnt = 0;
	gpGunShm->portCnt = 0;

	for( i = 0; i < MAX_CHANNEL_CNT; i++ ) {	/*该通道再用*/
		if( gpGunShm->chnLogUsed[i] == 1 ) {
			gpGunShm->chnCnt++;
		}
	}

	for( i = 0; i < MAX_PORT_CNT; i++ ) {
		if(gpGunShm->portLogUsed[i] == 1 ) {	/*该串口在用*/
			gpGunShm->portCnt++;
		}
	}

	gpGunShm->sysStat = 0;

	// Inital flag: gpIfsf_shm->is_new_kernel;
	is_new_version_kernel();


	// 初始化稳牌模块需要的标记位wayne_rcap2l_flag
	ret = InitWayneRcap2lFlag();
	if (ret < 0) {
		RunLog("初始化Wayneflag失败");
		return -1;
	}

#if 0
	// Do recovery here
	if (RecoveryTrAsOffline() < 0) {
		return -1;
	}
	if (RecoveryVolTotal() < 0) {
		return -1;
	}
	if (RecoveryTrSeqNb() < 0) {
		return -1;
	}
#endif

#if 0
	show_all_dispenser();
#endif

	//#ifdef IFSF
	DetachShm((char**)(&gpGunShm));
	DetachShm((char**)(&gpIfsf_shm));
	//#else 
	//DetachShm( (char**)(&gpGunShm));
	//#endif
	/*创建一个消息队列.用于监听串口的程序同通道监听程序通信*/
	ret = CreateMsq(GUN_MSG_TRAN);
	if( ret < 0 ) {
		UserLog( "创建消息队列失败。请检查系统." );
		return -1;
	}

	/*创建一个消息队列.用于监听后台的程序同通道监听程序通信*/
	ret = CreateMsq(ORD_MSG_TRAN);
	if( ret < 0 ) {
		UserLog( "创建消息队列失败。请检查系统." );
		return -1;
	}

	// TLG
	ret = CreateMsq(TANK_MSG_ID);
	if( ret < 0 ) {
		UserLog( "创建消息队列失败。请检查系统." );
		return -1;
	}

	//用于读取UDC泵码
	ret = CreateMsq(TOTAL_MSG_TRAN);
	if( ret < 0 ) {
		UserLog( "创建消息队列失败。请检查系统." );
		return -1;
	}

	//测试用于TCP 发送的消息队列
	ret = CreateMsq(TCP_MSG_TRAN);
	if( ret < 0 ) {
		UserLog( "创建消息TCP_MSG_TRAN 队列失败。请检查系统." );
		return -1;
	}
	//备用消息队列
	ret = CreateMsq(TCP_MSG_TRAN2);
	if( ret < 0 ) {
		UserLog( "创建消息TCP_MSG_TRAN2 队列失败。请检查系统." );
		return -1;
	}
	/*创建一个信号量.用于对串口写时加锁*/
	ret = CreateSem( PORT_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}

	/*创建一个信号量.用于记超时的消息队列键值时加锁*/
	ret = CreateSem( ERR_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}
	
	//#ifdef IFSF  创建一个信号量.
	ret = CreateSem( IFSF_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}
	ret = CreateSem( HEARTBEAT_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}
	ret = CreateSem( HEARTBEAT_SEM_RECV_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}
	ret = CreateSem( HEARTBEAT_SEM_CONVERT_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}
	ret = CreateSem( IFSF_DP_TR_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}	
				
	ret = CreateSem( IFSF_DP_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}
	ret = CreateSem( CD_SOCK_INDEX);
	if( ret < 0 ) {
		UserLog( "创建信号量失败" );
		return -1;
	}
	
	ret = CreateSem(WAYNE_RCAP2L_SEM_INDEX);
	if (ret < 0) {
		UserLog("创建信号量WAYNE_RCAP2L失败");
		return -1;
	}
	ret = CreateSem(TCC_ADJUST_SEM_INDEX);
	if (ret < 0) {
		UserLog("创建信号量TCC_ADJUST_SEM_INDEX失败");
		return -1;
	}
	//#endif

	return 0;

}






