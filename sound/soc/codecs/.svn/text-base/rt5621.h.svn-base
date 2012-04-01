#ifndef _RT5621_H
#define _RT5621_H


//Index of Codec Register definition
#define RT5621_RESET						0X00			//RESET CODEC TO DEFAULT
#define RT5621_SPK_OUT_VOL					0X02			//SPEAKER OUT VOLUME
#define RT5621_HP_OUT_VOL					0X04			//HEADPHONE OUTPUT VOLUME
#define RT5621_MONO_OUT_VOL			          0X06			//MONO OUTPUT VOLUME
#define RT5621_AUX_IN_VOL 				     0x08			//AUX Input volume
#define RT5621_LINE_IN_VOL					0X0A			//LINE IN VOLUME
#define RT5621_STEREO_DAC_VOL				     0X0C			//STEREO DAC VOLUME
#define RT5621_MIC_VOL						0X0E			//MICROPHONE VOLUME
#define RT5621_MIC_ROUTING_CTRL				0X10			//MIC ROUTING CONTROL
#define RT5621_ADC_REC_GAIN					0X12			//ADC RECORD GAIN
#define RT5621_ADC_REC_MIXER				     0X14			//ADC RECORD MIXER CONTROL
#define RT5621_OUTPUT_MIXER_CTRL			     0X1C			//OUTPUT MIXER CONTROL
#define RT5621_MIC_CTRL						0X22			//MICROPHONE CONTROL
#define RT5621_MAIN_SDP_CTRL				     0X34			//MAIN SERIAL DATA PORT CONTROL(STEREO I2S)
#define RT5621_STEREO_DAC_CLK_CTRL		          0x36			//stereo da ad clk control
#define RT5621_PWR_MANAG_ADD1				     0X3A			//POWER MANAGMENT ADDITION 1
#define RT5621_PWR_MANAG_ADD2				     0X3C			//POWER MANAGMENT ADDITION 2
#define RT5621_PWR_MANAG_ADD3				     0X3E			//POWER MANAGMENT ADDITION 3
#define RT5621_ADDITION_CTRL				     0X40			//GENERAL addition CONTROL REGISTER
#define RT5621_GLOBAL_CLK_CTRL_REG				0X42			//global clk control  REGISTER
#define RT5621_PLL_CTRL						0X44			//PLL CONTROL
#define RT5621_GPIO_PIN_CTRL				     0x4a			//GPIO¡¡Out pin control
#define RT5621_GPIO_PIN_CONFIG				0X4C			//GPIO PIN CONFIGURATION
#define RT5621_GPIO_PIN_POLARITY			     0X4E			//GPIO PIN POLARITY
#define RT5621_GPIO_PIN_STICKY				0X50			//GPIO PIN STICKY
#define RT5621_GPIO_PIN_WAKEUP				0X52			//GPIO PIN WAKE UP
#define RT5621_GPIO_PIN_STATUS				0X54			//GPIO PIN STATUS
#define RT5621_GPIO_PIN_SHARING				0X56			//GPIO PIN SHARING
#define RT5621_OVER_TEMP_CURR_STATUS		     0X58			//OVER TEMPERATURE AND CURRENT STATUS
#define RT5621_JD_CTRL_REG					0x5a			//jd func control reg
#define RT5621_MISC_CTRL					     0X5E			//MISC CONTROL
#define RT5621_PSEDUEO_SPATIAL_CTRL			0X60			//PSEDUEO STEREO /SPATIAL EFFECT BLOCK CONTROL
#define RT5621_EQ_CTRL_REG					0x62           //eq control
#define RT5621_EQ_STATUS_REG					0x64			//eq status
#define RT5621_EQ_MODE_ENABLE					0x66			//eq mode
#define RT5621_AVC_CTRL_REG					0x68			//avc control
#define RT5621_INDEX_ADDRESS				     0X6A			//INDEX ADDRESS
#define RT5621_INDEX_DATA					0X6C			//INDEX DATA
#define RT5621_EQ_STATUS					     0X6E			//EQ STATUS
#define RT5621_VENDOR_ID1	  		    	     0x7C			//VENDOR ID1
#define RT5621_VENDOR_ID2	  		    	     0x7E			//VENDOR ID2


//*************************************************************************************************
//Bit define of Codec Register
//*************************************************************************************************
//global definition
#define RT_L_MUTE				(0x1<<15)		//Mute Left Control
#define RT_L_ZC					(0x1<<14)		//Mute Left Zero-Cross Detector Control
#define RT_R_MUTE				(0x1<<7)		//Mute Right Control
#define RT_R_ZC					(0x1<<6)		//Mute Right Zero-Cross Detector Control
#define RT_M_HP_MIXER				(0x1<<15)		//Mute source to HP Mixer
#define RT_M_SPK_MIXER				(0x1<<14)		//Mute source to Speaker Mixer
#define RT_M_MONO_MIXER				(0x1<<13)		//Mute source to Mono Mixer

