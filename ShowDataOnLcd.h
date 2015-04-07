#ifndef SHOWDATAONLCD_H
#define SHOWDATAONLCD_H
#include "CommunicateMcu.h"
#include "Convert.h"

class ShowDataOnLcd {
public:
	ShowDataOnLcd();
	~ShowDataOnLcd();
	void run();
private:
	CommunicateMcu mcu;
	Convert convert;
	int switch_input_channel_num;
	int relay_output_channel_num;
	int input_register_channel_num;
	int holding_register_channel_num;
};

#endif
