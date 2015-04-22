#ifndef CONVER_H
#define CONVER_H
#include "CommunicateMcu.h"
#define BUFFER_SIZE	128
struct BoardInfo {
	int sn;
	int channel_type;
	float asr_minval;
	float asr_maxval;
	float phy_minval;
	float phy_maxval;
	int decimal_num;
	int ad_fixedpoint1;
	int ad_fixedpoint2;
	int ad_fixedpoint3;
	int ad_fixedpoint4;
};

struct Flags {
	int fixed_point_number;//几点标定，2:两点标定；3：三点标定；4：四点标定
	int sensor_conf;	//flag=1:enabled; flag=0:diable
	int adc_type;		//flag=1:TL1549; flag=2:ADS8320; flag=3:ADS1247
	int unit1_type;		//flag=1:AIN; flag=2:AOUT, flag=3:DIN; flag=4:DOUT
	int unit1_sub_type; //sub_flag=0:AIN_mA; sub_flag=1:AIN_V
	int unit2_type;
	int unit2_sub_type;
	int unit3_type;
	int unit3_sub_type;
	int unit4_type;
	int unit4_sub_type;
	int unit5_type;
	int unit5_sub_type;
	int unit6_type;
	int unit6_sub_type;
};

class Convert {
public:
	Convert();
	~Convert();
	int get_node_data(int sensor_number, unsigned char (&buffer)[BUFFER_SIZE]);
	int asr_to_ad_channel(int sensor_num, int channel_num, float asr_val);
	int get_unit_type(int sensor_id);
	float ad_to_asr_channel(int channel_num, int ad_val,int asr_type);
	int asr_to_phy_channel(int channel_num, float asr_val);
	int get_channel_sub_type(int channel_number);
private:
	int init_board_flag();
	int init_board_info();
	int ad_to_asr(int sensor_number, unsigned char (&buffer)[BUFFER_SIZE]);
	int asr_to_phy(int sensor_number, unsigned char (&buffer)[BUFFER_SIZE]);
	struct BoardInfo myboard[48];
	struct Flags	flags;
	CommunicateMcu mcu;
	float asr_val[8];//模拟值
	int channel_val[8];//转换后的物理值

};
#endif