//Phone Input/MONO Output Volume(0x08)
#define M_PHONEIN_TO_HP_MIXER			(0x1<<15)		//Mute Phone In volume to HP mixer
#define M_PHONEIN_TO_SPK_MIXER			(0x1<<14)		//Mute Phone In volume to speaker mixer
#define M_MONO_OUT_VOL				(0x1<<7)		//Mute Mono output volume


//Mic Routing Control(0x10)
#define M_MIC1_TO_HP_MIXER			(0x1<<15)		//Mute MIC1 to HP mixer
#define M_MIC1_TO_SPK_MIXER			(0x1<<14)		//Mute MiC1 to SPK mixer
#define M_MIC1_TO_MONO_MIXER			(0x1<<13)		//Mute MIC1 to MONO mixer
#define MIC1_DIFF_INPUT_CTRL			(0x1<<12)		//MIC1 different input control
#define M_MIC2_TO_HP_MIXER			(0x1<<7)		//Mute MIC2 to HP mixer
#define M_MIC2_TO_SPK_MIXER			(0x1<<6)		//Mute MiC2 to SPK mixer
#define M_MIC2_TO_MONO_MIXER			(0x1<<5)		//Mute MIC2 to MONO mixer
#define MIC2_DIFF_INPUT_CTRL			(0x1<<4)		//MIC2 different input control

//ADC Record Gain(0x12)
#define M_ADC_L_TO_HP_MIXER			(0x1<<15)		//Mute left of ADC to HP Mixer
#define M_ADC_R_TO_HP_MIXER			(0x1<<14)		//Mute right of ADC to HP Mixer
#define M_ADC_L_TO_MONO_MIXER			(0x1<<13)		//Mute left of ADC to MONO Mixer
#define M_ADC_R_TO_MONO_MIXER			(0x1<<12)		//Mute right of ADC to MONO Mixer
#define ADC_L_GAIN_MASK				(0x1f<<7)		//ADC Record Gain Left channel Mask
#define ADC_L_ZC_DET				(0x1<<6)		//ADC Zero-Cross Detector Control
#define ADC_R_ZC_DET				(0x1<<5)		//ADC Zero-Cross Detector Control
#define ADC_R_GAIN_MASK					(0x1f<<0)		//ADC Record Gain Right channel Mask

//Voice DAC Output Volume(0x18)
#define M_V_DAC_TO_HP_MIXER				(0x1<<15)
#define M_V_DAC_TO_SPK_MIXER			(0x1<<14)
#define M_V_DAC_TO_MONO_MIXER			(0x1<<13)

//Output Mixer Control(0x1C)
#define	SPKL_INPUT_SEL_MASK				(0x3<<14)
#define	SPKL_INPUT_SEL_MONO				(0x3<<14)
#define	SPKL_INPUT_SEL_SPK				(0x2<<14)
#define	SPKL_INPUT_SEL_HPL				(0x1<<14)
#define	SPKL_INPUT_SEL_VMID				(0x0<<14)

#define SPK_OUT_SEL_CLASS_D				(0x1<<13)
#define SPK_OUT_SEL_CLASS_AB			(0x0<<13)

#define	SPKR_INPUT_SEL_MASK				(0x3<<11)
#define	SPKR_INPUT_SEL_MONO_MIXER		(0x3<<11)
#define	SPKR_INPUT_SEL_SPK_MIXER		(0x2<<11)
#define	SPKR_INPUT_SEL_HPR_MIXER		(0x1<<11)
#define	SPKR_INPUT_SEL_VMID				(0x0<<11)

#define HPL_INPUT_SEL_HPL_MIXER			(0x1<<9)
#define HPR_INPUT_SEL_HPR_MIXER			(0x1<<8)

#define MONO_INPUT_SEL_MASK				(0x3<<6)
#define MONO_INPUT_SEL_MONO_MIXER		(0x3<<6)
#define MONO_INPUT_SEL_SPK_MIXER		(0x2<<6)
#define MONO_INPUT_SEL_HP_MIXER			(0x1<<6)
#define MONO_INPUT_SEL_VMID				(0x0<<6)

//Micphone Control define(0x22)
#define MIC1		1
#define MIC2		2
#define MIC_BIAS_90_PRECNET_AVDD	1
#define	MIC_BIAS_75_PRECNET_AVDD	2

#define MIC1_BOOST_CONTROL_MASK		(0x3<<10)
#define MIC1_BOOST_CONTROL_BYPASS	(0x0<<10)
#define MIC1_BOOST_CONTROL_20DB		(0x1<<10)
#define MIC1_BOOST_CONTROL_30DB		(0x2<<10)
#define MIC1_BOOST_CONTROL_40DB		(0x3<<10)

