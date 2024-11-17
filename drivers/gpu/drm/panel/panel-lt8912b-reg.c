
/* mipi input 3210 swap */
/* #define LANE_SWAP */

/* mipi input lane pn swap */
/* #define PN_SWAP */

struct video_timing {
	unsigned short hfp;
	unsigned short hs;
	unsigned short hbp;
	unsigned short hact;
	unsigned short htotal;
	unsigned short vfp;
	unsigned short vs;
	unsigned short vbp;
	unsigned short vact;
	unsigned short vtotal;
	unsigned int pclk_khz;
};

struct lt8912b_reg {
	unsigned char reg;
	unsigned char data;
};

/*
 * this timing is mipi timing, please set these timing paremeter same with
 * actual mipi timing(processor's timing)
 * hfp, hs, hbp,hact,htotal,vfp, vs, vbp,vact,vtotal
 */
static struct video_timing video_1920x1080_60Hz = {88, 44, 148, 1920, 2200, 4, 5, 36, 1080, 1125};

static struct lt8912b_reg digital_clock_enable[] = {
	{0x02, 0xf7}, {0x08, 0xff}, {0x09, 0xff},
	{0x0a, 0xff}, {0x0b, 0x7c}, {0x0c, 0xff},
};

static struct lt8912b_reg tx_analog[] = {
	{0x31, 0xE1}, {0x32, 0xE1}, {0x33, 0x0c},
	{0x37, 0x00}, {0x38, 0x22}, {0x60, 0x82},
};

static struct lt8912b_reg cbus_analog[] = {
	{0x39, 0x45}, {0x3a, 0x00}, {0x3b, 0x00},
};

static struct lt8912b_reg hmdi_pll_analog[] = {
	{0x44, 0x31}, {0x55, 0x44}, {0x57, 0x01},
	{0x5a, 0x02},
};

static struct lt8912b_reg dds_config[] = {
	{0x4e, 0xaa}, {0x4f, 0xaa}, {0x50, 0x6a},
	{0x51, 0x80}, {0x1e, 0x4f}, {0x1f, 0x5e},
	{0x20, 0x01}, {0x21, 0x2c}, {0x22, 0x01},
	{0x23, 0xfa}, {0x24, 0x00}, {0x25, 0xc8},
	{0x26, 0x00}, {0x27, 0x5e}, {0x28, 0x01},
	{0x29, 0x2c}, {0x2a, 0x01}, {0x2b, 0xfa},
	{0x2c, 0x00}, {0x2d, 0xc8}, {0x2e, 0x00},
	{0x42, 0x64}, {0x43, 0x00}, {0x44, 0x04},
	{0x45, 0x00}, {0x46, 0x59}, {0x47, 0x00},
	{0x48, 0xf2}, {0x49, 0x06}, {0x4a, 0x00},
	{0x4b, 0x72}, {0x4c, 0x45}, {0x4d, 0x00},
	{0x52, 0x08}, {0x53, 0x00}, {0x54, 0xb2},
	{0x55, 0x00}, {0x56, 0xe4}, {0x57, 0x0d},
	{0x58, 0x00}, {0x59, 0xe4}, {0x5a, 0x8a},
	{0x5b, 0x00}, {0x5c, 0x34}, {0x51, 0x00},
};

static struct lt8912b_reg lvds_bypass[] = {
	{0x50, 0x24}, {0x51, 0x2d}, {0x52, 0x04},
	{0x69, 0x0e}, {0x69, 0x8e}, {0x6a, 0x00},
	{0x6c, 0xb8}, {0x6b, 0x51}, {0x04, 0xfb},
	{0x04, 0xff}, {0x7f, 0x00}, {0xa8, 0x13},
};

static struct lt8912b_reg lvds_output_on[] = {
	{0x02, 0xf7}, {0x02, 0xff}, {0x03, 0xcb},
	{0x03, 0xfb}, {0x03, 0xff}, {0x44, 0x30},
};

static struct lt8912 *g_lt = NULL;

static int lt8912b_write_byte(int idx, unsigned char addr, unsigned char data)
{
	int err;
	
	err = regmap_write(g_lt->regmap[idx], addr, data);
	mdelay(1);
	if (err) {
		dev_err(g_lt->dev, "write %x %x fail(%d)\n", addr, data, err);
		return -1;
	}
	
	return 0;
}

