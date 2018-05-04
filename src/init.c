/*
2007-7-13 change for ifsf by chenfm. 
2008-03-04 - Jiangbo Yin <yinjb@css-intelligent.com>
    ����init_tlg_shm()
*/

#include "init.h"

extern char isChild;

static void RemoveIPC()
{
	int i;
	UserLog( "����IPC" );	
	RemoveShm( GUN_SHM_INDEX );	/*���жϷ�����.�о�����*/	
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
	RemoveShm( IFSF_SHM_INDEX );	/*���жϷ�����.�о�����*/ 
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
	UserLog( "����[%d]�˳�,�丸����Ϊ[%d], ��ǰ�����̵ĸ�����Ϊ[%d]",  \
		getpid() ,getppid(), gpGunShm->ppid);
	if( (isChild == 0) &&(gpGunShm->ppid == getppid()) )	  /*�����̸���ipc��ɾ��*/
		RemoveIPC(); 

	return;
}



/*!
 * \brief   �����ַ���ת��ת��Ϊѹ��BCD
 * \param   asc   ��ת�������ַ���
 * \param   len   ��ת�������ַ�������
 * \param   bcd   ת�����
 * \param   align ���뷽ʽ, 0 - ������Ҳ���, 1 - �Ҷ�������
 * \return  int   �ɹ�����0��ʧ�ܷ���-1(Ascii�ַ��з������ַ�)
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
		//����ÿ����Ʒ����
		//��Ʒ��
		sprintf(soilno,"%08d",oil_no_r);  	
		buff.oilno[0] =  soilno[0]<<4|(soilno[1]&0x0f);	 
		buff.oilno[1] =  soilno[2]<<4|(soilno[3]&0x0f);		 	
		buff.oilno[2] =	 soilno[4]<<4|(soilno[5]&0x0f);	 	 	
		buff.oilno[3] =	 soilno[6]<<4|(soilno[7]&0x0f);	
         
		//��Ʒ����
		Asc2Bcd(oil_name_r, strlen(oil_name_r), buff.oilname, 0);
		//memcpy(&buff.oilname,oil_name_r,sizeof(buff.oilname));
		//��Ʒ�۸�
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
		//����ÿ����ǹ����
		//buff[0] = sort_gun_r;		 					/*������ǹ��*/
		buff[0] = node_r;		 	 					/*�ڵ��*/
		buff[1] = chn_log_r;							/*�������Ӻ�*/		
		buff[2] = node_driver_r;	 					/*�ͻ�Ʒ�ƺ�*/		
		buff[3] = fp_r;		        					/*��ͷ��*/		
		buff[4] = log_fp_r;                					/*�ڵ�����*/	
		buff[5] = phy_gun_r;		 					/*�ڵ�ǹ��*/ 
		buff[6] = logical_gun_r;							 /*���ǹ��*/
		
		sprintf(soilno,"%08d",oil_no_r);  					/*��Ʒ��*/
		buff[7] =  soilno[0]<<4|(soilno[1]&0x0f);	 
		buff[8] =  soilno[2]<<4|(soilno[3]&0x0f);		 	
		buff[9] =soilno[4]<<4|(soilno[5]&0x0f);	 	 	
		buff[10] =soilno[6]<<4|(soilno[7]&0x0f);	 	
		//buff[12] = terminal_id_r;       					 /*�����ն˺�*/ 
		buff[11] = is_used_r;      						 /*���ñ�־*/         

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
	RunLog("************��ǰҺλ������ģʽ************");
	RunLog("0.�ر�Һλ������  1.����Һλ������");
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
		if (JsonReadGeneralCfg(child) < 0) //Ĭ��Ϊ����Һλ����������
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
* 	breif : ������cd.cfg��cd_mainʱ��ȡ�ڴ淽ʽΪ1, ����Ϊ0
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
		RunLog("û������Һλ�ǽڵ�");
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