#define MIC2_BOOST_CONTROL_MASK		(0x3<<8)
#define MIC2_BOOST_CONTROL_BYPASS	(0x0<<8)
#define MIC2_BOOST_CONTROL_20DB		(0x1<<8)
#define MIC2_BOOST_CONTROL_30DB		(0x2<<8)
#define MIC2_BOOST_CONTROL_40DB		(0x3<<8)

#define MIC1_BIAS_VOLT_CTRL_MASK	(0x1<<5)
#define MIC1_BIAS_VOLT_CTRL_90P		(0x0<<5)
#define MIC1_BIAS_VOLT_CTRL_75P		(0x1<<5)

#define MIC2_BIAS_VOLT_CTRL_MASK	(0x1<<4)
#define MIC2_BIAS_VOLT_CTRL_90P		(0x0<<4)
#define MIC2_BIAS_VOLT_CTRL_75P		(0x1<<4)

//PowerDown control of register(0x26)
//power management bits
#define RT_PWR_PR7					(0x1<<15)	//write this bit to power down the Speaker Amplifier
#define RT_PWR_PR6					(0x1<<14)	//write this bit to power down the Headphone Out and MonoOut
#define RT_PWR_PR5					(0x1<<13)	//write this bit to power down the internal clock(without PLL)
#define RT_PWR_PR3					(0x1<<11)	//write this bit to power down the mixer(vref/vrefout out off)
#define RT_PWR_PR2					(0x1<<10)	//write this bit to power down the mixer(vref/vrefout still on)
#define RT_PWR_PR1					(0x1<<9) 	//write this bit to power down the dac
#define RT_PWR_PR0					(0x1<<8) 	//write this bit to power down the adc
#define RT_PWR_REF					(0x1<<3)	//read only
#define RT_PWR_ANL					(0x1<<2)	//read only
#define RT_PWR_DAC					(0x1<<1)	//read only
#define RT_PWR_ADC					(0x1)		//read only


//Main Serial Data Port Control(0x34)
#define MAIN_I2S_MODE_SEL			(0x1<<15)		//0:Master mode 1:Slave mode
#define MAIN_I2S_SADLRCK_CTRL		(0x1<<14)		//0:Disable,ADC and DAC use the same fs,1:Enable
#define MAIN_I2S_BCLK_POLARITY		(0x1<<12)		//0:Normal 1:Invert
#define MAIN_I2S_DA_CLK_SOUR		(0x1<<11)		//0:from DA Filter,1:from DA Sigma Delta Clock Divider

//I2S DA SIGMA delta clock divider
#define MAIN_I2S_CLK_DIV_MASK		(0x7<<8)
#define MAIN_I2S_CLK_DIV_2			(0x0<<8)
#define MAIN_I2S_CLK_DIV_4			(0x1<<8)
#define MAIN_I2S_CLK_DIV_8			(0x2<<8)
#define MAIN_I2S_CLK_DIV_16			(0x3<<8)
#define MAIN_I2S_CLK_DIV_32			(0x4<<8)
#define MAIN_I2S_CLK_DIV_64			(0x5<<8)

#define MAIN_I2S_PCM_MODE			(0x1<<6)		//PCM    	0:mode A				,1:mode B
													//Non PCM	0:Normal SADLRCK/SDALRCK,1:Invert SADLRCK/SDALRCK
//Data Length Slection
#define MAIN_I2S_DL_MASK			(0x3<<2)		//main i2s Data Length mask
#define MAIN_I2S_DL_16				(0x0<<2)		//16 bits
#define MAIN_I2S_DL_20				(0x1<<2)		//20 bits
#define	MAIN_I2S_DL_24				(0x2<<2)		//24 bits
#define MAIN_I2S_DL_32				(0x3<<2)		//32 bits

//PCM Data Format Selection
#define MAIN_I2S_DF_MASK			(0x3)			//main i2s Data Format mask
#define MAIN_I2S_DF_I2S				(0x0)			//I2S FORMAT
#define MAIN_I2S_DF_RIGHT			(0x1)			//RIGHT JUSTIFIED format
#define	MAIN_I2S_DF_LEFT			(0x2)			//LEFT JUSTIFIED  format
#define MAIN_I2S_DF_PCM				(0x3)			//PCM format