static unsigned char lt8912b_read_byte(int idx, unsigned char addr)
{
	unsigned int val;
	int err;
	
	err = regmap_read(g_lt->regmap[idx], addr, &val);
	if (err) {
		dev_err(g_lt->dev, "read %x fail(%d)\n", addr, err);
		return 0;
	}
	
	return val;
}

static void lt8912b_write_regs(int idx, struct lt8912b_reg *regs, unsigned int regs_num)
{
	int i;

	// dev_info(g_lt->dev, "regs num %d\n", regs_num);

	for(i = 0; i < regs_num; i++) {
		// dev_info(g_lt->dev, "write %x %x\n", regs[i].reg, regs[i].data);
		lt8912b_write_byte(idx, regs[i].reg, regs[i].data);
	}
}

static void lt8912b_digital_clock_enable(void)
{
	lt8912b_write_regs(I2C_1ST, digital_clock_enable, ARRAY_SIZE(digital_clock_enable));
}

static void lt8912b_tx_analog(void)
{
	lt8912b_write_regs(I2C_1ST, tx_analog, ARRAY_SIZE(tx_analog));
}

static void lt8912b_cbus_analog(void)
{
	lt8912b_write_regs(I2C_1ST, cbus_analog, ARRAY_SIZE(cbus_analog));
}

static void lt8912b_hmdi_pll_analog(void)
{
	lt8912b_write_regs(I2C_1ST, hmdi_pll_analog, ARRAY_SIZE(hmdi_pll_analog));
}

static void lt8912b_avi_info_frame(void)
{
	/* enable null package */
	lt8912b_write_byte(I2C_3RD, 0x3c, 0x41);

	/* defualt AVI */
	/* sync polarity + */
	lt8912b_write_byte(I2C_1ST, 0xab, 0x03);

	/* PB0:check sum */
	lt8912b_write_byte(I2C_3RD, 0x43, 0x27);
	/* PB1 */
	lt8912b_write_byte(I2C_3RD, 0x44, 0x10);
	/* PB2 */
	lt8912b_write_byte(I2C_3RD, 0x45, 0x28);
	/* PB3 */
	lt8912b_write_byte(I2C_3RD, 0x46, 0x00);
	/* PB4:vic */
	lt8912b_write_byte(I2C_3RD, 0x47, 0x10);

	/* 1080P60Hz 16:9 */
	/* sync polarity + */
	lt8912b_write_byte(I2C_1ST, 0xab, 0x03);

	/* PB0:check sum */
	lt8912b_write_byte(I2C_3RD, 0x43, 0x27);
	/* PB1 */
	lt8912b_write_byte(I2C_3RD, 0x44, 0x10);
	/* PB2 */
	lt8912b_write_byte(I2C_3RD, 0x45, 0x28);
	/* PB3 */
	lt8912b_write_byte(I2C_3RD, 0x46, 0x00);
	/* PB4:vic */
	lt8912b_write_byte(I2C_3RD, 0x47, 0x10);
}

static void lt8912b_mipi_analog(void)
{
#ifdef PN_SWAP
	/* P/N swap */
	lt8912b_write_byte(I2C_1ST, 0x3e, 0xf6);
#else
	/* if mipi pin map follow reference design, no need swap P/N. */
	lt8912b_write_byte(I2C_1ST, 0x3e, 0xd6);
#endif

	lt8912b_write_byte(I2C_1ST, 0x3f, 0xd4);
	lt8912b_write_byte(I2C_1ST, 0x41, 0x3c);
}

static void lt8912b_mipi_basic_set(void)
{
	/* 0: 4lane; 1: 1lane; 2: 2lane; 3: 3lane */
	unsigned char lane = 0;

	/* term en */
	lt8912b_write_byte(I2C_2ND, 0x10, 0x01);
	/* settle */
	lt8912b_write_byte(I2C_2ND, 0x11, 0x5);
	/* trail */
	/* lt8912b_write_byte(I2C_2ND, 0x12,0x08); */
	lt8912b_write_byte(I2C_2ND, 0x13, lane);
	/* debug mux */
	lt8912b_write_byte(I2C_2ND, 0x14, 0x00);

/* for EVB only, if mipi pin map follow reference design, no need swap lane */
#ifdef LANE_SWAP
	/* lane swap:3210 */
	lt8912b_write_byte(I2C_2ND, 0x15,0xa8);
	dev_info(g_lt->dev, "mipi basic set: lane swap 3210, %d\n", lane);
#else
	/* lane swap:0123 */
	lt8912b_write_byte(I2C_2ND, 0x15,0x00);
#endif
	/* hshift 3 */
	lt8912b_write_byte(I2C_2ND, 0x1a,0x03);
	/* vshift 3 */
	lt8912b_write_byte(I2C_2ND, 0x1b,0x03);
}