/*ϵͳ��ʼ��*/
int InitAll( int n )
{
	int fd;								/*�ļ�������*/
	int ret;
	int size;
	int dpCnt;
	int i, j, m, k;
	int startPort = 3;
	int ifsf_size ;
	int NodeRecCount = 0;           //NodeChannel�ļ�¼����
	int PreVrPanelNo;
	int PreOpwGunNo;
		
	unsigned char chn;
	unsigned char port;
	unsigned char portChnl[MAX_PORT_CNT];
	unsigned char buff[sizeof(GUN_CONFIG)+1];	
	unsigned char oilbuff[sizeof(OIL_CONFIG)+1];  //modify by liys 
	unsigned char iNozzleCount;                   //modify by liys
	unsigned char NodeGunCount[MAX_CHANNEL_CNT];
	unsigned char NodeOil[MAX_CHANNEL_CNT][FP_PR_CNT][4]; //�ڵ��ϵ���Ʒ
	const unsigned char zero_array[4] = {0};

	DISPENSER_DATA *dispenser_data;
	NODE_CHANNEL *pNode_channel;
	GUN_CONFIG *pGun_Config;    //modify by liys
	OIL_CONFIG *poilConfig;
	
	extern IFSF_SHM * gpIfsf_shm; //�ýṹ��ȫ�ֵ�����ֻ��һ��,��ifsf_main.c�ж���.

	if (!GetCfgMode()) {
		poilConfig = (OIL_CONFIG *)(&oilbuff[0]);
		
		if( n <= 0 ||  n % sizeof(GUN_CONFIG) != 0 )	//�ļ������ڻ���Ϊ�� modify by liys
		{
			UserLog( "�����ļ�����. FileSize=[%d]", n );
			return -1;
		}


		dpCnt = n / sizeof(GUN_CONFIG); /*ȡ�����ļ��ļ�¼����*/
		
		if( dpCnt > MAX_GUN_CNT ) {
			UserLog( "�ܼ���ǹ������,�����pub.h �е�MAX_GUN_CNT ���������±������" );
			return -1;
		}
	}
	
	size = sizeof(GunShm);
	ifsf_size = sizeof(IFSF_SHM);
	
	/*���������ڴ�*/	
	ret = CreateShm( GUN_SHM_INDEX, size );	
	if(ret < 0) {
		UserLog( "���������ڴ�GunShm ʧ��.����ϵͳ." );
		return -1;
	}
	ret = CreateShm( IFSF_SHM_INDEX, ifsf_size );
	if( ret < 0 ) {
		UserLog( "���������ڴ�IFSF_SHM ʧ��.����ϵͳ." );
		return -1;
	}
		
	gpGunShm = (GunShm*)AttachShm( GUN_SHM_INDEX );
	gpIfsf_shm = (IFSF_SHM* )AttachShm( IFSF_SHM_INDEX );		
	
	if ((gpIfsf_shm == NULL) && (gpGunShm == NULL)) {
		UserLog( "���ӹ����ڴ�ʧ��.����ϵͳ" );
		return -1;
	}
	
	InitIfsfShm();
	
	gpGunShm->sockCDPos = -1; //add in 2007.9.10
	gpGunShm->chnCnt= dpCnt;	
	atexit( WhenExit );


	if( 1 == atoi(SHOW_FCC_SEND_DATA)){
		RunLog("SHOW_FCC_SEND_DATA = [%d] ��ӡFCC���͵��ͻ�����", atoi(SHOW_FCC_SEND_DATA));
	}else{
		RunLog("SHOW_FCC_SEND_DATA = [%d] ����ӡFCC���͵��ͻ�����", atoi(SHOW_FCC_SEND_DATA));
	}

	ret= GetPortChannel(portChnl);
	if(ret < 0 ) {
		UserLog( "GetPortChannel ʧ��.����" );
		return -1;
	}
	if (!GetCfgMode()){
	        // @@@@@@@@��oil.cfg ��OIL_CONFIG  modify by liys @@@@@@@@@@@@@@@//

		/*�������ļ�oil.cfg��Ϣ */
		/*�ļ��м�¼���ڵ������*/
		/*0 ��Ʒ��              */
		/*1 ��Ʒ����            */
		/*2 ��Ʒ�۸�            */	
	      
		do {
			m = GetOILCfgNameSize();	//modify by liys 
				
			if (m == -1) {
				UserLog("��ȡ oil.cfg ��С��Ϣʧ��.");
				sleep(2);
			} else if (m == 0 || m == -2) {   /*0�ļ�Ϊ�� -2�ļ�������*/
				/*��ǹ�����ļ�Ϊ��.�Ӻ�̨ȡ����.��ʱ��Ľ��׸Ų�����*/
				UserLog("oil.cfg �ļ�Ϊ��.��������Ʒ��Ϣ");
				sleep(2);
			} else {
				break;
			}
		} while (m <=0 );

		fd = OpenCfg();
		if( fd < 0 ) {
			UserLog( "�������ļ�[oil.cfg]ʧ��.�����Ƿ���������Ʒ��Ϣ");
			return -1;
		}

		memset(oilbuff, 0, sizeof(oilbuff));
		i = 0;
		gpIfsf_shm->oilCnt = 0;

		while ( 1 ) {
			n = read(fd, oilbuff, sizeof(OIL_CONFIG));
			if( n == 0 ) {		/*�ļ�����*/
				break;
			} else if(n < 0) {
				UserLog("��ȡ oil.cfg �����ļ�����");
				close(fd);
				return -1;
			}

		//	RunLog("��[%d]�ζ�ȡ��oil.cfg ����[%d]���ַ�", i + 1, n);
			if( n != sizeof(OIL_CONFIG) ) {
				UserLog( "gun.cfg ������Ϣ����" );
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


		//@@@@@@@@@@@@��dsp.cfg ��GUN_CONFIG  modify by liys @@@@@@@@@@@@@@@@@@@@@@@@@//
	 
		/* �������ļ�dsp.cfg��Ϣ */
		/* �ļ��м�¼���ڵ������*/
		/* 0 �ڵ��              */
		/* 1 �߼�ͨ����	         */
		/* 2 �������            */
		/* 3 ����              */
		/* 4 �߼���ǹ��          */
		/* 5 ������ǹ��          */
		/* 6 ��Ʒ��              */
		/* 7 �Ƿ����            */
		/* 8 �߼�����          */
		
		do {
			size = GetCfgNameSize();
			if (size == -1) {
				UserLog("��ȡ dsp.cfg ��С��Ϣʧ��.");
				sleep(2);
			} else if (size == 0 || size == -2) {
				UserLog("dsp.cfg Ϊ��. ��������ǹ��Ϣ");
				sleep(2);
			} else {
				break;
			}
		} while (size <= 0);

		fd = OpenCfg();
		if( fd < 0 ) {
			UserLog( "�������ļ�[dsp.cfg]ʧ��.�����Ƿ���������ǹ" );
			return -1;
		}
	      
		memset( buff, 0, sizeof(buff) );
		i = 0;
		while (1) {
			n = read( fd, buff, sizeof(GUN_CONFIG) );
			if( n == 0 ) {	
				break;
			} else if( n < 0 ) {
				UserLog( "��ȡ dsp.cfg �����ļ�����" );
				close(fd);
				return -1;
			}

			//RunLog("��[%d]�ζ�ȡ��dsp.cfg ����[%d]���ַ�\n",i+1, n);

			if( n != sizeof(GUN_CONFIG) ) {
				UserLog( "dsp.cfg ������Ϣ����" );
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
		iNozzleCount = i;  // dsp.cfg ����ǹ��������/gun_config �ļ�¼��
		gpIfsf_shm->gunCnt = i;
	}
	else {
		ret = ReadJsonCfg("/home/App/ifsf/etc/cd.cfg");
		if (ret < 0) {
			RunLog("��ʼ��JSON������Ϣʧ��");
			return -1;
		}
		
		gpGunShm->chnCnt = iNozzleCount = gpIfsf_shm->gunCnt;
	}
	//@@@@@@@@@@@@@@  ����gun_config ��node_channel ��ֵ @@@@@@@@@@@//
     
	/* 0 �ڵ��                 */
	/* 1 �߼�ͨ����             */
	/* 2 �������               */
	/* 3 ���ͻ������           */
	/* 4 ���ͻ���ǹ��           */
	/* 5 ���ͻ���Ʒ��           */
	/* 6 ���ͻ����1-4��ǹ��    */
	/* 7 ���ں�                 */
	/* 8 ����ͨ����             */
	
	//������buff1��Ŵ��ļ��ж�ȡNODE_CHANNEL �ļ�¼.
	chn = 0;
	port = 0;
	i = 0;
	j = 0;
	m = 0;

	memset(NodeOil, 0, sizeof(NodeOil));
	//��ѭ����gun_config �ṹ��,�ڵ�����1��ʼ˳����,���Ҳ�����
	for (j = 0; j < iNozzleCount; j++) {             	
		if (gpIfsf_shm->gun_config[j].isused != 1) {
			// ����������δ����, ����
//			RunLog("idx: %d, disabled , skip ......", j);
			continue;
		}

		gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].node = gpIfsf_shm->gun_config[j].node;
		gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].chnLog = gpIfsf_shm->gun_config[j].chnLog;
		gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].nodeDriver = gpIfsf_shm->gun_config[j].nodeDriver;

		//����ڵ��ϵ������
		if (gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].cntOfFps < gpIfsf_shm->gun_config[j].logfp) {
			gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].cntOfFps = gpIfsf_shm->gun_config[j].logfp;
		}

		//����ڵ��ϵ���ǹ��
		gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].cntOfNoz += 1;//��Ӧ�ڵ����ǹ����1

		//����ڵ��ϵ���Ʒ��
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


		//��������ϵ���ǹ��
		gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].cntOfFpNoz[gpIfsf_shm->gun_config[j].logfp - 1] +=1;

		//���㴮�ں�
		ret = GetSerialPort(gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].chnLog, portChnl);
		if(ret >0) {
			gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].serialPort = ret;
		} else {
			UserLog( "GetSerialPort ����" );
			exit(-1);
		}

		//��������ͨ���� (0-7)
		ret = GetChnPhy(gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].chnLog, portChnl);
		if(ret >= 0) {
			gpIfsf_shm->node_channel[gpIfsf_shm->gun_config[j].node -1].chnPhy = ret;
		} else {
			UserLog( "GetChnPhy ����" );
			exit(-1);
		}

		//����node_channel �ṹ��ļ�¼����

		if(NodeRecCount < gpIfsf_shm->gun_config[j].node) {
			NodeRecCount = gpIfsf_shm->gun_config[j].node;
		}
	}


	// ��ԭ���ķ�ʽѭ������node_channel
			 
	for (i = 0; i < NodeRecCount; i++) {
		pNode_channel = (NODE_CHANNEL*)(&gpIfsf_shm->node_channel[i]);
		if (pNode_channel->node == 0) {
			continue;
		}
					
		ret = AddDispenser(gpIfsf_shm->node_channel[i].node);
		dispenser_data = (DISPENSER_DATA *)(&(gpIfsf_shm->dispenser_data[i]));
		if(ret < 0){
			UserLog( "���ͻ�[%d]��ʼ������" ,gpIfsf_shm->node_channel[i].node);
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


		chn = pNode_channel->chnLog;       //buff[3];		/*ͨ����*/
		port = pNode_channel->serialPort;   //buff[4];		/*���ں�*/

		if( chn > MAX_CHANNEL_CNT || port > MAX_PORT_CNT ) {
			UserLog( "�����ļ�������Ϣ����" );
			close( fd );
			return -1;
		}


		gpGunShm->minInd[chn-1] = chn * INDTYP;
		gpGunShm->maxInd[chn-1] = chn* INDTYP + MAXADD;
		gpGunShm->useInd[chn-1] = gpGunShm->minInd[chn-1];
		// ��Ϊ������Ϣ����ͨѶ��Ϊ�˲�ʹ��ʱ����������Ϣ�������У����ڳ�ʱʱ����typeд���ڴ棬
		// ��ר�ŵĳ������.ͬʱ������type��1
		
		gpGunShm->isCyc[chn-1] = 0;	/*δѭ��*/

		gpGunShm->chnLogUsed[chn-1] = 1;	  /*ͨ���ŵȵ����ö��Ǵ�1��ʼ.�±��0��ʼ*/
		gpGunShm->portLogUsed[port-1] = 1;	  /*1��ʾͨ�����ߴ�������*/
		gpGunShm->chnLogSet[chn-1] = 0;		  /*������ͨ�����Ի�δ����*/


		//@@@@@@@@@@ ���������ļ� oil.cfg дifsf ���ݿ�modify by liys  2008-03-14 @@@@@@@@//

		dispenser_data = (DISPENSER_DATA *)(&(gpIfsf_shm->dispenser_data[i]));

		//< Test code ##01 added by jie @ 2009.08.21
		// ��RTXĿǰ�Ļ���ʵ�ֻ�����ҪFCC��Prod DB���������п���ʹ�õ���Ʒ���ܳɹ�, ��������8��,
		// ���ｫ��RTX����ʱ������, ������֮���RTX����ʵ�ֻ��Ź��ܺ����˴���.

		if (access("/home/App/ifsf/etc/huanhao.spec", F_OK) == 0) {
//			RunLog("## oilCnt: %d", gpIfsf_shm->oilCnt);
			if (gpIfsf_shm->oilCnt > 8) {
				RunLog("ERROR:Retalix Ŀǰ��ʵ�ֻ��Ʋ��������õ���Ʒ�����ڰ���!\n");
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
				// ��������û������, ������
				continue;
			}

			if (gpIfsf_shm->node_channel[i].node == pGun_Config->node) {
				for(n = 0; n < FP_PR_CNT; n++) {
					if(memcmp(dispenser_data->fp_pr_data[n].Prod_Nb, \
								pGun_Config->oil_no,4) == 0) {
					      break;                           	
					}
				}

				//�õ���Ʒ��ַ,����Ҳ���������product database ��λ������һ��������¼��ַ		   
				if (n < FP_PR_CNT) {
					n = n + 1; 
				} else {
					for(n = 0; n < FP_PR_CNT; n++) {
						if ((dispenser_data->fp_pr_data[n].Prod_Nb[0] == 0 ) && \
							(dispenser_data->fp_pr_data[n].Prod_Nb[1] == 0 ) && \
							(dispenser_data->fp_pr_data[n].Prod_Nb[2] == 0 ) && \
							(dispenser_data->fp_pr_data[n].Prod_Nb[3] == 0 )) { // Ϊ��
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

			//�������ź��߼�ǹ������logical nozzle database �е����ݣ����޸�pr_id ��physical_noz_id
			dispenser_data->fp_ln_data[pGun_Config->logfp - 1][pGun_Config->logical_gun_no - 1].PR_Id = n;
			dispenser_data->fp_ln_data[pGun_Config->logfp - 1][pGun_Config->logical_gun_no - 1].Physical_Noz_Id \
											= pGun_Config->physical_gun_no;
		                  
			}
		}

			//���ͼ۳�ʼ����ifsf ���ݿ���
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



	//��ʼ��videer-root���������� 

	bzero(VrPanelNo,sizeof(VrPanelNo));
	PreVrPanelNo = 0;
	for (i = 0; i < NodeRecCount; i++) {
		for (j = 0; j < gpIfsf_shm->node_channel[i].cntOfFps; j++){			
			VrPanelNo[i][j] +=PreVrPanelNo + 1;
			PreVrPanelNo = VrPanelNo[i][j];			
		}
	}	

	// ��ʼ��opw-root������ǹ��

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
		RunLog("��ʼ��Һλ�����ݵ������ڴ�ʧ��");
		return -1;
	}

	gpGunShm->sysStat = PAUSE_PROC;
	gpIfsf_shm->auth_flag = DEFAULT_AUTH_MODE;
  	ret = DefaultSV_DATA();
	if(ret < 0){
		UserLog( "FCC PCD ͨѶ���ݿ��ʼ������" );		
		return -1;
	}
	gpGunShm->chnCnt = 0;
	gpGunShm->portCnt = 0;

	for( i = 0; i < MAX_CHANNEL_CNT; i++ ) {	/*��ͨ������*/
		if( gpGunShm->chnLogUsed[i] == 1 ) {
			gpGunShm->chnCnt++;
		}
	}

	for( i = 0; i < MAX_PORT_CNT; i++ ) {
		if(gpGunShm->portLogUsed[i] == 1 ) {	/*�ô�������*/
			gpGunShm->portCnt++;
		}
	}

	gpGunShm->sysStat = 0;

	// Inital flag: gpIfsf_shm->is_new_kernel;
	is_new_version_kernel();


	// ��ʼ������ģ����Ҫ�ı��λwayne_rcap2l_flag
	ret = InitWayneRcap2lFlag();
	if (ret < 0) {
		RunLog("��ʼ��Wayneflagʧ��");
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
	/*����һ����Ϣ����.���ڼ������ڵĳ���ͬͨ����������ͨ��*/
	ret = CreateMsq(GUN_MSG_TRAN);
	if( ret < 0 ) {
		UserLog( "������Ϣ����ʧ�ܡ�����ϵͳ." );
		return -1;
	}

	/*����һ����Ϣ����.���ڼ�����̨�ĳ���ͬͨ����������ͨ��*/
	ret = CreateMsq(ORD_MSG_TRAN);
	if( ret < 0 ) {
		UserLog( "������Ϣ����ʧ�ܡ�����ϵͳ." );
		return -1;
	}

	// TLG
	ret = CreateMsq(TANK_MSG_ID);
	if( ret < 0 ) {
		UserLog( "������Ϣ����ʧ�ܡ�����ϵͳ." );
		return -1;
	}

	//���ڶ�ȡUDC����
	ret = CreateMsq(TOTAL_MSG_TRAN);
	if( ret < 0 ) {
		UserLog( "������Ϣ����ʧ�ܡ�����ϵͳ." );
		return -1;
	}

	//��������TCP ���͵���Ϣ����
	ret = CreateMsq(TCP_MSG_TRAN);
	if( ret < 0 ) {
		UserLog( "������ϢTCP_MSG_TRAN ����ʧ�ܡ�����ϵͳ." );
		return -1;
	}
	//������Ϣ����
	ret = CreateMsq(TCP_MSG_TRAN2);
	if( ret < 0 ) {
		UserLog( "������ϢTCP_MSG_TRAN2 ����ʧ�ܡ�����ϵͳ." );
		return -1;
	}
	/*����һ���ź���.���ڶԴ���дʱ����*/
	ret = CreateSem( PORT_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "�����ź���ʧ��" );
		return -1;
	}

	/*����һ���ź���.���ڼǳ�ʱ����Ϣ���м�ֵʱ����*/
	ret = CreateSem( ERR_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "�����ź���ʧ��" );
		return -1;
	}
	
	//#ifdef IFSF  ����һ���ź���.
	ret = CreateSem( IFSF_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "�����ź���ʧ��" );
		return -1;
	}
	ret = CreateSem( HEARTBEAT_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "�����ź���ʧ��" );
		return -1;
	}
	ret = CreateSem( HEARTBEAT_SEM_RECV_INDEX );
	if( ret < 0 ) {
		UserLog( "�����ź���ʧ��" );
		return -1;
	}
	ret = CreateSem( HEARTBEAT_SEM_CONVERT_INDEX );
	if( ret < 0 ) {
		UserLog( "�����ź���ʧ��" );
		return -1;
	}
	ret = CreateSem( IFSF_DP_TR_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "�����ź���ʧ��" );
		return -1;
	}	
				
	ret = CreateSem( IFSF_DP_SEM_INDEX );
	if( ret < 0 ) {
		UserLog( "�����ź���ʧ��" );
		return -1;
	}
	ret = CreateSem( CD_SOCK_INDEX);
	if( ret < 0 ) {
		UserLog( "�����ź���ʧ��" );
		return -1;
	}
	
	ret = CreateSem(WAYNE_RCAP2L_SEM_INDEX);
	if (ret < 0) {
		UserLog("�����ź���WAYNE_RCAP2Lʧ��");
		return -1;
	}
	ret = CreateSem(TCC_ADJUST_SEM_INDEX);
	if (ret < 0) {
		UserLog("�����ź���TCC_ADJUST_SEM_INDEXʧ��");
		return -1;
	}
	//#endif

	return 0;

}