//Extend Serial Data Port Control(0x36)
#define EXT_I2S_FUNC_ENABLE			(0x1<<15)		//Enable PCM interface on GPIO 1,3,4,5  0:GPIO function,1:Voice PCM interface
#define EXT_I2S_MODE_SEL			(0x1<<14)		//0:Master	,1:Slave
#define EXT_I2S_VOICE_ADC_EN		(0x1<<8)		//ADC_L=Stereo, ADC_R=Voice
#define EXT_I2S_BCLK_POLARITY		(0x1<<7)		//0:Normal 1:Invert
#define EXT_I2S_PCM_MODE			(0x1<<6)		//PCM    	0:mode A				,1:mode B
													//Non PCM	0:Normal SADLRCK/SDALRCK,1:Invert SADLRCK/SDALRCK
//Data Length Slection
#define EXT_I2S_DL_MASK				(0x3<<2)		//Extend i2s Data Length mask
#define EXT_I2S_DL_32				(0x3<<2)		//32 bits
#define	EXT_I2S_DL_24				(0x2<<2)		//24 bits
#define EXT_I2S_DL_20				(0x1<<2)		//20 bits
#define EXT_I2S_DL_16				(0x0<<2)		//16 bits

//Voice Data Format
#define EXT_I2S_DF_MASK				(0x3)			//Extend i2s Data Format mask
#define EXT_I2S_DF_I2S				(0x0)			//I2S FORMAT
#define EXT_I2S_DF_RIGHT			(0x1)			//RIGHT JUSTIFIED format
#define	EXT_I2S_DF_LEFT				(0x2)			//LEFT JUSTIFIED  format
#define EXT_I2S_DF_PCM				(0x3)			//PCM format

//Power managment addition 1 (0x3A),0:Disable,1:Enable
#define PWR_HI_R_LOAD_MONO			(0x1<<15)
#define PWR_HI_R_LOAD_HP			(0x1<<14)
#define PWR_ZC_DET_PD				(0x1<<13)
#define PWR_MAIN_I2S				(0x1<<11)
#define	PWR_MIC_BIAS1_DET			(0x1<<5)
#define	PWR_MIC_BIAS2_DET			(0x1<<4)
#define PWR_MIC_BIAS1				(0x1<<3)
#define PWR_MIC_BIAS2				(0x1<<2)
#define PWR_MAIN_BIAS				(0x1<<1)
#define PWR_DAC_REF					(0x1)


//Power managment addition 2(0x3C),0:Disable,1:Enable
#define EN_THREMAL_SHUTDOWN			(0x1<<15)
#define PWR_CLASS_AB				(0x1<<14)
#define PWR_MIXER_VREF				(0x1<<13)
#define PWR_PLL						(0x1<<12)
#define PWR_VOICE_CLOCK				(0x1<<10)
#define PWR_L_DAC_CLK				(0x1<<9)
#define PWR_R_DAC_CLK				(0x1<<8)
#define PWR_L_ADC_CLK_GAIN			(0x1<<7)
#define PWR_R_ADC_CLK_GAIN			(0x1<<6)
#define PWR_L_HP_MIXER				(0x1<<5)
#define PWR_R_HP_MIXER				(0x1<<4)
#define PWR_SPK_MIXER				(0x1<<3)
#define PWR_MONO_MIXER				(0x1<<2)
#define PWR_L_ADC_REC_MIXER			(0x1<<1)
#define PWR_R_ADC_REC_MIXER			(0x1)


//Power managment addition 3(0x3E),0:Disable,1:Enable
#define PWR_MONO_VOL				(0x1<<14)
#define PWR_SPK_LN_OUT				(0x1<<13)
#define PWR_SPK_RN_OUT				(0x1<<12)
#define PWR_HP_L_OUT				(0x1<<11)
#define PWR_HP_R_OUT				(0x1<<10)
#define PWR_SPK_L_OUT				(0x1<<9)
#define PWR_SPK_R_OUT				(0x1<<8)
#define PWR_LINE_IN_L				(0x1<<7)
#define PWR_LINE_IN_R				(0x1<<6)
#define PWR_PHONE_MIXER				(0x1<<5)
#define PWR_PHONE_VOL				(0x1<<4)
#define PWR_MIC1_VOL_CTRL			(0x1<<3)
#define PWR_MIC2_VOL_CTRL			(0x1<<2)
#define PWR_MIC1_BOOST				(0x1<<1)
#define PWR_MIC2_BOOST				(0x1)

//General Purpose Control Register 1(0x40)
#define GP_CLK_FROM_PLL					(0x1<<15)	//Clock source from PLL output
#define GP_CLK_FROM_MCLK				(0x0<<15)	//Clock source from MCLK output

#define GP_HP_AMP_CTRL_MASK				(0x3<<8)
#define GP_HP_AMP_CTRL_RATIO_100		(0x0<<8)		//1.00 Vdd
#define GP_HP_AMP_CTRL_RATIO_125		(0x1<<8)		//1.25 Vdd
#define GP_HP_AMP_CTRL_RATIO_150		(0x2<<8)		//1.50 Vdd