static void lt8912b_mipi_video_setup(struct video_timing *video_format)
{
	/* hwidth */
	lt8912b_write_byte(I2C_2ND, 0x18, (unsigned char)(video_format->hs % 256));
	/* vwidth 6 */
	lt8912b_write_byte(I2C_2ND, 0x19, (unsigned char)(video_format->vs % 256));
	/* H_active[7:0] */
	lt8912b_write_byte(I2C_2ND, 0x1c, (unsigned char)(video_format->hact % 256));
	/* H_active[15:8] */
	lt8912b_write_byte(I2C_2ND, 0x1d, (unsigned char)(video_format->hact / 256));
	/* fifo_buff_length 12 */
	lt8912b_write_byte(I2C_2ND, 0x2f, 0x0c);
	/* H_total[7:0] */
	lt8912b_write_byte(I2C_2ND, 0x34, (unsigned char)(video_format->htotal % 256));
	/* H_total[15:8] */
	lt8912b_write_byte(I2C_2ND, 0x35, (unsigned char)(video_format->htotal / 256));
	/* V_total[7:0] */
	lt8912b_write_byte(I2C_2ND, 0x36, (unsigned char)(video_format->vtotal % 256));
	/* V_total[15:8] */
	lt8912b_write_byte(I2C_2ND, 0x37, (unsigned char)(video_format->vtotal / 256));
	/* VBP[7:0] */
	lt8912b_write_byte(I2C_2ND, 0x38, (unsigned char)(video_format->vbp % 256));
	/* VBP[15:8] */
	lt8912b_write_byte(I2C_2ND, 0x39, (unsigned char)(video_format->vbp / 256));
	/* VFP[7:0] */
	lt8912b_write_byte(I2C_2ND, 0x3a, (unsigned char)(video_format->vfp % 256));
	/* VFP[15:8] */
	lt8912b_write_byte(I2C_2ND, 0x3b, (unsigned char)(video_format->vfp / 256));
	/* HBP[7:0] */
	lt8912b_write_byte(I2C_2ND, 0x3c, (unsigned char)(video_format->hbp % 256));
	/* HBP[15:8] */
	lt8912b_write_byte(I2C_2ND, 0x3d, (unsigned char)(video_format->hbp / 256));
	/* HFP[7:0] */
	lt8912b_write_byte(I2C_2ND, 0x3e, (unsigned char)(video_format->hfp % 256));
	/* HFP[15:8] */
	lt8912b_write_byte(I2C_2ND, 0x3f, (unsigned char)(video_format->hfp / 256));
}

static void lt8912b_mipi_rx_logic_resgister(void)
{
	/*mipi rx reset */
	lt8912b_write_byte(I2C_1ST, 0x03, 0x7f);
	usleep_range(10000, 20000);
	lt8912b_write_byte(I2C_1ST, 0x03, 0xff);

	/* dds reset */
	lt8912b_write_byte(I2C_1ST, 0x05, 0xfb);
	usleep_range(10000, 20000);
	lt8912b_write_byte(I2C_1ST, 0x05, 0xff);
}

static void lt8912b_dds_config(void)
{
	lt8912b_write_regs(I2C_2ND, dds_config, ARRAY_SIZE(dds_config));
}

static void lt8912b_audio_iis_enable(void)
{
	/* sampling 48K, sclk = 64*fs */
	lt8912b_write_byte(I2C_1ST, 0xB2, 0x01);
	lt8912b_write_byte(I2C_3RD, 0x06, 0x08);
	lt8912b_write_byte(I2C_3RD, 0x07, 0xF0);
	/* 0xE2:32FS; 0xD2:64FS */
	lt8912b_write_byte(I2C_3RD, 0x34, 0xD2);
}

static void lt8912b_lvds_power_up(void)
{
	lt8912b_write_byte(I2C_1ST, 0x44, 0x30);
	lt8912b_write_byte(I2C_1ST, 0x51, 0x05);
}

static void lt8912b_lvds_bypass(void)
{
	lt8912b_write_regs(I2C_1ST, lvds_bypass, ARRAY_SIZE(lvds_bypass));
}