#define GP_SPK_D_AMP_CTRL_MASK			(0x3<<6)
#define GP_SPK_D_AMP_CTRL_RATIO_175		(0x0<<6)		//1.75 Vdd
#define GP_SPK_D_AMP_CTRL_RATIO_150		(0x1<<6)		//1.50 Vdd
#define GP_SPK_D_AMP_CTRL_RATIO_125		(0x2<<6)		//1.25 Vdd
#define GP_SPK_D_AMP_CTRL_RATIO_100		(0x3<<6)		//1.00 Vdd

#define GP_SPK_AB_AMP_CTRL_MASK			(0x7<<3)
#define GP_SPK_AB_AMP_CTRL_RATIO_225	(0x0<<3)		//2.25 Vdd
#define GP_SPK_AB_AMP_CTRL_RATIO_200	(0x1<<3)		//2.00 Vdd
#define GP_SPK_AB_AMP_CTRL_RATIO_175	(0x2<<3)		//1.75 Vdd
#define GP_SPK_AB_AMP_CTRL_RATIO_150	(0x3<<3)		//1.50 Vdd
#define GP_SPK_AB_AMP_CTRL_RATIO_125	(0x4<<3)		//1.25 Vdd
#define GP_SPK_AB_AMP_CTRL_RATIO_100	(0x5<<3)		//1.00 Vdd

//General Purpose Control Register 2(0x42)
#define GP2_VOICE_TO_DIG_PATH_EN		(0x1<<15)
#define GP2_CLASS_AB_SEL_DIFF			(0x0<<13)		//Differential Mode of Class AB
#define GP2_CLASS_D_SEL_SINGLE			(0x1<<13)		//Single End mode of Class AB
#define GP2_PLL_PRE_DIV_1				(0x0<<0)		//PLL pre-Divider 1
#define GP2_PLL_PRE_DIV_2				(0x1<<0)		//PLL pre-Divider 2

//PLL Control(0x44)
#define PLL_M_CODE_MASK				0xF				//PLL M code mask
#define PLL_K_CODE_MASK				(0x7<<4)		//PLL K code mask
#define PLL_BYPASS_N				(0x1<<7)		//bypass PLL N code
#define PLL_N_CODE_MASK				(0xFF<<8)		//PLL N code mask

#define PLL_CTRL_M_VAL(m)		((m)&0xf)
#define PLL_CTRL_K_VAL(k)		(((k)&0x7)<<4)
#define PLL_CTRL_N_VAL(n)		(((n)&0xff)<<8)


//GPIO Pin Configuration(0x4C)
#define GPIO_1						(0x1<<1)
#define	GPIO_2						(0x1<<2)
#define	GPIO_3						(0x1<<3)
#define GPIO_4						(0x1<<4)
#define GPIO_5						(0x1<<5)


////INTERRUPT CONTROL(0x5E)
#define DISABLE_FAST_VREG			(0x1<<15)

#define AVC_TARTGET_SEL_MASK		(0x3<<12)
#define	AVC_TARTGET_SEL_NONE		(0x0<<12)
#define	AVC_TARTGET_SEL_R 			(0x1<<12)
#define	AVC_TARTGET_SEL_L			(0x2<<12)
#define	AVC_TARTGET_SEL_BOTH		(0x3<<12)

//Stereo DAC Clock Control 1(0x60)
#define STEREO_SCLK_DIV1_MASK		(0xF<<12)
#define STEREO_SCLK_DIV1_1			(0x0<<12)
#define STEREO_SCLK_DIV1_2			(0x1<<12)
#define STEREO_SCLK_DIV1_3			(0x2<<12)
#define STEREO_SCLK_DIV1_4			(0x3<<12)
#define STEREO_SCLK_DIV1_5			(0x4<<12)
#define STEREO_SCLK_DIV1_6			(0x5<<12)
#define STEREO_SCLK_DIV1_7			(0x6<<12)
#define STEREO_SCLK_DIV1_8			(0x7<<12)
#define STEREO_SCLK_DIV1_9			(0x8<<12)
#define STEREO_SCLK_DIV1_10			(0x9<<12)
#define STEREO_SCLK_DIV1_11			(0xA<<12)
#define STEREO_SCLK_DIV1_12			(0xB<<12)
#define STEREO_SCLK_DIV1_13			(0xC<<12)
#define STEREO_SCLK_DIV1_14			(0xD<<12)
#define STEREO_SCLK_DIV1_15			(0xE<<12)
#define STEREO_SCLK_DIV1_16			(0xF<<12)

#define STEREO_SCLK_DIV2_MASK		(0x7<<8)
#define STEREO_SCLK_DIV2_2			(0x0<<8)
#define STEREO_SCLK_DIV2_4			(0x1<<8)
#define STEREO_SCLK_DIV2_8			(0x2<<8)
#define STEREO_SCLK_DIV2_16			(0x3<<8)
#define STEREO_SCLK_DIV2_32			(0x4<<8)

#define STEREO_AD_WCLK_DIV1_MASK	(0xF<<4)
#define STEREO_AD_WCLK_DIV1_1		(0x0<<4)
#define STEREO_AD_WCLK_DIV1_2		(0x1<<4)
#define STEREO_AD_WCLK_DIV1_3		(0x2<<4)
#define STEREO_AD_WCLK_DIV1_4		(0x3<<4)
#define STEREO_AD_WCLK_DIV1_5		(0x4<<4)
#define STEREO_AD_WCLK_DIV1_6		(0x5<<4)
#define STEREO_AD_WCLK_DIV1_7		(0x6<<4)
#define STEREO_AD_WCLK_DIV1_8		(0x7<<4)
#define STEREO_AD_WCLK_DIV1_9		(0x8<<4)
#define STEREO_AD_WCLK_DIV1_10		(0x9<<4)
#define STEREO_AD_WCLK_DIV1_11		(0xA<<4)
#define STEREO_AD_WCLK_DIV1_12		(0xB<<4)
#define STEREO_AD_WCLK_DIV1_13		(0xC<<4)
#define STEREO_AD_WCLK_DIV1_14		(0xD<<4)
#define STEREO_AD_WCLK_DIV1_15		(0xE<<4)
#define STEREO_AD_WCLK_DIV1_16		(0xF<<4)

#define STEREO_AD_WCLK_DIV2_MASK	(0x7<<1)
#define STEREO_AD_WCLK_DIV2_2		(0x0<<1)
#define STEREO_AD_WCLK_DIV2_4		(0x1<<1)
#define STEREO_AD_WCLK_DIV2_8		(0x2<<1)
#define STEREO_AD_WCLK_DIV2_16		(0x3<<1)
#define STEREO_AD_WCLK_DIV2_32		(0x4<<1)

#define STEREO_DA_WCLK_DIV_MASK		(1)
#define STEREO_DA_WCLK_DIV_32		(0)
#define STEREO_DA_WCLK_DIV_64		(1)

//Stereo DAC Clock Control 2(0x62)
#define STEREO_DA_FILTER_DIV1_MASK	(0xF<<12)
#define STEREO_DA_FILTER_DIV1_1		(0x0<<12)
#define STEREO_DA_FILTER_DIV1_2		(0x1<<12)
#define STEREO_DA_FILTER_DIV1_3		(0x2<<12)
#define STEREO_DA_FILTER_DIV1_4		(0x3<<12)
#define STEREO_DA_FILTER_DIV1_5		(0x4<<12)
#define STEREO_DA_FILTER_DIV1_6		(0x5<<12)
#define STEREO_DA_FILTER_DIV1_7		(0x6<<12)
#define STEREO_DA_FILTER_DIV1_8		(0x7<<12)
#define STEREO_DA_FILTER_DIV1_9		(0x8<<12)
#define STEREO_DA_FILTER_DIV1_10	(0x9<<12)
#define STEREO_DA_FILTER_DIV1_11	(0xA<<12)
#define STEREO_DA_FILTER_DIV1_12	(0xB<<12)
#define STEREO_DA_FILTER_DIV1_13	(0xC<<12)
#define STEREO_DA_FILTER_DIV1_14	(0xD<<12)
#define STEREO_DA_FILTER_DIV1_15	(0xE<<12)
#define STEREO_DA_FILTER_DIV1_16	(0xF<<12)

#define STEREO_DA_FILTER_DIV2_MASK	(0x7<<9)
#define STEREO_DA_FILTER_DIV2_2		(0x0<<9)
#define STEREO_DA_FILTER_DIV2_4		(0x1<<9)
#define STEREO_DA_FILTER_DIV2_8		(0x2<<9)
#define STEREO_DA_FILTER_DIV2_16	(0x3<<9)
#define STEREO_DA_FILTER_DIV2_32	(0x4<<9)

#define STEREO_AD_FILTER_DIV1_MASK	(0xF<<4)
#define STEREO_AD_FILTER_DIV1_1		(0x0<<4)
#define STEREO_AD_FILTER_DIV1_2		(0x1<<4)
#define STEREO_AD_FILTER_DIV1_3		(0x2<<4)
#define STEREO_AD_FILTER_DIV1_4		(0x3<<4)
#define STEREO_AD_FILTER_DIV1_5		(0x4<<4)
#define STEREO_AD_FILTER_DIV1_6		(0x5<<4)
#define STEREO_AD_FILTER_DIV1_7		(0x6<<4)
#define STEREO_AD_FILTER_DIV1_8		(0x7<<4)
#define STEREO_AD_FILTER_DIV1_9		(0x8<<4)
#define STEREO_AD_FILTER_DIV1_10	(0x9<<4)
#define STEREO_AD_FILTER_DIV1_11	(0xA<<4)
#define STEREO_AD_FILTER_DIV1_12	(0xB<<4)
#define STEREO_AD_FILTER_DIV1_13	(0xC<<4)
#define STEREO_AD_FILTER_DIV1_14	(0xD<<4)
#define STEREO_AD_FILTER_DIV1_15	(0xE<<4)
#define STEREO_AD_FILTER_DIV1_16	(0xF<<4)