static void lt8912b_lvds_output(int on)
{
	if(on) {
		lt8912b_write_regs(I2C_1ST, lvds_output_on, ARRAY_SIZE(lvds_output_on));
		// dev_info(g_lt->dev, "lt8912b lvds output enable\n");
	} else {
		lt8912b_write_byte(I2C_1ST, 0x44, 0x31);
	}
}

static void lt8912b_hdmi_output(int on)
{
	if(on) {
		/* enable hdmi output */
		lt8912b_write_byte(I2C_1ST, 0x33, 0x0e);
	} else {
		/* disable hdmi output */
		lt8912b_write_byte(I2C_1ST, 0x33, 0x0c);
	}
}

static void lt8912b_lvds_output_cfg(void)
{
	lt8912b_lvds_power_up();
	lt8912b_lvds_bypass();
}

static void lt8912b_mipi_input_det(void)
{
	static unsigned char Hsync_H_last = 0, Hsync_L_last = 0;
	static unsigned char Vsync_H_last = 0, Vsync_L_last = 0;
	static unsigned char Hsync_L, Hsync_H, Vsync_L, Vsync_H;

	Hsync_L = lt8912b_read_byte(I2C_1ST, 0x9c);
	Hsync_H = lt8912b_read_byte(I2C_1ST, 0x9d);
	Vsync_L = lt8912b_read_byte(I2C_1ST, 0x9e);
	Vsync_H = lt8912b_read_byte(I2C_1ST, 0x9f);

	/* high byte changed */
	if((Hsync_H != Hsync_H_last) || (Vsync_H != Vsync_H_last)) {
		dev_info(g_lt->dev, "0x9c~9f = %x, %x, %x, %x\n", Hsync_H, Hsync_L,Vsync_H, Vsync_L);
	}

	lt8912b_mipi_video_setup(&video_1920x1080_60Hz);
	dev_info(g_lt->dev, "videoformat = VESA_1920x1080_60\n");

	Hsync_L_last = Hsync_L;
	Hsync_H_last = Hsync_H;
	Vsync_L_last = Vsync_L;
	Vsync_H_last = Vsync_H;

	lt8912b_mipi_rx_logic_resgister();
}

static int lt8912b_get_hpd(void)
{
	if((lt8912b_read_byte(I2C_1ST, 0xc1) & 0x80) == 0x80)
		return 1;
	return 0;
}

static int lt8912b_kthread(void *data)
{
	struct lt8912 *lt = (struct lt8912 *)data;
	unsigned int val;
	int hpd_last = -1;
	int hpd_cur;
	
	gpiod_set_value_cansleep(lt->reset_gpiod, 0);
	msleep(200);
	gpiod_set_value_cansleep(lt->reset_gpiod, 1);
	
	regmap_read(lt->regmap[0], 0x00, &val);
	dev_info(lt->dev, "lt8912_id_00 = 0x%02x\n", val);
	regmap_read(lt->regmap[0], 0x01, &val);
	dev_info(lt->dev, "lt8912_id_01 = 0x%02x\n", val);
	
	g_lt = lt;
	
	lt8912b_digital_clock_enable();
	lt8912b_tx_analog();
	lt8912b_cbus_analog();
	lt8912b_hmdi_pll_analog();
	lt8912b_mipi_analog();
	lt8912b_mipi_basic_set();
	lt8912b_dds_config();
	lt8912b_mipi_input_det();
	lt8912b_audio_iis_enable();
	lt8912b_avi_info_frame();
	lt8912b_mipi_rx_logic_resgister();
	lt8912b_lvds_output_cfg();

	/* lvds output only */
	lt8912b_lvds_output(1);

	lt8912b_hdmi_output(1);
	
	while (1) {
		msleep(1000);
		hpd_cur = lt8912b_get_hpd();
		if (hpd_cur != hpd_last) {
			hpd_last = hpd_cur;
			dev_info(lt->dev, "hpd=%d\n", hpd_cur);
		}
	}

	return 0;
}

static int lt8912_hw_init(struct lt8912 *lt)
{
	lt->thd = kthread_create(lt8912b_kthread, lt, "lt8912b_kthread");
	if (IS_ERR(lt->thd)) {
		dev_err(lt->dev, "kthread_create lt8912b_kthread failed\n");
		return -1;
	}
	wake_up_process(lt->thd);
	return 0;
}