#define STEREO_AD_FILTER_DIV2_MASK	(0x7<<1)
#define STEREO_AD_FILTER_DIV2_2		(0x0<<1)
#define STEREO_AD_FILTER_DIV2_4		(0x1<<1)
#define STEREO_AD_FILTER_DIV2_8		(0x2<<1)
#define STEREO_AD_FILTER_DIV2_16	(0x3<<1)
#define STEREO_AD_FILTER_DIV2_32	(0x4<<1)


//Voice DAC PCM Clock Control 1(0x64)
#define VOICE_MCLK_SEL_MASK			(0x1<<15)
#define VOICE_MCLK_SEL_MCLK_INPUT	(0x0<<15)
#define VOICE_MCLK_SEL_PLL_OUTPUT	(0x1<<15)

#define VOICE_SYSCLK_SEL_MASK		(0x1<<14)
#define VOICE_SYSCLK_SEL_MCLK		(0x0<<14)
#define VOICE_SYSCLK_SEL_EXTCLK		(0x1<<14)

#define VOICE_WCLK_DIV_MASK			(0x1<<13)
#define VOICE_WCLK_DIV_32			(0x0<<13)
#define VOICE_WCLK_DIV_64			(0x1<<13)

#define VOICE_EXTCLK_OUT_DIV_MASK   (0x7<<8)
#define VOICE_EXTCLK_OUT_DIV_1		(0x0<<8)
#define VOICE_EXTCLK_OUT_DIV_2   	(0x1<<8)
#define VOICE_EXTCLK_OUT_DIV_4   	(0x2<<8)
#define VOICE_EXTCLK_OUT_DIV_8   	(0x3<<8)
#define VOICE_EXTCLK_OUT_DIV_16   	(0x4<<8)

#define VOICE_SCLK_DIV1_MASK		(0xF<<4)
#define VOICE_SCLK_DIV1_1			(0x0<<4)
#define VOICE_SCLK_DIV1_2			(0x1<<4)
#define VOICE_SCLK_DIV1_3			(0x2<<4)
#define VOICE_SCLK_DIV1_4			(0x3<<4)
#define VOICE_SCLK_DIV1_5			(0x4<<4)
#define VOICE_SCLK_DIV1_6			(0x5<<4)
#define VOICE_SCLK_DIV1_7			(0x6<<4)
#define VOICE_SCLK_DIV1_8			(0x7<<4)
#define VOICE_SCLK_DIV1_9			(0x8<<4)
#define VOICE_SCLK_DIV1_10			(0x9<<4)
#define VOICE_SCLK_DIV1_11			(0xA<<4)
#define VOICE_SCLK_DIV1_12			(0xB<<4)
#define VOICE_SCLK_DIV1_13			(0xC<<4)
#define VOICE_SCLK_DIV1_14			(0xD<<4)
#define VOICE_SCLK_DIV1_15			(0xE<<4)
#define VOICE_SCLK_DIV1_16			(0xF<<4)

#define VOICE_SCLK_DIV2_MASK		(0x7)
#define VOICE_SCLK_DIV2_2			(0x0)
#define VOICE_SCLK_DIV2_4			(0x1)
#define VOICE_SCLK_DIV2_8			(0x2)
#define VOICE_SCLK_DIV2_16			(0x3)
#define VOICE_SCLK_DIV2_32			(0x4)

//Voice DAC PCM Clock Control 2(0x66)
#define VOICE_CLK_FILTER_SLAVE_DIV_MASK (0x1<<15)
#define VOICE_CLK_FILTER_SLAVE_DIV_1 	(0x0<<15)
#define VOICE_CLK_FILTER_SLAVE_DIV_2	(0x1<<15)

#define VOICE_FILTER_CLK_F_MASK		(0x1<<14)
#define VOICE_FILTER_CLK_F_MCLK		(0x0<<14)
#define VOICE_FILTER_CLK_F_VBCLK	(0X1<<14)

#define VOICE_AD_DA_FILTER_SEL_MASK  (0x1<<13)
#define VOICE_AD_DA_FILTER_SEL_128X	(0x0<<13)
#define VOICE_AD_DA_FILTER_SEL_64X	(0x1<<13)

#define VOICE_CLK_FILTER_DIV1_MASK	(0xF<<4)
#define VOICE_CLK_FILTER_DIV1_1		(0x0<<4)
#define VOICE_CLK_FILTER_DIV1_2		(0x1<<4)
#define VOICE_CLK_FILTER_DIV1_3		(0x2<<4)
#define VOICE_CLK_FILTER_DIV1_4		(0x3<<4)
#define VOICE_CLK_FILTER_DIV1_5		(0x4<<4)
#define VOICE_CLK_FILTER_DIV1_6		(0x5<<4)
#define VOICE_CLK_FILTER_DIV1_7		(0x6<<4)
#define VOICE_CLK_FILTER_DIV1_8		(0x7<<4)
#define VOICE_CLK_FILTER_DIV1_9		(0x8<<4)
#define VOICE_CLK_FILTER_DIV1_10	(0x9<<4)
#define VOICE_CLK_FILTER_DIV1_11	(0xA<<4)
#define VOICE_CLK_FILTER_DIV1_12	(0xB<<4)
#define VOICE_CLK_FILTER_DIV1_13	(0xC<<4)
#define VOICE_CLK_FILTER_DIV1_14	(0xD<<4)
#define VOICE_CLK_FILTER_DIV1_15	(0xE<<4)
#define VOICE_CLK_FILTER_DIV1_16	(0xF<<4)

#define VOICE_CLK_FILTER_DIV2_MASK	(0x7)
#define VOICE_CLK_FILTER_DIV2_2		(0x0)
#define VOICE_CLK_FILTER_DIV2_4		(0x1)
#define VOICE_CLK_FILTER_DIV2_8		(0x2)
#define VOICE_CLK_FILTER_DIV2_16	(0x3)
#define VOICE_CLK_FILTER_DIV2_32	(0x4)


//Psedueo Stereo & Spatial Effect Block Control(0x68)
#define SPATIAL_CTRL_EN				(0x1<<15)
#define ALL_PASS_FILTER_EN			(0x1<<14)
#define PSEUDO_STEREO_EN			(0x1<<13)
#define STEREO_EXPENSION_EN			(0x1<<12)

#define SPATIAL_GAIN_MASK			(0x3<<6)
#define SPATIAL_GAIN_1_0			(0x0<<6)
#define SPATIAL_GAIN_1_5			(0x1<<6)
#define SPATIAL_GAIN_2_0			(0x2<<6)

#define SPATIAL_RATIO_MASK			(0x3<<4)
#define SPATIAL_RATIO_0_0			(0x0<<4)
#define SPATIAL_RATIO_0_66			(0x1<<4)
#define SPATIAL_RATIO_1_0			(0x2<<4)

#define APF_MASK					(0x3)
#define APF_FOR_48K					(0x3)
#define APF_FOR_44_1K				(0x2)
#define APF_FOR_32K					(0x1)



struct rt5621_setup_data {
	unsigned short i2c_address;
	unsigned short i2c_bus;
};



#define RT5621_PLL_FR_MCLK			0
#define RT5621_PLL_FR_BCLK			1

//extern struct snd_soc_dai rt5621_dai[];
extern struct snd_soc_dai rt5621_dai;
extern struct snd_soc_codec_device soc_codec_dev_rt5621;

#define RT5621_MCLK       1
#define RT5621_DAC_CLKSEL 2
#define RT5621_ADC_CLKSEL 3
#define RT5621_CLKOUTSRC  4
#define RT5621_MCLKRATIO  5
#define RT5621_BCLKRATIO  6
#define RT5621_PAIF_CLKSEL 7

#define RT5621_CLKSRC_ADCMCLK 0
#define RT5621_CLKSRC_MCLK    1
#define RT5621_CLKSRC_PLLA    2
#define RT5621_CLKSRC_PLLB    3
#define RT5621_CLKSRC_OSC     4
#define RT5621_CLKSRC_NONE    5

//JY.Lai -- only 1 dai setting for RT5621
#define RT5621_DAI_SAIF   0

//#define USE_DAPM_CONTROL 1

#define USE_DAPM_CONTROL 0
#define REALTEK_HWDEP 0

#include <linux/ioctl.h>
#include <linux/types.h>
struct rt56xx_reg_state
{
	unsigned int reg_index;
	unsigned int reg_value;
};

struct rt56xx_cmd
{
	size_t number;
	struct rt56xx_reg_state __user *buf;
};


enum
{
	RT_READ_CODEC_REG_IOCTL = _IOR('R', 0x01, struct rt56xx_cmd),
	RT_READ_ALL_CODEC_REG_IOCTL = _IOR('R', 0x02, struct rt56xx_cmd),
	RT_WRITE_CODEC_REG_IOCTL = _IOW('R', 0x03, struct rt56xx_cmd),
};

#endif
